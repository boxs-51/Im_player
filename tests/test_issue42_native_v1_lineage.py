"""RED-first v1/native lineage: session and process IDs must scope PCM events.

V1's historical unscoped trace remains supported, but a new native trace
with run_uid/session_uid MUST NOT borrow capture or drop markers belonging
to a different session incarnation or process run merely because load_id,
generation, epoch and timestamps overlap. Synthetic only, not physical audio.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze


def event(stage, timestamp, **fields):
    suffix = " ".join(f"{k}={v}" for k, v in fields.items())
    return (f"[AUDIO-TELEMETRY] STARTUP_BOUNDARY "
            f"stage={stage} t_us={timestamp} {suffix}")


def underflow(run=901, session=11, **extra):
    return event(
        "SDL_QUEUE_UNDERFLOW", 1_600_000,
        load_id=1, last_generation=3, run_uid=run, session_uid=session,
        paused=0, core_idle=0, idle_active=0, seeking=0,
        pause_for_cache=0, **extra)


def acquire(seq, pub, *, run=901, session=11, legacy=False):
    fields = dict(load_id=1, generation=3, sequence=seq,
                  capture_publish_us=pub)
    if not legacy:
        fields.update(run_uid=run, session_uid=session)
    return event("PROCESSOR_ACQUIRE_RAW", pub + 100, **fields)


def parse(*records):
    return analyze("\n".join(records))


class NativeV1LineageGuard(unittest.TestCase):
    def test_other_session_same_load_generation_cannot_mint_gap(self):
        r = parse(acquire(1, 1_000_000, session=12),
                  underflow(session=11),
                  acquire(2, 2_100_000, session=12))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertFalse(r["audible_output_verified"])

    def test_other_process_run_same_session_load_cannot_mint_gap(self):
        r = parse(acquire(1, 1_000_000, run=902),
                  underflow(run=901),
                  acquire(2, 2_100_000, run=902))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIsNone(r["selected_underflow_t_us"])

    def test_partial_native_lineage_cannot_borrow_old_unscoped_marker(self):
        r = parse(acquire(1, 1_000_000, legacy=True),
                  underflow(),
                  acquire(2, 2_100_000, legacy=True))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_mixed_sessions_one_acquisition_each_cannot_create_gap(self):
        r = parse(acquire(1, 1_000_000, session=12),
                  underflow(session=11),
                  acquire(2, 2_100_000, session=11))
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_same_native_lineage_remains_structural_not_audible_proof(self):
        r = parse(acquire(1, 1_000_000), underflow(),
                  acquire(2, 2_100_000))
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertTrue(r["playback_context_verified"])
        self.assertFalse(r["audible_output_verified"])
        self.assertEqual(r["root_cause"], "UNKNOWN")

    def test_legacy_all_unscoped_trace_remains_backward_compatible(self):
        u = event("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=1,
                  last_generation=3, paused=0, core_idle=0,
                  idle_active=0, seeking=0, pause_for_cache=0)
        r = parse(acquire(1, 1_000_000, legacy=True), u,
                  acquire(2, 2_100_000, legacy=True))
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertFalse(r["audible_output_verified"])


if __name__ == "__main__":
    unittest.main()
