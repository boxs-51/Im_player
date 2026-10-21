"""RED-first S01 oracle: multiple underflows, chronological selection and identity isolation.

Synthetic trace contract only. These tests do not demonstrate audible playback.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from issue42_first_boundary import analyze


def event(stage, t, **kwargs):
    extra = " ".join(f"{k}={v}" for k, v in kwargs.items())
    return f"[AUDIO-TELEMETRY] BACKPRESSURE stage={stage} t_us={t} {extra}".strip()


def acquire(seq, publication, *, load=7, generation=3):
    return event("PROCESSOR_ACQUIRE_RAW", publication + 10, load_id=load,
                 generation=generation, sequence=seq, capture_publish_us=publication)


class MultipleUnderflowOracle(unittest.TestCase):
    def test_earlier_paused_event_does_not_hide_later_playback_gap(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 1_000_000, load_id=7, last_generation=3, paused=1),
            acquire(100, 3_000_000),
            event("SDL_QUEUE_UNDERFLOW", 3_600_000, load_id=7, last_generation=3,
                  paused=0, seeking=0, core_idle=0),
            acquire(101, 4_100_000),
        ])
        result = analyze(trace)
        self.assertEqual(result["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(result["capture_gap_us"], 1_100_000)
        self.assertEqual(result["selected_underflow_t_us"], 3_600_000)
        self.assertEqual(result["underflow_count"], 2)
        self.assertEqual(result["root_cause"], "UNKNOWN")
        self.assertFalse(result["audible_output_verified"])

    def test_earlier_missing_identity_does_not_hide_later_processor_drop(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 500_000, index=1),
            event("PROCESSOR_PUBLISH_DROP", 3_400_000, load_id=7,
                  generation=3, sequence=101),
            event("SDL_QUEUE_UNDERFLOW", 3_600_000, load_id=7, last_generation=3),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "PROCESSOR_DROP_OBSERVED")
        self.assertEqual(r["selected_underflow_t_us"], 3_600_000)
        self.assertEqual(r["underflow_count"], 2)

    def test_earlier_inconclusive_active_does_not_hide_later_gap(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 400_000, load_id=7, generation=3),
            acquire(20, 3_000_000),
            event("SDL_QUEUE_UNDERFLOW", 3_600_000, load_id=7, generation=3),
            acquire(21, 4_100_000),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["selected_underflow_t_us"], 3_600_000)

    def test_two_qualifying_events_choose_earliest_in_time_not_trace_order(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 6_600_000, load_id=7, last_generation=3),
            acquire(1, 3_000_000),
            event("PROCESSOR_PUBLISH_DROP", 6_400_000, load_id=7,
                  generation=3, sequence=11),
            event("SDL_QUEUE_UNDERFLOW", 3_600_000, load_id=7, last_generation=3),
            acquire(2, 4_100_000),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["selected_underflow_t_us"], 3_600_000)

    def test_different_loads_never_get_mixed_to_find_later_boundary(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 800_000, load_id=7, last_generation=3),
            acquire(1, 3_000_000, load=8),
            event("SDL_QUEUE_UNDERFLOW", 3_600_000, load_id=8, last_generation=3),
            acquire(2, 4_100_000, load=8),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(r["selected_underflow_t_us"], 3_600_000)

    def test_only_nonplaying_or_incomplete_events_remain_inconclusive(self):
        trace = "\n".join([
            event("SDL_QUEUE_UNDERFLOW", 900_000, load_id=7, paused=1),
            event("SDL_QUEUE_UNDERFLOW", 1_500_000, last_generation=3),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "INCONCLUSIVE")
        self.assertEqual(r["underflow_count"], 2)
        self.assertIsNone(r["selected_underflow_t_us"])
        self.assertEqual(r["root_cause"], "UNKNOWN")

    def test_single_event_baseline_remains_supported(self):
        trace = "\n".join([
            event("PROCESSOR_PUBLISH_DROP", 1_400_000, load_id=7,
                  generation=3, sequence=100),
            event("SDL_QUEUE_UNDERFLOW", 1_600_000, load_id=7,
                  last_generation=3),
        ])
        r = analyze(trace)
        self.assertEqual(r["classification"], "PROCESSOR_DROP_OBSERVED")
        self.assertEqual(r["underflow_count"], 1)


if __name__ == "__main__":
    unittest.main()
