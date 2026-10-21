"""Fail-closed S01 typed telemetry-v2 correlation (synthetic verifier only).

This parser checks relationships within producer-authored evidence; it cannot
authenticate that a native producer actually minted any event. The Windows
physical and native lineage gates remain separate and mandatory.
"""
import json
from collections import defaultdict

V2_SCHEMA = "IM_PLAYER_AUDIO_V2"
RESULT_SCHEMA = "ISSUE42-S01-OBSERVATION-v2"
DOMAIN = "process_steady_v1"
UNDERFLOW = "SDL_QUEUE_UNDERFLOW"
ACQUIRE = "PROCESSOR_ACQUIRE_RAW"
INACTIVE = ("paused", "core_idle", "idle_active", "seeking", "pause_for_cache")
OWNER = ("RUN_START", "SESSION_START", "LOAD_STARTED", "GENERATION_STARTED")
STOP = ("SESSION_STOP", "SESSION_DESTROY", "LOAD_ENDED", "GENERATION_ENDED")
UNCORRELATED = ("PIPE_DISCONNECTED", "OUTPUT_DEVICE_OFFLINE_DROP")
REQUIRED = (
    "schema", "event_id", "run_uid", "session_uid", "load_id",
    "retry_attempt", "generation", "event_type", "component",
    "emitter_id", "emitter_seq", "event_mono_us", "clock_domain",
    "lifecycle_epoch", "playback_phase", "correlation_status",
)


def _int(value):
    # Avoid treating a boolean or a string that happens to contain digits as
    # independently trusted native integer evidence.
    return value if type(value) is int and value >= 0 else None


def _tuple(e):
    return (e.get("run_uid"), e.get("session_uid"),
            e.get("load_id"), e.get("generation"))


def _new_result(events):
    underflows = [e for e in events if e.get("event_type") == UNDERFLOW]
    return {
        "schema": RESULT_SCHEMA,
        "classification": "INCONCLUSIVE" if underflows else "NO_UNDERFLOW_OBSERVED",
        "root_cause": "UNKNOWN",
        "audible_output_verified": False,
        "binding_status": "UNVERIFIED",
        # The JSONL input is unauthenticated synthetic/producer-authored data.
        # Consistent evidence fields are not an independent OS/host receipt.
        "producer_attestation": "UNVERIFIED",
        "selected_event_id": None,
        "reason_codes": [],
        "underflow_count": len(underflows),
        "observed_marker_count": len(events),
        "raw_underflow_marker_count": sum(
            1 for e in underflows if e.get("correlation_status") != "BOUND"
        ),
        "lost_event_count": sum(
            _int(e.get("lost_event_count")) or 0 for e in events
            if e.get("event_type") == "TELEMETRY_LOSS"
        ),
        "unscoped_marker_counts": {
            stage: sum(1 for e in events
                       if e.get("event_type") == stage and
                       e.get("correlation_status") != "BOUND")
            for stage in UNCORRELATED
        },
    }


def analyze_v2(raw, threshold_us=250000):
    if threshold_us <= 0:
        raise ValueError("threshold must be positive")
    events = []
    bad_format = False
    for line in raw.splitlines():
        if not line.strip():
            continue
        try:
            e = json.loads(line)
        except (json.JSONDecodeError, TypeError, ValueError):
            bad_format = True
            continue
        if not isinstance(e, dict) or e.get("schema") != V2_SCHEMA:
            bad_format = True
            continue
        events.append(e)

    result = _new_result(events)
    reasons = set()

    if bad_format or any(any(k not in e for k in REQUIRED) for e in events):
        reasons.add("INVALID_V2_EVENT")

    seen_ids = {}
    emitters = defaultdict(set)
    for e in events:
        eid = e.get("event_id")
        if not isinstance(eid, str) or not eid:
            reasons.add("INVALID_V2_EVENT")
        else:
            canonical = json.dumps(e, sort_keys=True, separators=(",", ":"))
            if eid in seen_ids and seen_ids[eid] != canonical:
                reasons.add("CONFLICTING_EVENT_ID")
            seen_ids[eid] = canonical
        seq = _int(e.get("emitter_seq"))
        ts = _int(e.get("event_mono_us"))
        if seq is None or seq < 1 or ts is None:
            reasons.add("INVALID_V2_EVENT")
        if e.get("clock_domain") != DOMAIN:
            # A mixed domain cannot be a source of positive correlation.
            reasons.add("UNKNOWN_CLOCK_DOMAIN")
        if seq is not None:
            group = (e.get("run_uid"), e.get("session_uid"), e.get("emitter_id"))
            emitters[group].add(seq)

    for seqs in emitters.values():
        if seqs and max(seqs) - min(seqs) + 1 != len(seqs):
            reasons.add("LOST_TELEMETRY")
    if result["lost_event_count"] or any(
        e.get("event_type") == "TELEMETRY_LOSS" for e in events
    ):
        reasons.add("LOST_TELEMETRY")

    # An evidence loss, conflicting ID, malformed record, or unknown clock
    # invalidates a positive observation on this trace (not only one marker).
    if reasons:
        result["classification"] = "INCONCLUSIVE"
        result["reason_codes"] = sorted(reasons)
        return result

    runs = {e.get("run_uid") for e in events if e.get("event_type") == "RUN_START"}
    sessions = {
        (e.get("run_uid"), e.get("session_uid")): e
        for e in events if e.get("event_type") == "SESSION_START"
    }
    loads = {
        (e.get("run_uid"), e.get("session_uid"), e.get("load_id")): e
        for e in events if e.get("event_type") == "LOAD_STARTED"
    }
    gens = {
        _tuple(e): e for e in events
        if e.get("event_type") == "GENERATION_STARTED"
    }
    retries = defaultdict(set)
    for e in events:
        if e.get("event_type") == "LOAD_RETRY":
            retries[(e.get("run_uid"), e.get("session_uid"),
                     e.get("load_id"))].add(_int(e.get("retry_attempt")))
    stops = [e for e in events if e.get("event_type") in STOP]
    underflows = sorted(
        (e for e in events if e.get("event_type") == UNDERFLOW),
        key=lambda e: (_int(e.get("event_mono_us")) or 0, str(e.get("event_id")))
    )
    acquisitions = [e for e in events if e.get("event_type") == ACQUIRE]
    if not underflows:
        return result

    for u in underflows:
        run, sess, load, gen = _tuple(u)
        t = _int(u.get("event_mono_us"))
        attempt = _int(u.get("retry_attempt"))
        epoch = _int(u.get("lifecycle_epoch"))
        lease = u.get("active_lease")
        if (not all(isinstance(v, str) and v for v in (run, sess)) or
                load is None or gen is None or
                any(type(v) is not int or v < 0 for v in (load, gen)) or
                t is None or epoch is None or attempt is None or
                u.get("correlation_status") != "BOUND" or
                u.get("playback_phase") != "ACTIVE" or
                not isinstance(lease, str) or not lease or
                any(_int(u.get(flag)) != 0 for flag in INACTIVE)):
            reasons.add("UNDERFLOW_UNVERIFIED")
            continue
        owner_sess = sessions.get((run, sess))
        owner_load = loads.get((run, sess, load))
        owner_gen = gens.get((run, sess, load, gen))
        if (run not in runs or
                owner_sess is None or owner_load is None or owner_gen is None or
                any(o.get("active_lease") != lease for o in
                    (owner_sess, owner_load, owner_gen)) or
                any(o.get("lifecycle_epoch") != epoch for o in
                    (owner_sess, owner_load, owner_gen)) or
                not all(_int(o.get("event_mono_us")) is not None and
                        _int(o.get("event_mono_us")) < t for o in
                        (owner_sess, owner_load, owner_gen))):
            reasons.add("IDENTITY_UNVERIFIED")
            continue
        if attempt > 0 and not set(range(1, attempt + 1)).issubset(
            retries.get((run, sess, load), set())
        ):
            reasons.add("RETRY_UNVERIFIED")
            continue
        # Never use text-file append order to prove a STOP timing/fence.
        if any(s.get("run_uid") == run and s.get("session_uid") == sess and
               s.get("lifecycle_epoch") == epoch and
               (_int(s.get("event_mono_us")) is None or
                _int(s.get("event_mono_us")) <= t)
               for s in stops):
            reasons.add("LIFECYCLE_STOP_FENCE")
            continue
        bound = []
        for a in acquisitions:
            if (_tuple(a) != (run, sess, load, gen) or
                    a.get("clock_domain") != DOMAIN or
                    a.get("correlation_status") != "BOUND" or
                    a.get("lifecycle_epoch") != epoch or
                    _int(a.get("retry_attempt")) != attempt):
                continue
            at, pub, seq = (_int(a.get(k)) for k in
                            ("event_mono_us", "capture_publish_us", "block_sequence"))
            if (at is None or pub is None or seq is None or
                    abs(at - t) > 2_000_000 or pub > at):
                continue
            bound.append((pub, at, seq))
        bound.sort()
        matched = False
        for left, right in zip(bound, bound[1:]):
            if (left[0] < t < right[0] and
                    left[1] < t < right[1] and
                    right[0] - left[0] >= threshold_us and
                    left[2] != right[2]):
                matched = True
                break
        if matched:
            result["classification"] = "CAPTURE_PUBLICATION_GAP_OBSERVED"
            result["binding_status"] = "VERIFIED"
            result["selected_event_id"] = u.get("event_id")
            result["reason_codes"] = []
            return result
        reasons.add("NO_CORRELATED_BOUNDARY")

    result["reason_codes"] = sorted(reasons)
    return result
