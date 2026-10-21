"""S01 source-independent synthetic trace oracle; NOT physical audio E2E."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze  # noqa: E402


def mark(stage, t, **kwargs):
    fields = " ".join(f"{k}={v}" for k, v in kwargs.items())
    return f"[AUDIO-TELEMETRY] BACKPRESSURE stage={stage} t_us={t} {fields}".rstrip()


def packet(seq, time, gen=3, load=7):
    return mark("PROCESSOR_ACQUIRE_RAW", time + 10, load_id=load,
                generation=gen, sequence=seq, capture_publish_us=time, pts=seq * .02)


class FirstBoundary(unittest.TestCase):
    def test_correlated_capture_gap_is_observed_not_called_cause(self):
        trace = "\n".join([packet(1, 1_000_000), packet(2, 1_020_000),
                          mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7, index=1, last_generation=3),
                          packet(3, 2_120_000)])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["capture_gap_us"], 1_100_000)
        self.assertEqual(r["root_cause"], "UNKNOWN")
        self.assertFalse(r["audible_output_verified"])

    def test_unrelated_load_must_not_classify(self):
        trace = "\n".join([packet(1, 1_000_000, load=9), packet(2, 2_200_000, load=9),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_generation_switch_does_not_get_joined(self):
        trace = "\n".join([packet(1, 1_000_000, gen=3), packet(1, 2_200_000, gen=4),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_explicit_pipe_disconnect_is_not_automatic_root_cause(self):
        trace = "\n".join([mark("PIPE_DISCONNECTED", 1_500_000, load_id=7),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "PIPE_DISCONNECT_OBSERVED")
        self.assertEqual(analyze(trace)["root_cause"], "UNKNOWN")

    def test_device_drop_marker_prioritized(self):
        trace = "\n".join([mark("OUTPUT_DEVICE_OFFLINE_DROP", 1_500_000, load_id=7),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "OUTPUT_DEVICE_OFFLINE_DROP_OBSERVED")

    def test_no_underflow_is_not_playback_pass(self):
        r = analyze("\n".join([packet(1, 1_000_000), packet(2, 1_020_000)]))
        self.assertEqual(r["classification"], "NO_UNDERFLOW_OBSERVED")
        self.assertFalse(r["audible_output_verified"])

    def test_incomplete_underflow_identity_fail_closed(self):
        r = analyze(mark("SDL_QUEUE_UNDERFLOW", 1_500_000, index=4))
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertIn("underflow_missing_load_identity", r["evidence"])

    def test_gap_outside_underflow_window_no_attribution(self):
        trace = "\n".join([packet(1, 1_000_000), packet(2, 2_200_000),
                           mark("SDL_QUEUE_UNDERFLOW", 3_000_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_processor_drop_requires_correlated_identity_and_time(self):
        unrelated = mark("PROCESSOR_PUBLISH_DROP", 1_400_000,
                         load_id=8, generation=3, sequence=9)
        matched = mark("PROCESSOR_PUBLISH_DROP", 1_400_000,
                       load_id=7, generation=3, sequence=9)
        underflow = mark("SDL_QUEUE_UNDERFLOW", 1_600_000,
                         load_id=7, last_generation=3)
        self.assertEqual(analyze(unrelated + "\n" + underflow)["classification"],
                         "INCONCLUSIVE")
        self.assertEqual(analyze(matched + "\n" + underflow)["classification"],
                         "PROCESSOR_DROP_OBSERVED")

    def test_generation_mismatch_underflow_does_not_claim_capture(self):
        trace = "\n".join([packet(1, 1_000_000, gen=3),
                           packet(2, 2_200_000, gen=3),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000,
                                load_id=7, last_generation=4)])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_unknown_underflow_generation_never_correlates_capture(self):
        trace = "\n".join([packet(1, 1_000_000), packet(2, 2_200_000),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7)])
        self.assertEqual(analyze(trace)["classification"], "INCONCLUSIVE")

    def test_segment_underflow_generation_is_respected(self):
        trace = "\n".join([packet(1, 1_000_000), packet(2, 2_200_000),
                           mark("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
                                generation=3, source="segment")])
        self.assertEqual(analyze(trace)["classification"],
                         "CAPTURE_PUBLICATION_GAP_OBSERVED")

    def test_reject_zero_threshold(self):
        with self.assertRaises(ValueError):
            analyze("", threshold_us=0)


if __name__ == "__main__":
    unittest.main()
