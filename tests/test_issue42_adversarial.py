"""S01 adversarial evidence tests. Synthetic oracle, NOT physical playback."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from issue42_first_boundary import analyze


def event(stage, t_us, **items):
    parts = " ".join(f"{key}={value}" for key, value in items.items())
    return f"[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage={stage} t_us={t_us} {parts}"


def capture(seq, t_us, generation=3):
    return event("PROCESSOR_ACQUIRE_RAW", t_us + 10, load_id=7,
                 sequence=seq, generation=generation,
                 capture_publish_us=t_us)


class AdversarialBoundary(unittest.TestCase):
    def test_drop_from_prior_generation_cannot_claim_current_underflow(self):
        trace = "\n".join([
            event("PROCESSOR_PUBLISH_DROP", 1_500_000, load_id=7,
                  generation=2, sequence=19, reason="processed_ring_full"),
            event("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
                  last_generation=3, paused=0, seeking=0, core_idle=0),
        ])
        result = analyze(trace)
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertEqual(result["root_cause"], "UNKNOWN")

    def test_pause_for_cache_underflow_is_not_active_playback_drop(self):
        trace = "\n".join([
            capture(100, 1_000_000),
            event("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
                  last_generation=3, pause_for_cache=1, paused=1),
            capture(101, 2_100_000),
        ])
        result = analyze(trace)
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertFalse(result["audible_output_verified"])

    def test_core_idle_underflow_is_not_active_playback_drop(self):
        trace = "\n".join([
            capture(100, 1_000_000),
            event("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
                  last_generation=3, core_idle=1),
            capture(101, 2_100_000),
        ])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_power_shell_wrapper_has_remote_coldurl_path(self):
        text = (ROOT / "scripts" / "issue42_s01_physical.ps1").read_text(encoding="utf-8")
        self.assertIn("[string]$MediaUrl", text)
        self.assertIn("'-Scenario','ColdUrl'", text)
        self.assertIn("'-MediaUrl',$MediaUrl", text)
        self.assertIn("Only one of -MediaPath and -MediaUrl", text)


if __name__ == "__main__":
    unittest.main()
