#!/usr/bin/env python3
"""Generate a deterministic PCM WAV fixture for BRG5-C playback smoke."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--sample-rate", type=int, default=44100)
    parser.add_argument("--frequency", type=float, default=440.0)
    args = parser.parse_args()

    if args.seconds < 30:
        raise SystemExit("fixture duration must be at least 30 seconds")
    if args.sample_rate < 8000:
        raise SystemExit("sample rate too low")

    args.output.parent.mkdir(parents=True, exist_ok=True)

    amplitude = 0.10 * 32767.0
    frame_count = args.seconds * args.sample_rate

    with wave.open(str(args.output), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(args.sample_rate)

        chunk = bytearray()
        for i in range(frame_count):
            sample = int(amplitude * math.sin(2.0 * math.pi * args.frequency * i / args.sample_rate))
            chunk.extend(struct.pack("<h", sample))
            if len(chunk) >= 65536:
                wav.writeframesraw(chunk)
                chunk.clear()
        if chunk:
            wav.writeframesraw(chunk)

    print(
        f"[BRG5-C] generated media fixture path={args.output} "
        f"seconds={args.seconds} sample_rate={args.sample_rate}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
