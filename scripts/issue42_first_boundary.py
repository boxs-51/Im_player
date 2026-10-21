#!/usr/bin/env python3
"""S01 #42: classify observed PCM boundaries, never claim an audio root cause."""
import argparse
import json
import re
from pathlib import Path

STAGES = {"PROCESSOR_ACQUIRE_RAW", "PROCESSOR_PUBLISH_DROP", "SDL_QUEUE_UNDERFLOW",
          "PIPE_DISCONNECTED", "OUTPUT_DEVICE_OFFLINE_DROP"}


def analyze(raw: str, threshold_us: int = 250000) -> dict:
    if threshold_us <= 0:
        raise ValueError("threshold must be positive")

    events = []
    lifecycle_stop_seen = False
    lifecycle_stop_count = 0
    lifecycle_epoch = 0
    lifecycle_components = {"PlayerSession", "AudioOutputWorker",
                            "AudioProcessor", "AudioCaptureManager"}
    for line in raw.splitlines():
        # Existing native LifecycleEvidence emits these records into the same
        # append-only trace file. It supplies no t_us or load_id, so a STOP
        # can only impose a conservative log-order exclusion, not prove which
        # player/load ended. Never infer a physical mid-playback event after it.
        if line.startswith("[Lifecycle] "):
            lifecycle = dict(re.findall(r"\b([A-Za-z_][\w]*)=([^\s]+)", line))
            if (lifecycle.get("phase") == "STOP" and
                    lifecycle.get("component") in lifecycle_components):
                lifecycle_stop_seen = True
                lifecycle_stop_count += 1
                lifecycle_epoch += 1
            continue
        if "[AUDIO-TELEMETRY] " not in line:
            continue
        fields = dict(re.findall(r"\b([A-Za-z_][\w]*)=([^\s]+)",
                                 line.split("[AUDIO-TELEMETRY] ", 1)[1]))
        fields["_after_lifecycle_stop"] = lifecycle_stop_seen
        fields["_lifecycle_epoch"] = lifecycle_epoch
        try:
            fields["t_us"] = int(fields["t_us"])
        except (KeyError, ValueError):
            continue
        if fields.get("stage") in STAGES and fields["t_us"] >= 0:
            events.append(fields)

    underflows = sorted((e for e in events if e["stage"] == "SDL_QUEUE_UNDERFLOW"),
                        key=lambda e: e["t_us"])
    result = {"schema": "ISSUE42-S01-OBSERVATION-v1",
              "classification": "INCONCLUSIVE" if underflows else "NO_UNDERFLOW_OBSERVED",
              "root_cause": "UNKNOWN", "audible_output_verified": False,
              "underflow_count": len(underflows), "observed_marker_count": len(events),
              "capture_gap_us": None, "evidence": [],
              "selected_underflow_t_us": None, "examined_underflow_count": 0,
              "playback_context_verified": False,
              "playback_context": "UNKNOWN",
              "lifecycle_stop_count": lifecycle_stop_count,
              # LifecycleEvidence.id is a component-local string/pointer;
              # there is no authenticated mapping to numeric PCM load_id.
              "lifecycle_identity_binding": (
                  "UNVERIFIED" if lifecycle_stop_count else "NO_STOP_OBSERVED"
              )}
    if not underflows:
        return result

    def integer(e, key):
        try:
            return int(e[key])
        except (KeyError, ValueError, TypeError):
            return None

    def inspect_underflow(u):
        """One event's observed markers; cannot prove audible output or cause."""
        candidate = {"classification": "INCONCLUSIVE", "capture_gap_us": None,
                     "evidence": [], "playback_context_verified": False,
                     "playback_context": "UNKNOWN"}
        if u.get("_after_lifecycle_stop"):
            candidate["evidence"].append("shutdown_lifecycle_stop_observed")
            return candidate
        t = u["t_us"]
        load = integer(u, "load_id")
        if load is None or load <= 0:
            candidate["evidence"].append("underflow_missing_load_identity")
            return candidate
        inactive_flags = ("paused", "core_idle", "idle_active", "seeking",
                          "pause_for_cache")
        if any(integer(u, flag) == 1 for flag in inactive_flags):
            candidate["evidence"].append("underflow_nonplaying_state")
            return candidate
        # Missing or non-boolean fields are not affirmative evidence that
        # playback was active. Preserve observed marker association, but never
        # allow such a candidate to satisfy the physical-reproduction gate.
        if all(integer(u, flag) == 0 for flag in inactive_flags):
            candidate["playback_context_verified"] = True
            candidate["playback_context"] = "ACTIVE_FLAGS_OBSERVED"

        generation = integer(u, "generation")
        if generation is None:
            generation = integer(u, "last_generation")
        # Never bridge a lifecycle STOP using timestamp/sequence alone:
        # native lifecycle records lack monotonic timestamp and load_id.
        # Log-order segmentation is conservative, not proof of OS ordering.
        scoped = [e for e in events
                  if integer(e, "load_id") == load
                  and e.get("_lifecycle_epoch") == u.get("_lifecycle_epoch")]
        before = [e for e in scoped if t - 2_000_000 <= e["t_us"] <= t]
        for stage, classification in (
            ("PIPE_DISCONNECTED", "PIPE_DISCONNECT_OBSERVED"),
            ("OUTPUT_DEVICE_OFFLINE_DROP", "OUTPUT_DEVICE_OFFLINE_DROP_OBSERVED"),
            ("PROCESSOR_PUBLISH_DROP", "PROCESSOR_DROP_OBSERVED")
        ):
            matches = [e for e in before if e["stage"] == stage]
            if stage == "PROCESSOR_PUBLISH_DROP":
                matches = [e for e in matches if generation is not None
                           and integer(e, "generation") == generation]
            if matches:
                candidate["classification"] = classification
                candidate["evidence"].append(stage)
                return candidate

        if generation is None:
            candidate["evidence"].append("underflow_missing_generation_identity")
            return candidate
        acquisitions = {}
        for e in scoped:
            if (e["stage"] != "PROCESSOR_ACQUIRE_RAW" or
                    not t - 2_000_000 <= e["t_us"] <= t + 2_000_000):
                continue
            gen, seq, pub = (integer(e, k) for k in
                             ("generation", "sequence", "capture_publish_us"))
            if None in (gen, seq, pub) or gen != generation:
                continue
            acquisitions[seq] = pub

        ordered = sorted(acquisitions.values())
        gaps = [right - left for left, right in zip(ordered, ordered[1:])
                if left <= t <= right and right - left >= threshold_us]
        if gaps:
            candidate.update(classification="CAPTURE_PUBLICATION_GAP_OBSERVED",
                             capture_gap_us=max(gaps), generation=generation)
            candidate["evidence"].append("same_load_generation_gap_brackets_underflow")
        else:
            candidate["evidence"].append("no_correlated_boundary_marker")
        return candidate

    # Sort by event clock; after native lifecycle STOP, the record's phase
    # cannot be attributed to active playback using this instrumentation.
    # If there is a fully-observed active candidate, prefer its earliest
    # event to an earlier candidate with incomplete state. For legacy traces
    # keep the earliest observed association, explicitly marked unverified.
    weak_candidate = None
    for underflow in underflows:
        candidate = inspect_underflow(underflow)
        result["examined_underflow_count"] += 1
        if candidate["classification"] != "INCONCLUSIVE":
            if candidate["playback_context_verified"]:
                result.update(candidate)
                result["selected_underflow_t_us"] = underflow["t_us"]
                return result
            if weak_candidate is None:
                weak_candidate = (underflow["t_us"], candidate)
        else:
            for reason in candidate["evidence"]:
                if reason not in result["evidence"]:
                    result["evidence"].append(reason)
    if weak_candidate is not None:
        t, candidate = weak_candidate
        result.update(candidate)
        result["selected_underflow_t_us"] = t
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--require-reproduction", action="store_true")
    args = parser.parse_args()
    result = analyze(args.trace.read_text(encoding="utf-8", errors="replace"))
    content = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
    print(content)
    if args.require_reproduction and (
        result["classification"] in ("INCONCLUSIVE", "NO_UNDERFLOW_OBSERVED")
        or not result["playback_context_verified"]
    ):
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
