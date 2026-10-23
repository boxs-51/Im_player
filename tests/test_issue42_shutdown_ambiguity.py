"""S01 shutdown/unknown-playback-context RED-first oracles.

The true media fault remains UNVERIFIED. These synthetic tests protect trace
interpretation from a teardown underflow and incomplete playback-state fields.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from issue42_first_boundary import analyze


def marker(stage, time, **kwargs):
    values = " ".join(f"{name}={value}" for name, value in kwargs.items())
    return f"[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage={stage} t_us={time} {values}"


def acquire(seq, published, *, load=7):
    return marker("PROCESSOR_ACQUIRE_RAW", published + 10,
                  load_id=load, generation=3, sequence=seq,
                  capture_publish_us=published)


def underflow(time, *, load=7, complete=False):
    context = (dict(paused=0, core_idle=0, idle_active=0,
                    seeking=0, pause_for_cache=0) if complete else {})
    return marker("SDL_QUEUE_UNDERFLOW", time,
                  load_id=load, last_generation=3, **context)


STOP = "[Lifecycle] phase=STOP component=PlayerSession id=session01 pid=99 tid=5"


class ShutdownAmbiguityOracle(unittest.TestCase):
    def test_stop_before_underflow_prevents_false_capture_attribution(self):
        trace = "\n".join([
            acquire(1, 1_000_000), STOP,
            underflow(1_600_000, complete=True), acquire(2, 2_100_000),
        ])
        result = analyze(trace)
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertIsNone(result["selected_underflow_t_us"])
        self.assertIn("shutdown_lifecycle_stop_observed", result["evidence"])

    def test_pre_shutdown_valid_event_wins_over_later_teardown_event(self):
        trace = "\n".join([
            acquire(1, 1_000_000), underflow(1_600_000, complete=True),
            acquire(2, 2_100_000), STOP,
            acquire(3, 4_000_000), underflow(4_600_000, complete=True),
            acquire(4, 5_100_000),
        ])
        result = analyze(trace)
        self.assertEqual(result["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(result["selected_underflow_t_us"], 1_600_000)
        self.assertTrue(result["playback_context_verified"])
        self.assertEqual(result["underflow_count"], 2)

    def test_missing_state_fields_cannot_verify_mid_playback(self):
        trace = "\n".join([acquire(1, 1_000_000),
                            underflow(1_600_000), acquire(2, 2_100_000)])
        result = analyze(trace)
        self.assertEqual(result["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertFalse(result["playback_context_verified"])
        self.assertEqual(result["playback_context"], "UNKNOWN")
        self.assertFalse(result["audible_output_verified"])

    def test_later_complete_active_candidate_preferred_over_weak_earlier(self):
        trace = "\n".join([
            acquire(1, 1_000_000), underflow(1_600_000),
            acquire(2, 2_100_000),
            acquire(3, 5_000_000), underflow(5_600_000, complete=True),
            acquire(4, 6_100_000),
        ])
        r = analyze(trace)
        self.assertEqual(r["selected_underflow_t_us"], 5_600_000)
        self.assertTrue(r["playback_context_verified"])
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")

    def test_stop_after_underflow_does_not_retroactively_block(self):
        trace = "\n".join([acquire(1, 1_000_000),
                           underflow(1_600_000, complete=True),
                           acquire(2, 2_100_000), STOP])
        r = analyze(trace)
        self.assertEqual(r["selected_underflow_t_us"], 1_600_000)
        self.assertTrue(r["playback_context_verified"])

    def test_other_component_stop_does_not_block_audio(self):
        non_audio = "[Lifecycle] phase=STOP component=PlayBackRenderThread id=42 pid=99 tid=5"
        trace = "\n".join([acquire(1, 1_000_000), non_audio,
                           underflow(1_600_000, complete=True),
                           acquire(2, 2_100_000)])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertTrue(r["playback_context_verified"])

    def test_incomplete_state_cannot_pass_cli_reproduction_gate(self):
        trace = "\n".join([acquire(1, 1_000_000), underflow(1_600_000),
                           acquire(2, 2_100_000)])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "trace.log"
            path.write_text(trace, encoding="utf-8")
            p = subprocess.run(
                [sys.executable, str(ROOT / "scripts" / "issue42_first_boundary.py"),
                 str(path), "--require-reproduction"],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(p.returncode, 2, p.stdout + p.stderr)


if __name__ == "__main__":
    unittest.main()
