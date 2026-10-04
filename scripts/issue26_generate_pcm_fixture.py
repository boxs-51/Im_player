#!/usr/bin/env python3
"""Generate deterministic PCM WAV inputs for Issue #26 normalization evidence."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=12)
    parser.add_argument("--sample-rate", type=int, required=True)
    parser.add_argument("--channels", type=int, choices=(1, 2), required=True)
    parser.add_argument("--frequency", type=float, default=440.0)
    args = parser.parse_args()

    if args.seconds < 5:
        raise SystemExit("fixture duration must be at least 5 seconds")
    if args.sample_rate not in (44100, 48000):
        raise SystemExit("Issue #26 fixture rate must be 44100 or 48000")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    amplitude = 0.10 * 32767.0
    frame_count = args.seconds * args.sample_rate

    with wave.open(str(args.output), "wb") as wav:
        wav.setnchannels(args.channels)
        wav.setsampwidth(2)
        wav.setframerate(args.sample_rate)

        chunk = bytearray()
        for i in range(frame_count):
            left = int(
                amplitude
                * math.sin(2.0 * math.pi * args.frequency * i / args.sample_rate)
            )
            if args.channels == 1:
                chunk.extend(struct.pack("<h", left))
            else:
                right = int(
                    amplitude
                    * math.sin(
                        2.0 * math.pi * (args.frequency * 1.5) * i / args.sample_rate
                    )
                )
                chunk.extend(struct.pack("<hh", left, right))

            if len(chunk) >= 65536:
                wav.writeframesraw(chunk)
                chunk.clear()

        if chunk:
            wav.writeframesraw(chunk)

    print(
        "[ISSUE26] generated fixture "
        f"path={args.output} sample_rate={args.sample_rate} "
        f"channels={args.channels} seconds={args.seconds}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
