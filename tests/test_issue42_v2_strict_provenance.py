"""RED-first: v2 cannot attribute gaps across forged or stale provenance.

Exact immutable controls from T1 and CLI remain untouched. A verifier is not
a native attestation authority; these tests only exercise rejection behavior.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_issue42_telemetry_v2_redfirst import e, gap, trace, lines, lifecycle
from issue42_first_boundary import analyze


class StrictProvenanceOracles(unittest.TestCase):
    def test_duplicate_emitter_sequence_distinct_ids_fails_closed(self):
        a, u, b = gap()
        forged = dict(a)
        forged["event_id"] = "forged-distinct-id-for-same-emitter-sequence"
        forged["event_mono_us"] = 1_850_100
        forged["capture_publish_us"] = 1_850_000
        report = analyze(trace(a, u, forged, b))
        self.assertEqual(report["classification"], "INCONCLUSIVE")
        self.assertIn("DUPLICATE_EMITTER_SEQUENCE", report["reason_codes"])
        self.assertNotEqual(report["binding_status"], "VERIFIED")

    def test_capture_acquisition_old_active_lease_never_correlates(self):
        a, u, b = gap()
        a["active_lease"] = "stale-lease"
        b["active_lease"] = "stale-lease"
        report = analyze(trace(a, u, b))
        self.assertEqual(report["classification"], "INCONCLUSIVE")
        self.assertIn("ACQUISITION_LEASE_MISMATCH", report["reason_codes"])
        self.assertNotEqual(report["binding_status"], "VERIFIED")

    def test_generation_started_before_owning_load_rejected(self):
        a, u, b = gap()
        owner = list(lifecycle())
        owner[3]["event_mono_us"] = 150
        self.assertLess(owner[3]["event_mono_us"], owner[2]["event_mono_us"])
        report = analyze(lines(*owner, a, u, b))
        self.assertEqual(report["classification"], "INCONCLUSIVE")
        self.assertIn("OWNER_CHRONOLOGY_INVALID", report["reason_codes"])
        self.assertNotEqual(report["binding_status"], "VERIFIED")

    def test_zero_load_id_is_absence_not_bound_pcm_identity(self):
        a, u, b = gap()
        for event in (a, u, b):
            event["load_id"] = 0
        owner = list(lifecycle(load=0))
        report = analyze(lines(*owner, a, u, b))
        self.assertEqual(report["classification"], "INCONCLUSIVE")
        self.assertIn("ZERO_LOAD_ID_UNBOUND", report["reason_codes"])
        self.assertNotEqual(report["binding_status"], "VERIFIED")

    def test_valid_owner_lease_gap_still_remains_structural_observation(self):
        a, u, b = gap()
        report = analyze(trace(a, u, b))
        self.assertEqual(report["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(report["binding_status"], "VERIFIED")
        self.assertEqual(report["producer_attestation"], "UNVERIFIED")
        self.assertFalse(report["audible_output_verified"])
        self.assertEqual(report["root_cause"], "UNKNOWN")


if __name__ == "__main__":
    unittest.main()
