#!/usr/bin/env python3
"""Generates the app's sounds into assets/ (16-bit mono 44.1 kHz WAV).

    python3 tools/gen_sounds.py

chime.wav     - session complete: a rising four-note phrase
break_end.wav - break over: a two-tone "ding-dong", played twice

Each file ends in silence: the app loops them until the user responds, and
the silence is the pause between repeats.
"""
import math
import struct
import wave
from pathlib import Path

RATE = 44100
ASSETS = Path(__file__).resolve().parent.parent / "assets"


def note(freq, start, length, decay, volume):
    """(start_sec, samples) for a sine with a short attack and exp decay,
    plus a faint octave harmonic for a bell-like tone."""
    n = int(length * RATE)
    attack = int(0.008 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-t / decay) * min(1.0, i / attack)
        # Fade the last 20 ms to zero to avoid a click.
        env *= min(1.0, (n - i) / (0.02 * RATE))
        s = math.sin(2 * math.pi * freq * t) + 0.25 * math.sin(4 * math.pi * freq * t)
        out.append(volume * env * s / 1.25)
    return start, out


def render(notes, length):
    buf = [0.0] * int(length * RATE)
    for start, samples in notes:
        offset = int(start * RATE)
        for i, s in enumerate(samples):
            if offset + i < len(buf):
                buf[offset + i] += s
    return buf


def write(name, samples):
    path = ASSETS / name
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(
            struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))
    print(f"wrote {path} ({len(samples) / RATE:.2f}s)")


if __name__ == "__main__":
    # C6 E6 G6 C7, then ~1 s of quiet.
    arpeggio = [1046.5, 1318.5, 1568.0, 2093.0]
    write("chime.wav", render(
        [note(f, i * 0.14, 0.8, 0.22, 0.34 - 0.03 * i) for i, f in enumerate(arpeggio)], 2.0))
    # E6-C6, E6-C6, then ~1 s of quiet.
    dong = []
    for t0 in (0.0, 0.6):
        dong += [note(1318.5, t0, 0.5, 0.20, 0.30), note(1046.5, t0 + 0.22, 0.6, 0.25, 0.30)]
    write("break_end.wav", render(dong, 2.3))
