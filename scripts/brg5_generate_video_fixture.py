#!/usr/bin/env python3
"""Generate a deterministic YUV4MPEG2 video fixture for BRG5-E/AUD-8-02."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=12)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--width", type=int, default=160)
    parser.add_argument("--height", type=int, default=90)
    args = parser.parse_args()

    if args.seconds <= 0 or args.fps <= 0:
        parser.error("seconds and fps must be positive")
    if args.width <= 0 or args.height <= 0:
        parser.error("width and height must be positive")
    if args.width % 2 or args.height % 2:
        parser.error("YUV420 fixture width and height must be even")

    args.output.parent.mkdir(parents=True, exist_ok=True)

    frame_count = args.seconds * args.fps
    uv_width = args.width // 2
    uv_height = args.height // 2

    with args.output.open("wb") as stream:
        header = (
            f"YUV4MPEG2 W{args.width} H{args.height} "
            f"F{args.fps}:1 Ip A1:1 C420jpeg\n"
        )
        stream.write(header.encode("ascii"))

        for frame in range(frame_count):
            stream.write(b"FRAME\n")

            # Moving horizontal luma gradient: every frame is visibly distinct.
            row = bytes(
                16 + ((x * 3 + frame * 5) % 220)
                for x in range(args.width)
            )
            y_plane = row * args.height

            # Slowly varying chroma keeps the stream deterministic while
            # ensuring decoded frames are not a static grayscale image.
            u_value = 96 + (frame % 48)
            v_value = 160 - (frame % 48)
            u_plane = bytes([u_value]) * (uv_width * uv_height)
            v_plane = bytes([v_value]) * (uv_width * uv_height)

            stream.write(y_plane)
            stream.write(u_plane)
            stream.write(v_plane)

    print(
        "[BRG5-E] generated video fixture "
        f"path={args.output} frames={frame_count} "
        f"size={args.width}x{args.height} fps={args.fps}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
