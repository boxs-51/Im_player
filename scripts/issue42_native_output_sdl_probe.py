"""NAT03 native real AudioOutputWorker/SDL dummy, never physical-audio proof.

Healthy MPV/local WAV -> real capture -> processor -> output -> actual SDL queue;
cold 503 is a strict false-PASS guard. Pressure control records actual production
PROCESSOR_PUBLISH_DROP from a private same-process trace; RED means a controlled
stress finding, not a reproduced field URL outage or a production fix.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from issue42_libmpv_http_probe import Source, localhost_fixture

RECEIPT = re.compile(
    r"^I42_NAT03_REAL_OUTPUT_OBSERVED mode=(healthy|pressure) "
    r"raw_blocks=(\d+) sdl_blocks=(\d+) first_sdl_write=(\d) "
    r"queue_nonzero=(\d) highwater_bytes=(\d+) queued_bytes=(\d+) "
    r"format_drops=(\d+) write_failures=(\d+) underflows=(\d+) "
    r"driver=SDL_DUMMY capture=REAL_SOURCE processor=REAL_SOURCE "
    r"output=REAL_SOURCE sdl=REAL_SOURCE "
    r"audible=NOT_RUN midplay_503=NOT_RUN root_cause=UNKNOWN$",
    re.MULTILINE,
)
POSITIVE = "I42_NAT03_REAL_OUTPUT_OBSERVED "
HARD_CAP = 48000 * 2 * 4 * 150 // 1000


def invoke(binary, url, mode, trace):
    env = os.environ.copy()
    env["SDL_AUDIODRIVER"] = "dummy"
    env["IM_PLAYER_LIFECYCLE_LOG"] = str(trace)
    env["IM_PLAYER_AUDIO_CONTINUITY_TRACE"] = "1"
    env["IM_PLAYER_AUDIO_BACKPRESSURE_TRACE"] = "1"
    env["IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS"] = (
        "1000" if mode == "pressure" else "0"
    )
    try:
        p = subprocess.run(
            [str(binary), url, mode], env=env, capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=23, check=False,
        )
    except subprocess.TimeoutExpired as ex:
        raise AssertionError("NAT03_NATIVE_TIMEOUT") from ex
    # Do not emit arbitrary process stdout/stderr, samples, URLs or private paths.
    return p.returncode, p.stdout


def owned_receipts(source, status):
    return [
        x for x in source.snapshot()
        if x["role"] == "audio" and x["status"] == status
        and (status == 503 or
             (x["bytes_sent"] == x["declared_bytes"] and
              x["bytes_sent"] > 1000))
    ]


def positive(mode, rc, output):
    matches = RECEIPT.findall(output)
    scoped = [m for m in matches if m[0] == mode]
    valid = (
        rc == 0 and output.count(POSITIVE) == 1 and len(matches) == 1
        and len(scoped) == 1
    )
    if not valid:
        raise AssertionError("NAT03_REAL_OUTPUT_MARKER_NOT_PROVEN")
    fields = [int(x) for x in scoped[0][1:]]
    (raw, written, first_write, nonzero_queue, highwater, queued,
     fmt_drops, failures, underflows) = fields
    if not (raw >= 2 and written >= 1 and first_write == 1
            and nonzero_queue == 1 and 0 < highwater <= HARD_CAP
            and 0 <= queued <= HARD_CAP and fmt_drops == 0
            and failures == 0):
        raise AssertionError("NAT03_OUTPUT_METRICS_VIOLATE_CONTRACT")
    if mode == "healthy" and written < 2:
        raise AssertionError("NAT03_HEALTHY_TOO_FEW_SDL_WRITES")
    if mode == "pressure" and raw < 40:
        raise AssertionError("NAT03_PRESSURE_WAS_NOT_APPLIED")
    return (raw, written, underflows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "win32":
        raise SystemExit("NAT03_WINDOWS_ONLY")
    binary = args.binary.resolve()
    if not binary.is_file():
        raise AssertionError("NAT03_NATIVE_BINARY_MISSING")
    with tempfile.TemporaryDirectory(prefix="i42_nat03_") as root:
        root = Path(root)
        # Real SDL dummy queue, not a mocked AudioOutputWorker.
        healthy_source = Source()
        with localhost_fixture(healthy_source) as base:
            rc, output = invoke(binary, base + "/audio", "healthy",
                                root / "healthy.log")
        raw, written, _ = positive("healthy", rc, output)
        if not owned_receipts(healthy_source, 200):
            raise AssertionError("NAT03_HEALTHY_HTTP_RECEIPT_MISSING")

        # Independent cold failure must never claim that it queued PCM.
        failed_source = Source()
        failed_source.set_modes(audio="503")
        with localhost_fixture(failed_source) as base:
            failed_rc, failed_out = invoke(
                binary, base + "/audio", "cold503", root / "cold503.log"
            )
        if (failed_rc != 20 or POSITIVE in failed_out
                or not owned_receipts(failed_source, 503)):
            raise AssertionError("NAT03_HTTP_503_FALSE_SDL_SUCCESS")

        # RED-first controlled pressure with frozen production source.
        # The OUTPUT delay is injected through an EXISTING production diagnostic
        # env contract. A drop must be independently present in its own log.
        pressure_source = Source()
        with localhost_fixture(pressure_source) as base:
            pressure_rc, pressure_out = invoke(
                binary, base + "/audio", "pressure", root / "pressure.log"
            )
        pressure_raw, pressure_written, _ = positive(
            "pressure", pressure_rc, pressure_out
        )
        if not owned_receipts(pressure_source, 200):
            raise AssertionError("NAT03_PRESSURE_HTTP_RECEIPT_MISSING")
        log = (root / "pressure.log").read_text(
            encoding="utf-8", errors="replace"
        ) if (root / "pressure.log").exists() else ""
        drops = re.findall(
            r"\[AUDIO-TELEMETRY\] PROCESSOR_PUBLISH_DROP "
            r"reason=processed_ring_full sequence=\d+ generation=\d+",
            log
        )
        if not drops:
            raise AssertionError("NAT03_PRESSURE_RED_NOT_REPRODUCED")
        # A targeted RED observation, not a passing no-loss acceptance oracle.
        print(
            "I42_NAT03_PRESSURE_CONTINUITY_RED "
            f"processor_publish_drop_events={len(drops)} "
            f"raw_blocks={pressure_raw} sdl_blocks={pressure_written} "
            "controlled_output_delay_ms=1000 "
            "first_broken_boundary=PROCESSOR_TO_PROCESSED_RING "
            "cause_scope=CONTROLLED_BACKPRESSURE_ONLY "
            "field_media_root_cause=UNKNOWN",
            flush=True,
        )
        print(
            "I42_NAT03_NATIVE_SDL_DUMMY_CONTROL_PASS "
            f"healthy_raw_blocks={raw} healthy_sdl_blocks={written} "
            "healthy_audio_http=200 cold_failure_http=503 "
            "capture=REAL processor=REAL output=REAL sdl_queue=REAL "
            "hardware_audio=NOT_RUN late_http_503=NOT_RUN "
            "original_audio_dropout=NOT_REPRODUCED",
            flush=True,
        )


if __name__ == "__main__":
    main()
