"""RED-first S01 lifecycle/load identity and post-STOP evidence isolation.

Native LifecycleEvidence carries component-local id, pid, tid, but no load_id,
generation, or monotonic t_us. A chronological log marker alone cannot bind
a PlayerSession or AudioOutputWorker instance to the PCM load being analyzed.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze


def mark(stage, t, **kw):
    extras = " ".join(f"{key}={value}" for key, value in kw.items())
    return f"[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage={stage} t_us={t} {extras}"


def acquire(sequence, published, *, load=7, gen=3):
    return mark("PROCESSOR_ACQUIRE_RAW", published + 10,
                sequence=sequence, generation=gen, load_id=load,
                capture_publish_us=published)


def underflow(t, *, load=7, gen=3):
    return mark("SDL_QUEUE_UNDERFLOW", t, load_id=load, last_generation=gen,
                paused=0, core_idle=0, idle_active=0, seeking=0,
                pause_for_cache=0)


STOP_SESSION_A = "[Lifecycle] phase=STOP component=PlayerSession id=session-A pid=99 tid=5"
START_SESSION_B = "[Lifecycle] phase=START component=PlayerSession id=session-B pid=99 tid=5"
STOP_WORKER = "[Lifecycle] phase=STOP component=AudioOutputWorker id=0xAB12 pid=99 tid=16"


class LifecycleLoadIdentityOracle(unittest.TestCase):
    def test_prestop_underflow_cannot_consume_capture_publication_after_stop(self):
        trace = "\n".join([
            acquire(10, 1_000_000),
            underflow(1_600_000),
            STOP_SESSION_A,
            acquire(11, 2_100_000),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIsNone(r["selected_underflow_t_us"])
        self.assertFalse(r["playback_context_verified"])

    def test_poststop_stage_marker_with_earlier_clock_cannot_attribute_prestop_underflow(self):
        # Cross-thread log append can follow a monotonic-clock event from
        # another thread; same-load timestamps alone are not a safe join.
        trace = "\n".join([
            underflow(1_600_000),
            STOP_WORKER,
            mark("PIPE_DISCONNECTED", 1_550_000, load_id=7),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIsNone(r["selected_underflow_t_us"])

    def test_old_and_new_session_ids_do_not_prove_load_binding(self):
        trace = "\n".join([
            STOP_SESSION_A, START_SESSION_B,
            acquire(10, 3_000_000), underflow(3_600_000),
            acquire(11, 4_100_000),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["lifecycle_identity_binding"], "UNVERIFIED")
        self.assertGreaterEqual(r["lifecycle_stop_count"], 1)

    def test_complete_prestop_publications_still_correlate(self):
        trace = "\n".join([
            acquire(10, 1_000_000), underflow(1_600_000),
            acquire(11, 2_100_000), STOP_WORKER,
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertTrue(r["playback_context_verified"])

    def test_unrelated_stop_without_load_binding_never_certifies_next_load(self):
        trace = "\n".join([
            STOP_WORKER,
            acquire(10, 3_000_000, load=99),
            underflow(3_600_000, load=99),
            acquire(11, 4_100_000, load=99),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["lifecycle_identity_binding"], "UNVERIFIED")


if __name__ == "__main__":
    unittest.main()
