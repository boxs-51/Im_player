"""Issue #42 PCM02: same-run real libmpv HLS AUDIO 503 + raw PCM output.

TEST-ONLY Windows hosted probe. Require MID01/MID02 owned external audio
selected, independent late-segment AUDIO HTTP 503 and advancing VIDEO media
time AFTER the fault, AND raw nonzero finite frame-aligned libmpv AO=pcm.
PCM file is a separate temp artifact, NOT Im_player's Windows named pipe,
processor, SDL or an audible device. Its mere existence is never a PASS.
Do not infer PCM *continued* after the fault: AO can buffer/write ahead.
"""
import ctypes
import math
import os
from pathlib import Path
import struct
import sys
import tempfile

from issue42_mpv_midstream_probe import experiment

CHANNELS = 2
BYTES_PER_FLOAT32 = 4
BYTES_PER_FRAME = CHANNELS * BYTES_PER_FLOAT32
MIN_FRAMES = 2400
MAX_BYTES = 32 * 1024 * 1024


def verify_pcm_fault_same_run(lib):
    with tempfile.TemporaryDirectory(prefix="i42-pcm02-") as root:
        path = Path(root) / "owned-audio.pcm"
        evidence = experiment(lib, pcm_output_path=path)
        if not isinstance(evidence, dict):
            raise AssertionError("PCM02_NATIVE_FAULT_PROOF_ABSENT")
        if (evidence.get("native_midplay_http_503_verified") is not True or
                evidence.get("external_audio_selected_pre_fault") is not True or
                evidence.get("video_timeline_advanced_after_fault") is not True or
                evidence.get("audio_503_receipt_count") != 1):
            raise AssertionError("PCM02_FAULT_OR_SOURCE_BINDING_UNVERIFIED")
        if not path.is_file():
            raise AssertionError("PCM02_REAL_LIBMPV_PCM_FILE_MISSING")
        size = path.stat().st_size
        if size < MIN_FRAMES * BYTES_PER_FRAME or size > MAX_BYTES:
            raise AssertionError("PCM02_PCM_LENGTH_OUT_OF_BOUNDS")
        if size % BYTES_PER_FRAME:
            raise AssertionError("PCM02_RAW_PCM_FRAME_MISALIGNED")
        raw = path.read_bytes()
        finite_count = 0
        nonzero_count = 0
        for (sample,) in struct.iter_unpack("<f", raw):
            if not math.isfinite(sample):
                raise AssertionError("PCM02_NONFINITE_FLOAT_SAMPLE")
            finite_count += 1
            if abs(sample) > 1e-6:
                nonzero_count += 1
        if finite_count != 2 * (size // BYTES_PER_FRAME):
            raise AssertionError("PCM02_SAMPLE_COUNT_MISMATCH")
        if nonzero_count == 0:
            raise AssertionError("PCM02_ZERO_ONLY_PCM_IS_NOT_AUDIO_EVIDENCE")
        # Validity of decoded PCM with a fault is not proof it happened AFTER
        # the fault. A valid post-fault first broken boundary needs timestamped
        # app pipe read, processed block + SDL write in the same run.
        print("I42_PCM02_REAL_AO_WITH_AUDIO_HTTP_503_PASS "
              f"raw_pcm_bytes={size} raw_pcm_frames={size // BYTES_PER_FRAME} "
              f"finite_samples={finite_count} nonzero_samples={nonzero_count} "
              "audio_fault_receipts=1 video_post_fault_progress=true "
              "pcm_post_fault_continuity=UNVERIFIED "
              "app_named_pipe=NOT_RUN app_processor=NOT_RUN sdl=NOT_RUN "
              "audible_output=NOT_RUN first_broken_boundary=UNKNOWN",
              flush=True)


def main():
    if sys.platform != "win32":
        raise SystemExit("PCM02_REQUIRES_WINDOWS_PINNED_LIBMPV")
    path = Path(__file__).resolve().parents[1] / "deps/mpv/libmpv-2.dll"
    if not path.is_file():
        raise SystemExit("PCM02_PINNED_LIBMPV_MISSING")
    with os.add_dll_directory(str(path.parent)):
        verify_pcm_fault_same_run(ctypes.WinDLL(str(path)))


if __name__ == "__main__":
    main()
