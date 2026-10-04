#!/usr/bin/env python3
"""Generate a deterministic AVI with RGB24 video + PCM mono audio for Issue #26.

The fixture is intentionally self-contained and requires no ffmpeg executable.
It is small enough for a >10-minute local drift run while providing a real
video timeline and a 44.1 kHz mono source that mpv must normalize into the
canonical 48 kHz stereo float32 transport.
"""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path
from typing import BinaryIO


def write_chunk(stream: BinaryIO, chunk_id: bytes, payload: bytes) -> int:
    start = stream.tell()
    stream.write(chunk_id)
    stream.write(struct.pack("<I", len(payload)))
    stream.write(payload)
    if len(payload) & 1:
        stream.write(b"\x00")
    return start


def start_list(stream: BinaryIO, list_type: bytes) -> int:
    stream.write(b"LIST")
    size_pos = stream.tell()
    stream.write(b"\x00\x00\x00\x00")
    stream.write(list_type)
    return size_pos


def end_list(stream: BinaryIO, size_pos: int) -> None:
    end = stream.tell()
    size = end - (size_pos + 4)
    stream.seek(size_pos)
    stream.write(struct.pack("<I", size))
    stream.seek(end)
    if size & 1:
        stream.write(b"\x00")


def make_video_frame(width: int, height: int, frame_index: int) -> bytes:
    # AVI BI_RGB frames with positive height are bottom-up BGR24.
    row_stride = ((width * 3 + 3) // 4) * 4
    padding = row_stride - width * 3
    row_pad = b"\x00" * padding
    frame = bytearray()

    for y in range(height - 1, -1, -1):
        for x in range(width):
            blue = (x * 5 + frame_index * 3) & 0xFF
            green = (y * 7 + frame_index * 2) & 0xFF
            red = ((x + y) * 4 + frame_index * 5) & 0xFF
            frame.extend((blue, green, red))
        frame.extend(row_pad)

    return bytes(frame)


def make_audio_chunk(
    sample_rate: int,
    samples_per_frame: int,
    frequency: float,
) -> bytes:
    # 440 Hz has an integral 44 cycles per 100 ms at 10 fps, so this chunk can
    # be repeated without introducing a discontinuity.
    amplitude = 0.10 * 32767.0
    payload = bytearray()
    for index in range(samples_per_frame):
        sample = int(
            amplitude
            * math.sin(2.0 * math.pi * frequency * index / sample_rate)
        )
        payload.extend(struct.pack("<h", sample))
    return bytes(payload)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=630)
    parser.add_argument("--fps", type=int, default=10)
    parser.add_argument("--width", type=int, default=64)
    parser.add_argument("--height", type=int, default=36)
    parser.add_argument("--sample-rate", type=int, default=44100)
    parser.add_argument("--frequency", type=float, default=440.0)
    args = parser.parse_args()

    if args.seconds < 5:
        parser.error("seconds must be at least 5")
    if args.fps <= 0:
        parser.error("fps must be positive")
    if args.width <= 0 or args.height <= 0:
        parser.error("width/height must be positive")
    if args.sample_rate <= 0 or args.sample_rate % args.fps != 0:
        parser.error("sample-rate must be positive and divisible by fps")
    if args.width > 320 or args.height > 240:
        parser.error("fixture dimensions intentionally capped at 320x240")

    args.output.parent.mkdir(parents=True, exist_ok=True)

    total_frames = args.seconds * args.fps
    samples_per_video_frame = args.sample_rate // args.fps
    audio_chunk = make_audio_chunk(
        args.sample_rate,
        samples_per_video_frame,
        args.frequency,
    )
    video_frame_size = ((args.width * 3 + 3) // 4) * 4 * args.height
    audio_block_align = 2
    audio_bytes_per_second = args.sample_rate * audio_block_align
    max_bytes_per_second = (
        video_frame_size * args.fps + audio_bytes_per_second + 4096
    )

    with args.output.open("w+b") as stream:
        stream.write(b"RIFF")
        riff_size_pos = stream.tell()
        stream.write(b"\x00\x00\x00\x00")
        stream.write(b"AVI ")

        hdrl_size_pos = start_list(stream, b"hdrl")

        avih = struct.pack(
            "<IIIIIIIIII4I",
            1_000_000 // args.fps,  # dwMicroSecPerFrame
            max_bytes_per_second,
            0,                      # dwPaddingGranularity
            0x10,                   # AVIF_HASINDEX
            total_frames,
            0,                      # dwInitialFrames
            2,                      # dwStreams
            max(video_frame_size, len(audio_chunk)),
            args.width,
            args.height,
            0, 0, 0, 0,
        )
        write_chunk(stream, b"avih", avih)

        video_strl_size_pos = start_list(stream, b"strl")
        video_strh = struct.pack(
            "<4s4sIHHIIIIIIIIhhhh",
            b"vids",
            b"DIB ",
            0,
            0,
            0,
            0,
            1,
            args.fps,
            0,
            total_frames,
            video_frame_size,
            0xFFFFFFFF,
            0,
            0, 0, args.width, args.height,
        )
        write_chunk(stream, b"strh", video_strh)
        video_strf = struct.pack(
            "<IiiHHIIiiII",
            40,
            args.width,
            args.height,
            1,
            24,
            0,  # BI_RGB
            video_frame_size,
            0,
            0,
            0,
            0,
        )
        write_chunk(stream, b"strf", video_strf)
        end_list(stream, video_strl_size_pos)

        audio_strl_size_pos = start_list(stream, b"strl")
        audio_strh = struct.pack(
            "<4s4sIHHIIIIIIIIhhhh",
            b"auds",
            b"\x00\x00\x00\x00",
            0,
            0,
            0,
            0,
            audio_block_align,
            audio_bytes_per_second,
            0,
            args.seconds * args.sample_rate,
            len(audio_chunk),
            0xFFFFFFFF,
            audio_block_align,
            0, 0, 0, 0,
        )
        write_chunk(stream, b"strh", audio_strh)
        audio_strf = struct.pack(
            "<HHIIHH",
            1,  # WAVE_FORMAT_PCM
            1,  # mono
            args.sample_rate,
            audio_bytes_per_second,
            audio_block_align,
            16,
        )
        write_chunk(stream, b"strf", audio_strf)
        end_list(stream, audio_strl_size_pos)

        end_list(stream, hdrl_size_pos)

        movi_size_pos = start_list(stream, b"movi")
        movi_data_start = stream.tell()
        # AVI idx1 offsets are relative to the start of the movi list payload,
        # including the four-byte "movi" list type. Therefore the first media
        # chunk has offset 4, not 0.
        movi_index_base = movi_data_start - 4
        index_entries: list[tuple[bytes, int, int, int]] = []

        for frame_index in range(total_frames):
            video_payload = make_video_frame(
                args.width,
                args.height,
                frame_index,
            )
            video_chunk_start = write_chunk(stream, b"00db", video_payload)
            index_entries.append(
                (
                    b"00db",
                    0x10,  # AVIIF_KEYFRAME
                    video_chunk_start - movi_index_base,
                    len(video_payload),
                )
            )

            audio_chunk_start = write_chunk(stream, b"01wb", audio_chunk)
            index_entries.append(
                (
                    b"01wb",
                    0,
                    audio_chunk_start - movi_index_base,
                    len(audio_chunk),
                )
            )

        end_list(stream, movi_size_pos)

        idx_payload = bytearray()
        for chunk_id, flags, offset, size in index_entries:
            idx_payload.extend(
                struct.pack("<4sIII", chunk_id, flags, offset, size)
            )
        write_chunk(stream, b"idx1", bytes(idx_payload))

        end = stream.tell()
        stream.seek(riff_size_pos)
        stream.write(struct.pack("<I", end - 8))
        stream.seek(end)

    size_mib = args.output.stat().st_size / (1024.0 * 1024.0)
    print(
        "[ISSUE26-LONGRUN] generated AV fixture "
        f"path={args.output} seconds={args.seconds} "
        f"fps={args.fps} size={args.width}x{args.height} "
        f"audio_rate={args.sample_rate} audio_channels=1 "
        f"size_mib={size_mib:.2f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
