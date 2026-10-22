"""Issue #42 PCM01: native libmpv AO=pcm baseline with owned localhost audio.

REAL pinned libmpv on hosted Windows CI, no application or physical audio.
Replicates only AudioCaptureManager::ConfigureMpvPcmTransport output format:
raw float32, 48000 Hz, stereo, ao-pcm-waveheader=no, ao=pcm.
A real HTTP receipt + selected AUDIO track + a nonempty finite nonzero
frame-aligned PCM file are all required. No fake "PCM_OK" marker can satisfy.
Does not claim named pipe receipt, processor, SDL, audible output or fault cause.
"""
import ctypes
import math
import os
from pathlib import Path
import struct
import sys
import tempfile
import time

from issue42_libmpv_http_probe import (
    RealMpv, Source, localhost_fixture, selected_track, FILE_LOADED,
)

SAMPLE_BYTES = 4
CHANNELS = 2
FRAME_BYTES = SAMPLE_BYTES * CHANNELS
MIN_PCM_FRAMES = 2400


def verify_native_pcm(lib):
    lib.mpv_set_property_string.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p,
    ]
    lib.mpv_set_property_string.restype = ctypes.c_int
    with tempfile.TemporaryDirectory(prefix="i42-pcm01-") as temp:
        target = Path(temp) / "audio.pcm"
        source = Source()
        with localhost_fixture(source) as base:
            mpv = RealMpv(lib)
            selected = False
            try:
                # Exact production PCM transport *format*, but an isolated
                # private file instead of the app's Windows named pipe.
                for option, val in [
                    ("ao-pcm-waveheader", "no"),
                    ("audio-format", "float"),
                    ("audio-samplerate", "48000"),
                    ("audio-channels", "stereo"),
                    ("ao-pcm-file", str(target)),
                    ("ao", "pcm"),
                ]:
                    rc = lib.mpv_set_property_string(
                        mpv.handle, option.encode("ascii"),
                        val.encode("utf-8"))
                    if rc < 0:
                        raise AssertionError("PCM01_PROPERTY_REJECTED_" +
                                             option.upper().replace("-", "_"))
                effective_ao = mpv.string_property("ao")
                if effective_ao != "pcm":
                    raise AssertionError("PCM01_EFFECTIVE_AO_NOT_PCM")

                rc = mpv.command("loadfile", base + "/video", "replace")
                if rc < 0:
                    raise AssertionError("PCM01_VIDEO_COMMAND_FAILED")
                mpv.until(lambda ev: FILE_LOADED in ev and
                          selected_track(mpv.tracks(), "video"))
                rc = mpv.command("audio-add", base + "/audio", "select")
                if rc < 0:
                    raise AssertionError("PCM01_AUDIO_COMMAND_FAILED")
                mpv.until(lambda _ev: selected_track(mpv.tracks(), "audio"))
                selected = True
                # The real AO may write asynchronously; await bounded
                # output without assuming a positive marker proves bytes.
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    if target.exists() and target.stat().st_size >= MIN_PCM_FRAMES * FRAME_BYTES:
                        break
                    time.sleep(0.09)
            finally:
                mpv.close()  # Flush the real AO before reading result.
            records = source.snapshot()
            audio_receipts = [
                x for x in records
                if x["role"] == "audio" and x["status"] == 200
                and x["bytes_sent"] == x["declared_bytes"]
                and x["bytes_sent"] > 1000
            ]
            video_receipts = [
                x for x in records
                if x["role"] == "video" and x["status"] == 200
                and x["bytes_sent"] == x["declared_bytes"]
            ]
            if not selected or not audio_receipts or not video_receipts:
                raise AssertionError("PCM01_HTTP_OR_AUDIO_SELECTION_MISSING")
            if not target.is_file():
                raise AssertionError("PCM01_NATIVE_PCM_FILE_MISSING")
            raw = target.read_bytes()
            if len(raw) < MIN_PCM_FRAMES * FRAME_BYTES:
                raise AssertionError("PCM01_NATIVE_PCM_TOO_SHORT")
            if len(raw) % FRAME_BYTES:
                raise AssertionError("PCM01_NATIVE_PCM_FRAME_MISALIGNED")
            nonzero = 0
            bad_float = 0
            for (sample,) in struct.iter_unpack("<f", raw):
                if not math.isfinite(sample):
                    bad_float += 1
                elif abs(sample) > 1e-6:
                    nonzero += 1
            if bad_float:
                raise AssertionError("PCM01_NONFINITE_FLOAT_SAMPLES")
            if nonzero == 0:
                raise AssertionError("PCM01_ALL_ZERO_FLOAT_PCM")
            # No sample contents, path or secret values are ever printed.
            print(
                "I42_NATIVE_PCM_AO_BASELINE_PASS "
                f"{{\"audio_http_receipts\":{len(audio_receipts)},"
                f"\"video_http_receipts\":{len(video_receipts)},"
                f"\"raw_pcm_bytes\":{len(raw)},"
                f"\"raw_pcm_frames\":{len(raw)//FRAME_BYTES},"
                f"\"nonzero_samples\":{nonzero},"
                "\"ao\":\"pcm\",\"sample_format\":\"float32\","
                "\"sample_rate\":48000,\"channels\":2,"
                "\"source_scope\":\"controlled_localhost\","
                "\"app_named_pipe_verified\":false,"
                "\"app_processor_verified\":false,"
                "\"sdl_output_verified\":false,"
                "\"audible_output_verified\":false,"
                "\"root_cause\":\"UNKNOWN\"}", flush=True
            )


def main():
    if sys.platform != "win32":
        raise SystemExit("PCM01_REQUIRES_WINDOWS_PINNED_LIBMPV")
    target = Path(__file__).resolve().parents[1] / "deps/mpv/libmpv-2.dll"
    if not target.is_file():
        raise SystemExit("PCM01_PINNED_DLL_MISSING")
    with os.add_dll_directory(str(target.parent)):
        verify_native_pcm(ctypes.WinDLL(str(target)))


if __name__ == "__main__":
    main()
