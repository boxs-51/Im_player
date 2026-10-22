"""Issue #42 NAT02: validate real native capture -> processor, with HTTP controls.
No user media, paths, audio samples, native output or secrets are printed.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
from issue42_libmpv_http_probe import Source, localhost_fixture

PASS = "I42_NAT02_CAPTURE_PROCESSOR_SPSC_PASS "
RECEIPT = re.compile(
    r"^I42_NAT02_CAPTURE_PROCESSOR_SPSC_PASS "
    r"processed_blocks=(\d+) processed_frames=(\d+) nonzero_samples=(\d+) "
    r"capture=REAL_SOURCE processor=REAL_SOURCE "
    r"sdl=NOT_RUN audible=NOT_RUN root_cause=UNKNOWN$", re.MULTILINE
)


def invoke(binary, url):
    try:
        p = subprocess.run(
            [str(binary), url], capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=16, check=False
        )
    except subprocess.TimeoutExpired as ex:
        raise AssertionError("NAT02_NATIVE_TIMEOUT") from ex
    return p.returncode, p.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", type=Path, required=True)
    args = ap.parse_args()
    if sys.platform != "win32":
        raise SystemExit("NAT02_WINDOWS_REQUIRED")
    binary = args.binary.resolve()
    if not binary.is_file():
        raise AssertionError("NAT02_BINARY_MISSING")

    healthy = Source()
    with localhost_fixture(healthy) as root:
        rc, out = invoke(binary, root + "/audio")
    receipts = [
        r for r in healthy.snapshot()
        if r["role"] == "audio" and r["status"] == 200
        and r["bytes_sent"] == r["declared_bytes"]
        and r["bytes_sent"] > 1000
    ]
    match = RECEIPT.findall(out)
    valid = (
        rc == 0 and out.count(PASS) == 1 and len(match) == 1
        and int(match[0][0]) >= 2
        and int(match[0][1]) >= 2400
        and int(match[0][2]) > 0
        and bool(receipts)
    )
    if not valid:
        print(
            "I42_NAT02_HEALTHY_RED "
            f"exit={rc} matched={bool(match)} "
            f"healthy_audio_receipts={len(receipts)}", flush=True
        )
        raise AssertionError("NAT02_REAL_PROCESSOR_NOT_PROVEN")

    failed = Source()
    failed.set_modes(audio="503")
    with localhost_fixture(failed) as root:
        negative_rc, negative_out = invoke(binary, root + "/audio")
    failures = [
        r for r in failed.snapshot()
        if r["role"] == "audio" and r["status"] == 503
    ]
    if negative_rc == 0 or PASS in negative_out or not failures:
        print(
            "I42_NAT02_NEGATIVE_FALSE_PASS_RED "
            f"exit={negative_rc} audio_503={len(failures)}", flush=True
        )
        raise AssertionError("NAT02_HTTP_503_FALSE_PASS")
    print(
        "I42_NAT02_REAL_PROCESSOR_GATE_PASS "
        "capture=REAL_SOURCE processor=REAL_SOURCE "
        f"healthy_audio_receipts={len(receipts)} "
        f"negative_audio_503_receipts={len(failures)} "
        "sdl=NOT_RUN audible=NOT_RUN "
        "midplay_fault=NOT_REPRODUCED root_cause=UNKNOWN", flush=True
    )


if __name__ == "__main__":
    main()
