"""Issue42 NAT01 Windows CI runner for REAL AudioCaptureManager source.

Only owned 127.0.0.1 WAV. The positive control needs real captured SPSC
AudioBlock evidence and independently completed AUDIO HTTP receipt. The
negative HTTP 503 must NOT be reported as capture success. No host GUI,
audible output, user-media URL or production-recovery assertion.
"""
import argparse
from pathlib import Path
import subprocess
import sys

from issue42_libmpv_http_probe import Source, localhost_fixture


def invoke(binary, url):
    cmd = [str(binary), url]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              encoding="utf-8", errors="replace",
                              timeout=16, check=False)
    except subprocess.TimeoutExpired as err:
        raise AssertionError("NAT01_REAL_CAPTURE_PROCESS_TIMEOUT") from err
    # Never emit stdout/stderr from the native binary: logger messages may
    # contain paths. Only locally defined bounded marker strings are parsed.
    return proc.returncode, proc.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        raise SystemExit("NAT01_TEST_REQUIRES_WINDOWS")
    binary = args.binary.resolve()
    if not binary.is_file():
        raise AssertionError("NAT01_NATIVE_EXECUTABLE_MISSING")

    healthy = Source()
    with localhost_fixture(healthy) as root:
        rc, output = invoke(binary, root + "/audio")
    audio_receipts = [
        r for r in healthy.snapshot()
        if r["role"] == "audio" and r["status"] == 200
        and r["bytes_sent"] == r["declared_bytes"]
        and r["bytes_sent"] > 1000
    ]
    valid_success = "I42_NAT01_PRODUCTION_CAPTURE_SPSC_PASS "
    if rc != 0 or output.count(valid_success) != 1 or not audio_receipts:
        print("I42_NAT01_HEALTHY_CONTROL_RED " +
              f"exit={rc} native_pass_count={output.count(valid_success)} "
              f"healthy_http_receipts={len(audio_receipts)} "
              "first_broken_boundary=UNKNOWN", flush=True)
        raise AssertionError("NAT01_REAL_CAPTURE_HEALTHY_NOT_PROVEN")

    failed = Source()
    failed.set_modes(audio="503")
    with localhost_fixture(failed) as root:
        rc, output = invoke(binary, root + "/audio")
    fault_receipts = [
        r for r in failed.snapshot() if r["role"] == "audio" and r["status"] == 503
    ]
    if rc == 0 or valid_success in output or not fault_receipts:
        print("I42_NAT01_NEGATIVE_FALSE_PASS_RED " +
              f"exit={rc} false_pass={valid_success in output} "
              f"http_503_receipts={len(fault_receipts)}", flush=True)
        raise AssertionError("NAT01_HTTP_503_MISCLASSIFIED_AS_CAPTURE_SUCCESS")

    print("I42_NAT01_NATIVE_SOURCE_GATE_PASS "
          "capture_manager_source=REAL_COMPILED "
          f"healthy_audio_http_receipts={len(audio_receipts)} "
          f"negative_audio_503_receipts={len(fault_receipts)} "
          "actual_win32_pipe=true raw_spsc_blocks=true "
          "app_audio_processor=NOT_RUN app_sdl=NOT_RUN "
          "original_midplay_symptom=NOT_REPRODUCED "
          "first_broken_boundary=UNKNOWN root_cause=UNKNOWN", flush=True)


if __name__ == "__main__":
    main()
