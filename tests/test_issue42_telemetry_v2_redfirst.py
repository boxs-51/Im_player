"""DESIGN-ONLY T1 tests for the proposed telemetry v2 analyzer.

Expected baseline: SEMANTIC RED on scripts.issue42_first_boundary.analyze at PR#62
HEAD 6cddc8c: it does not understand JSONL v2; this is not a claim tests ran.
Freeze the assertions before any implementation. Place in repo/tests/ only with
separate implementation authorization; run on GitHub Ubuntu/Windows.
"""
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze

SCHEMA = "IM_PLAYER_AUDIO_V2"


def e(seq, *, event_type, run="run-01", session="session-inc-a", load=1,
      gen=0, t=1_000_000, phase="ACTIVE", binding="BOUND", label="window-a",
      component="AudioCaptureManager", retry=0, **extra):
    emitter = f"emitter-{component.lower()}"
    data = {
        "schema": SCHEMA, "event_id": f"{run}:{session}:{emitter}:{seq}",
        "run_uid": run, "session_uid": session, "session_label": label,
        "load_id": load, "retry_attempt": retry, "generation": gen,
        "event_type": event_type, "component": component,
        "emitter_id": emitter, "emitter_seq": seq, "event_mono_us": t,
        "clock_domain": "process_steady_v1", "lifecycle_epoch": 1,
        "playback_phase": phase, "correlation_status": binding,
    }
    data.update(extra)
    return data


def lines(*events):
    return "\n".join(json.dumps(event, sort_keys=True) for event in events)


def lifecycle(run="run-01", session="session-inc-a", load=1, gen=0,
              label="window-a"):
    """Separate owner receipts: no inference from human-readable session labels."""
    run_start = e(1, event_type="RUN_START", run=run, session=None,
                  load=None, gen=None, t=50, component="TelemetryOwner",
                  binding="UNBOUND")
    session_start = e(1, event_type="SESSION_START", run=run,
                      session=session, load=load, gen=gen, t=100,
                      component="PlayerSession", label=label,
                      active_lease="lease-1")
    load_start = e(1, event_type="LOAD_STARTED", run=run,
                   session=session, load=load, gen=gen, t=200,
                   component="PlaybackCommand", label=label,
                   active_lease="lease-1")
    generation_start = e(1, event_type="GENERATION_STARTED", run=run,
                         session=session, load=load, gen=gen, t=300,
                         component="AudioCaptureManager", label=label,
                         active_lease="lease-1")
    return run_start, session_start, load_start, generation_start


def trace(*events):
    return lines(*lifecycle(), *events)


def rebind(event, *, session=None, run=None):
    """Keep event identity coherent when constructing a foreign-session fixture."""
    item = dict(event)
    if session is not None:
        item["session_uid"] = session
    if run is not None:
        item["run_uid"] = run
    item["event_id"] = (f'{item["run_uid"]}:{item["session_uid"]}:'
                        f'{item["emitter_id"]}:{item["emitter_seq"]}')
    return item


def gap(session="session-inc-a", load=1, gen=0, run="run-01"):
    a = e(1, event_type="PROCESSOR_ACQUIRE_RAW", session=session, load=load,
          run=run, gen=gen, t=1_000_100, component="AudioProcessor",
          block_sequence=11, capture_publish_us=1_000_000)
    u = e(1, event_type="SDL_QUEUE_UNDERFLOW", session=session, load=load,
          run=run, gen=gen, t=1_600_000, component="AudioOutputWorker",
          source="segment", paused=0, core_idle=0, idle_active=0,
          seeking=0, pause_for_cache=0, active_lease="lease-1")
    b = e(2, event_type="PROCESSOR_ACQUIRE_RAW", session=session, load=load,
          run=run, gen=gen, t=2_100_100, component="AudioProcessor",
          block_sequence=12, capture_publish_us=2_100_000)
    return a, u, b


class CorrelationV2Contract(unittest.TestCase):
    """Outputs are proposed additive analyzer contract (NOT current v1 behavior)."""

    def test_R15_valid_bound_active_gap_is_observation_not_cause(self):
        r = analyze(trace(*gap()))
        self.assertEqual(r["schema"], "ISSUE42-S01-OBSERVATION-v2")
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["binding_status"], "VERIFIED")
        self.assertEqual(r["root_cause"], "UNKNOWN")
        self.assertFalse(r["audible_output_verified"])

    def test_R01_R03_interleaved_sessions_same_load_gen_never_join(self):
        a, u, b = gap()
        a = rebind(a, session="session-inc-b")
        b = rebind(b, session="session-inc-b")
        # Both owners genuinely exist; the only illegal operation is crossing
        # the session key to construct a false single-session capture gap.
        other_owner = lifecycle(session="session-inc-b")
        r = analyze(lines(*lifecycle(), *other_owner[1:], a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertNotEqual(r.get("binding_status"), "VERIFIED")

    def test_R02_same_label_after_recreate_is_new_incarnation(self):
        a, u, b = gap()
        a = rebind(a, session="old-session-incarnation")
        b = rebind(b, session="old-session-incarnation")
        old_owner = lifecycle(session="old-session-incarnation", label="window-a")
        # u remains a new incarnation with the SAME human-readable label.
        r = analyze(lines(*old_owner, *lifecycle()[1:], a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R04_R16_seek_generation_boundary_even_if_sequence_reused(self):
        a, u, b = gap()
        a["generation"] = 0
        b["generation"] = 1
        a["block_sequence"] = b["block_sequence"] = 7
        r = analyze(trace(a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R05_retry_attempt_is_not_a_new_load_identity(self):
        a, u, b = gap()
        for item in (a, u, b):
            item["retry_attempt"] = 2
        retry_1 = e(2, event_type="LOAD_RETRY", t=400,
                    component="PlaybackCommand", retry=1)
        retry_2 = e(3, event_type="LOAD_RETRY", t=500,
                    component="PlaybackCommand", retry=2)
        r = analyze(trace(retry_1, retry_2, a, u, b))
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["binding_status"], "VERIFIED")

    def test_R06_stop_overrides_all_five_zero_flags(self):
        a, u, b = gap()
        u["playback_phase"] = "STOPPING"
        r = analyze(trace(a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertFalse(r["audible_output_verified"])

    def test_R07_buffered_event_after_stop_requires_lease_proof(self):
        a, u, b = gap()
        u["active_lease"] = None
        stop = e(2, event_type="SESSION_STOP", t=1_500_000,
                 component="PlayerSession", phase="STOPPING", fence_seq=42)
        # File append order is deliberately not chronological.
        r = analyze(trace(a, stop, b, u))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R08_cross_clock_domain_cannot_bound_gap(self):
        a, u, b = gap()
        b["clock_domain"] = "sdl_ticks_ms"
        r = analyze(trace(a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R09_raw_marker_unbound_does_not_mint_load_id(self):
        raw = e(1, event_type="SDL_QUEUE_UNDERFLOW", session=None,
                load=None, gen=None, binding="UNBOUND",
                component="AudioOutputWorker", t=1_600_000)
        r = analyze(trace(raw))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertNotEqual(r.get("binding_status"), "VERIFIED")

    def test_R10_unbound_pipe_cannot_be_promoted_into_causal_load(self):
        _, u, _ = gap()
        pipe = e(2, event_type="PIPE_DISCONNECTED", session=None,
                 load=None, gen=None, binding="UNBOUND", t=1_500_000)
        r = analyze(trace(pipe, u))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R11_duplicate_id_with_conflicting_payload_fails_closed(self):
        a, u, b = gap()
        conflict = dict(a, capture_publish_us=999_999)
        r = analyze(trace(a, conflict, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIn("CONFLICTING_EVENT_ID", r["reason_codes"])

    def test_R12_dropped_telemetry_cannot_prove_gap(self):
        a, u, b = gap()
        loss = e(1, event_type="TELEMETRY_LOSS", t=1_550_000,
                 lost_event_count=3, component="TelemetrySink")
        r = analyze(trace(a, u, loss, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIn("LOST_TELEMETRY", r["reason_codes"])

    def test_R13_equal_timestamps_and_file_order_do_not_create_causality(self):
        a, u, b = gap()
        a["event_mono_us"] = u["event_mono_us"] = b["event_mono_us"] = 1_600_000
        r = analyze(trace(b, u, a))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R08_cross_process_run_uid_must_not_join(self):
        a, u, b = gap()
        b = rebind(b, run="run-02")
        other_run = lifecycle(run="run-02")
        r = analyze(lines(*lifecycle(), *other_run, a, u, b))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_R14_legacy_v1_remains_supported(self):
        trace = (
            "[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage=SDL_QUEUE_UNDERFLOW "
            "t_us=1600000 load_id=1 generation=0 paused=1"
        )
        r = analyze(trace)
        self.assertEqual(r["schema"], "ISSUE42-S01-OBSERVATION-v1")
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["root_cause"], "UNKNOWN")


if __name__ == "__main__":
    unittest.main()