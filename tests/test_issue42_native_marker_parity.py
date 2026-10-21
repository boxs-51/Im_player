"""S01 native-emission parity for identity-unscoped continuity markers.

These fixtures copy the source-emitted stage/field shape. No physical playback
is implied; absence of load_id forbids causal attribution to an SDL underflow.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze


def underflow(t, load=7):
    return ("[AUDIO-TELEMETRY] STARTUP_BOUNDARY stage=SDL_QUEUE_UNDERFLOW "
            f"load_id={load} t_us={t} last_generation=3 "
            "paused=0 core_idle=0 idle_active=0 seeking=0 pause_for_cache=0")


def real_pipe_disconnect(t):
    return ("[AUDIO-TELEMETRY] CONTINUITY stage=PIPE_DISCONNECTED "
            f"t_us={t} connection_index=2 reason=109 bytes_read=0 "
            "last_read_us=1200000 gap_since_last_read_us=200000 running=1")


def real_device_drop(t):
    return ("[AUDIO-TELEMETRY] CONTINUITY stage=OUTPUT_DEVICE_OFFLINE_DROP "
            f"t_us={t} sequence=43 generation=3 pts=49.800 frames=512 "
            "processed_ring_size=4 reason=device_unavailable")


class NativeMarkerParity(unittest.TestCase):
    def test_unscoped_pipe_disconnection_is_observed_but_not_attributed(self):
        r = analyze(real_pipe_disconnect(1_400_000) + "\n" + underflow(1_600_000))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["unscoped_marker_counts"]["PIPE_DISCONNECTED"], 1)
        self.assertIn("unscoped_pipe_disconnect_observed", r["evidence"])
        self.assertFalse(r["audible_output_verified"])
        self.assertEqual(r["root_cause"], "UNKNOWN")

    def test_unscoped_output_drop_is_observed_but_not_attributed(self):
        r = analyze(real_device_drop(1_500_000) + "\n" + underflow(1_600_000))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["unscoped_marker_counts"]["OUTPUT_DEVICE_OFFLINE_DROP"], 1)
        self.assertIn("unscoped_output_offline_drop_observed", r["evidence"])
        self.assertFalse(r["audible_output_verified"])

    def test_two_unscoped_markers_are_counted_without_crossload_join(self):
        trace = "\n".join([real_pipe_disconnect(1_300_000),
                           real_device_drop(1_500_000),
                           underflow(1_600_000, load=8)])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["unscoped_marker_counts"]["PIPE_DISCONNECTED"], 1)
        self.assertEqual(r["unscoped_marker_counts"]["OUTPUT_DEVICE_OFFLINE_DROP"], 1)
        self.assertIsNone(r["selected_underflow_t_us"])

    def test_scoped_processor_drop_is_still_correlatable(self):
        drop = ("[AUDIO-TELEMETRY] STARTUP_BOUNDARY "
                "stage=PROCESSOR_PUBLISH_DROP load_id=7 t_us=1500000 "
                "sequence=44 generation=3 pts=49.810 retries=5")
        r = analyze(drop + "\n" + underflow(1_600_000))
        self.assertEqual(r["classification"], "PROCESSOR_DROP_OBSERVED")
        self.assertEqual(r["unscoped_marker_counts"]["PIPE_DISCONNECTED"], 0)
        self.assertEqual(r["unscoped_marker_counts"]["OUTPUT_DEVICE_OFFLINE_DROP"], 0)


if __name__ == "__main__":
    unittest.main()
