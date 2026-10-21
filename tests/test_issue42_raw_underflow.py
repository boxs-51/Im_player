"""S01 RED-first native raw SDL underflow visibility (no false NO_UNDERFLOW).

AudioOutputWorker always emits an ERROR SDL_QUEUE_UNDERFLOW line and only
conditionally emits the structured STARTUP_BOUNDARY stage/load/t_us record.
Raw markers cannot provide a load/generation correlation or audible proof.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze


def raw(index=1, source="idle"):
    extra = " source=segment" if source == "segment" else ""
    return ("[AUDIO-TELEMETRY] ERROR SDL_QUEUE_UNDERFLOW "
            f"index={index} empty_us=500000 grace_us=250000 "
            "action=rearm_prebuffer" + extra)


STRUCTURED = (
    "[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage=SDL_QUEUE_UNDERFLOW "
    "load_id=7 t_us=1600000 index=1 last_generation=3 "
    "paused=0 core_idle=0 idle_active=0 seeking=0 pause_for_cache=0"
)


class NativeRawUnderflowOracle(unittest.TestCase):
    def test_unstructured_native_underflow_must_not_be_reported_as_absent(self):
        r = analyze(raw())
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["raw_underflow_marker_count"], 1)
        self.assertEqual(r["underflow_count"], 0)
        self.assertFalse(r["audible_output_verified"])
        self.assertEqual(r["root_cause"], "UNKNOWN")

    def test_multiple_raw_idle_segment_underflows_count_without_attribution(self):
        r = analyze("\n".join([raw(1), raw(2, "segment")]))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["raw_underflow_marker_count"], 2)
        self.assertIsNone(r["selected_underflow_t_us"])
        self.assertEqual(r["underflow_count"], 0)

    def test_raw_and_structured_pair_does_not_double_count_scoped_events(self):
        r = analyze(raw() + "\n" + STRUCTURED)
        self.assertEqual(r["raw_underflow_marker_count"], 1)
        self.assertEqual(r["underflow_count"], 1)
        self.assertEqual(r["classification"], "INCONCLUSIVE")

    def test_unrelated_audio_error_must_not_create_underflow(self):
        trace = "[AUDIO-TELEMETRY] ERROR PROCESSOR_PUBLISH_DROP index=1"
        r = analyze(trace)
        self.assertEqual(r["classification"], "NO_UNDERFLOW_OBSERVED")
        self.assertEqual(r["raw_underflow_marker_count"], 0)


if __name__ == "__main__":
    unittest.main()
