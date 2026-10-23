"""S01 v2 CLI reproduction admission: semantic RED-first, no false PASS."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_issue42_telemetry_v2_redfirst import trace, gap, e

SCRIPT = HERE.parent / "scripts" / "issue42_first_boundary.py"


def execute(raw, *, require=True):
    with tempfile.TemporaryDirectory() as td:
        path = Path(td) / "trace.jsonl"
        out = Path(td) / "report.json"
        path.write_text(raw, encoding="utf-8")
        args = [sys.executable, str(SCRIPT), str(path), "--output", str(out)]
        if require:
            args.append("--require-reproduction")
        p = subprocess.run(args, capture_output=True, text=True, check=False)
        body = json.loads(out.read_text(encoding="utf-8")) if out.exists() else None
        return p, body


class V2CliGateOracle(unittest.TestCase):
    def test_structural_green_must_not_admit_unauthenticated_physical_reproduction(self):
        p, body = execute(trace(*gap()), require=True)
        self.assertEqual(body["classification"], "CAPTURE_PUBLICATION_GAP_OBSERVED")
        self.assertEqual(body["binding_status"], "VERIFIED")
        self.assertEqual(body["producer_attestation"], "UNVERIFIED")
        self.assertEqual(p.returncode, 2, p.stderr)
        self.assertNotIn("Traceback", p.stderr)

    def test_unbound_v2_does_not_admit_reproduction(self):
        raw = e(1, event_type="SDL_QUEUE_UNDERFLOW",
                component="AudioOutputWorker", session=None, load=None,
                gen=None, binding="UNBOUND")
        p, body = execute(trace(raw))
        self.assertEqual(body["classification"], "INCONCLUSIVE")
        self.assertEqual(p.returncode, 2, p.stderr)

    def test_valid_v2_without_require_does_not_claim_audible_output(self):
        p, body = execute(trace(*gap()), require=False)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertFalse(body["audible_output_verified"])
        self.assertEqual(body["root_cause"], "UNKNOWN")


if __name__ == "__main__":
    unittest.main()
