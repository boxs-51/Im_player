"""RED-first Issue #42 native pipe/device continuity attribution parity.

The source-level tests lock the producer field shape. Analyzer cases enforce
load+session+generation, preserving historical unbound pipe/device observations.
These are NOT proof of actual physical SDL or MPV playback.
"""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from issue42_first_boundary import analyze


def format_line(stage, clock, **values):
    tail = " ".join(f"{k}={v}" for k, v in values.items())
    return f"[AUDIO-TELEMETRY] CONTINUITY stage={stage} t_us={clock} {tail}"


def underflow(*, run=901, session=11, generation=3):
    return format_line(
        "SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
        run_uid=run, session_uid=session, last_generation=generation,
        paused=0, core_idle=0, idle_active=0, seeking=0, pause_for_cache=0)


def device_drop(*, run=901, session=11, generation=3):
    return format_line(
        "OUTPUT_DEVICE_OFFLINE_DROP", 1_500_000, load_id=7,
        run_uid=run, session_uid=session, generation=generation,
        sequence=100, pts=49.8, frames=512, reason="device_unavailable")


class NativeDevicePipeProvenance(unittest.TestCase):
    def test_device_drop_native_producer_emits_full_block_identity(self):
        code = (ROOT / "native/src/player/audio/AudioOutputWorker.cpp").read_text(encoding="utf-8")
        record = next(x for x in code.splitlines()
                      if "stage=OUTPUT_DEVICE_OFFLINE_DROP" in x)
        for field in ("load_id=%llu", "run_uid=%llu", "session_uid=%llu",
                      "t_us=%llu", "sequence=%llu", "generation=%llu"):
            self.assertIn(field, record)
        for field in ("dummy->startupLoadId", "dummy->telemetryRunUid",
                      "dummy->telemetrySessionUid", "dummy->generation"):
            self.assertIn(field, code)

    def test_pipe_disconnect_native_producer_emits_session_only_not_guess_load(self):
        code = (ROOT / "native/src/player/audio/AudioCaptureManager.cpp").read_text(encoding="utf-8")
        record = next(x for x in code.splitlines()
                      if "stage=PIPE_DISCONNECTED" in x)
        self.assertIn("run_uid=%llu", record)
        self.assertIn("session_uid=%llu", record)
        self.assertIn("connection_index=%llu", record)
        self.assertNotIn("load_id=", record)
        self.assertIn("AudioTelemetryProcessRunUid()", code)
        self.assertIn("m_telemetrySessionUid", code)

    def test_previous_generation_device_loss_cannot_attribute_current_underflow(self):
        result = analyze("\n".join([device_drop(generation=2), underflow()]))
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertFalse(result["audible_output_verified"])

    def test_matching_native_device_loss_remains_correlated_observation_only(self):
        result = analyze("\n".join([device_drop(), underflow()]))
        self.assertEqual(result["classification"], "OUTPUT_DEVICE_OFFLINE_DROP_OBSERVED")
        self.assertFalse(result["audible_output_verified"])
        self.assertEqual(result["root_cause"], "UNKNOWN")

    def test_other_session_device_drop_is_not_attributed(self):
        result = analyze("\n".join([device_drop(session=99), underflow(session=11)]))
        self.assertEqual(result["classification"], "INCONCLUSIVE")

    def test_pipe_with_native_session_but_no_load_is_context_only(self):
        pipe = format_line("PIPE_DISCONNECTED", 1_450_000, run_uid=901,
                           session_uid=11, connection_index=2, reason=109)
        result = analyze("\n".join([pipe, underflow()]))
        self.assertEqual(result["classification"], "INCONCLUSIVE")
        self.assertEqual(result["unscoped_marker_counts"]["PIPE_DISCONNECTED"], 1)
        self.assertIn("unscoped_pipe_disconnect_observed", result["evidence"])
        self.assertFalse(result["audible_output_verified"])


if __name__ == "__main__":
    unittest.main()
