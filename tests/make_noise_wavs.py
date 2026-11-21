#!/usr/bin/env python3
"""Writes the fixed noise files of the Noise File tests (4.2) into <dir>/null_jsfx_noise/.

They are made, not checked in: the same bytes on every machine (a Park-Miller generator, as the
JSFX's rand() replacement, and integer or IEEE single-precision samples).

  a_room_44k_stereo16.wav  10 s, 44.1 kHz, stereo, 16-bit: room tone (low-passed noise, a
                           different noise per channel) plus a 50 Hz hum and its 3rd harmonic
  b_room_48k_mono_f32.wav   8 s, 48 kHz, mono, 32-bit float: brighter hiss
  c_long_22k_mono16.wav    65 s, 22.05 kHz, mono, 16-bit: longer than 60 s (only 60 s are used)
  d_short.wav               800 samples, 44.1 kHz, mono, 16-bit: shorter than one frame at
                           up to 96 kHz (no profile)

Usage: make_noise_wavs.py <dir>
"""
import math
import struct
import sys
from pathlib import Path


class ParkMiller:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 16807) % 2147483647
        return self.s / 2147483647.0


def wav(path, rate, channels, frames, fmt):
    """frames: list of per-frame tuples of floats in -1..1; fmt 's16' or 'f32'."""
    if fmt == "s16":
        data = b"".join(struct.pack("<" + "h" * channels, *(max(-32768, min(32767, int(round(v * 32767)))) for v in f))
                        for f in frames)
        tag, bits = 1, 16
    else:
        data = b"".join(struct.pack("<" + "f" * channels, *f) for f in frames)
        tag, bits = 3, 32
    block = channels * bits // 8
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, tag, channels, rate, rate * block, block, bits)
    hdr += b"data" + struct.pack("<I", len(data))
    path.write_bytes(hdr + data)


def room(rate, frames, channels, seed, cutoff, hum, level):
    rngs = [ParkMiller(seed + 7919 * c) for c in range(channels)]
    a = math.exp(-2 * math.pi * cutoff / rate)
    lp = [0.0] * channels
    out = []
    for i in range(frames):
        t = i / rate
        h = hum * (math.sin(2 * math.pi * 50 * t) + 0.3 * math.sin(2 * math.pi * 150 * t))
        f = []
        for c in range(channels):
            x = rngs[c].next() * 2 - 1
            lp[c] = lp[c] * a + x * (1 - a)
            f.append(level * (lp[c] * 3 + 0.2 * x) + h)
        out.append(tuple(f))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    d = Path(sys.argv[1]) / "null_jsfx_noise"
    d.mkdir(parents=True, exist_ok=True)
    wav(d / "a_room_44k_stereo16.wav", 44100, 2, room(44100, 10 * 44100, 2, 12345, 900, 0.004, 0.02), "s16")
    wav(d / "b_room_48k_mono_f32.wav", 48000, 1, room(48000, 8 * 48000, 1, 777, 6000, 0.0, 0.015), "f32")
    wav(d / "c_long_22k_mono16.wav", 22050, 1, room(22050, 65 * 22050, 1, 4242, 1500, 0.002, 0.02), "s16")
    wav(d / "d_short.wav", 44100, 1, room(44100, 800, 1, 99, 900, 0.0, 0.02), "s16")
    print(f"make_noise_wavs: wrote 4 files to {d}")


if __name__ == "__main__":
    main()
