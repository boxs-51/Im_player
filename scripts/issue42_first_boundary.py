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
    for line in raw.splitlines():
        if "[AUDIO-TELEMETRY] " not in line:
            continue
        fields = dict(re.findall(r"\b([A-Za-z_][\w]*)=([^\s]+)",
                                 line.split("[AUDIO-TELEMETRY] ", 1)[1]))
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
              "selected_underflow_t_us": None, "examined_underflow_count": 0}
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
                     "evidence": []}
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

        generation = integer(u, "generation")
        if generation is None:
            generation = integer(u, "last_generation")
        scoped = [e for e in events if integer(e, "load_id") == load]
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

    # The first SDL underflow can be a startup/paused/teardown event. Do not
    # discard later events just because an earlier one lacks valid evidence.
    # Select the earliest *correlated* observation in timestamp order.
    for underflow in underflows:
        candidate = inspect_underflow(underflow)
        result["examined_underflow_count"] += 1
        if candidate["classification"] != "INCONCLUSIVE":
            result.update(candidate)
            result["selected_underflow_t_us"] = underflow["t_us"]
            return result
        for reason in candidate["evidence"]:
            if reason not in result["evidence"]:
                result["evidence"].append(reason)
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
    return 2 if args.require_reproduction and result["classification"] in ("INCONCLUSIVE", "NO_UNDERFLOW_OBSERVED") else 0


if __name__ == "__main__":
    raise SystemExit(main())
