#!/usr/bin/env python3
"""Synthesizes Rockfield's sound effects.

Writes each sound as a .wav (to listen to) and a .vsnd (Vircon32's
native format: "V32-VSND", uint32 sample count, then 16-bit stereo
samples at 44100 Hz), into the sounds/ folder. The order of SOUNDS
is the order they must appear in the ROM definition XML.
"""
import os
import struct
import wave
import numpy as np

RATE = 44100
rng = np.random.default_rng(7)


def t_axis(seconds):
    return np.arange(int(RATE * seconds)) / RATE


def envelope(n, attack=0.002, release=None, curve=3.0):
    """Short attack, then exponential-ish decay to zero."""
    t = np.linspace(0, 1, n)
    env = (1 - t) ** curve
    a = int(attack * RATE)
    if a > 0:
        env[:a] *= np.linspace(0, 1, a)
    return env


def lowpass(x, cutoff):
    """One-pole low-pass; cutoff may be a scalar or per-sample array."""
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=float), x.shape)
    alpha = 1 - np.exp(-2 * np.pi * cutoff / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc += alpha[i] * (x[i] - acc)
        y[i] = acc
    return y


def sweep(f0, f1, seconds, shape="square", exp=True):
    t = t_axis(seconds)
    if exp:
        freq = f0 * (f1 / f0) ** (t / seconds)
    else:
        freq = f0 + (f1 - f0) * t / seconds
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    if shape == "square":
        return np.sign(np.sin(phase))
    if shape == "saw":
        return 2 * ((phase / (2 * np.pi)) % 1.0) - 1
    if shape == "tri":
        return 2 * np.abs(2 * ((phase / (2 * np.pi)) % 1.0) - 1) - 1
    return np.sin(phase)


def normalize(x, peak):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


# ---------------------------------------------------------------- sounds

def fire():
    s = sweep(1500, 280, 0.13, "square") * 0.6 + sweep(1500, 280, 0.13, "tri") * 0.4
    return normalize(lowpass(s, 6000) * envelope(len(s), curve=1.5), 0.45)


def bang(seconds, cutoff_start, cutoff_end, peak):
    n = int(RATE * seconds)
    noise = rng.uniform(-1, 1, n)
    cut = np.geomspace(cutoff_start, cutoff_end, n)
    s = lowpass(lowpass(noise, cut), cut)
    return normalize(s * envelope(n, curve=2.2), peak)


def bang_large():
    return bang(1.0, 1400, 120, 0.85)


def bang_medium():
    return bang(0.65, 2200, 200, 0.7)


def bang_small():
    return bang(0.38, 3500, 400, 0.55)


def ship_explode():
    n = int(RATE * 1.6)
    noise = bang(1.6, 2500, 80, 1.0)
    rumble = sweep(90, 30, 1.6, "sine") * envelope(n, curve=1.5)
    crackle = (rng.uniform(0, 1, n) > 0.997) * rng.uniform(-1, 1, n)
    crackle = lowpass(crackle, 4000) * envelope(n, curve=1.0) * 6
    return normalize(noise + rumble * 0.6 + crackle, 0.9)


def thrust():
    # seamless loop: low rumbling noise, cross-faded end into start
    seconds = 0.6
    n = int(RATE * seconds)
    fade = int(RATE * 0.05)
    noise = rng.uniform(-1, 1, n + fade)
    s = lowpass(lowpass(noise, 500), 350)
    body = s[:n].copy()
    ramp = np.linspace(0, 1, fade)
    body[:fade] = body[:fade] * ramp + s[n:] * (1 - ramp)
    return normalize(body, 0.4)


def saucer(f_low, f_high, rate_hz, seconds, peak):
    # warbling siren; whole periods so it loops cleanly
    t = t_axis(seconds)
    freq = f_low + (f_high - f_low) * (0.5 + 0.5 * np.sin(2 * np.pi * rate_hz * t))
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    s = np.sign(np.sin(phase)) * 0.5 + np.sin(phase) * 0.5
    return normalize(lowpass(s, 3000), peak)


def saucer_big():
    return saucer(380, 520, 4.0, 0.5, 0.28)


def saucer_small():
    return saucer(700, 950, 8.0, 0.5, 0.25)


def saucer_fire():
    s = sweep(2200, 900, 0.09, "square")
    return normalize(lowpass(s, 7000) * envelope(len(s), curve=1.2), 0.35)


def notes(freqs, note_len, shape="square", peak=0.4, tail=1.0):
    out = []
    for f in freqs:
        s = sweep(f, f, note_len, shape)
        out.append(s * envelope(len(s), curve=tail))
    s = np.concatenate(out)
    return normalize(lowpass(s, 5000), peak)


def extra_life():
    return notes([784, 988, 1175, 1568, 1175, 1568], 0.07, "square", 0.35, 0.6)


def beat(freq):
    s = sweep(freq, freq * 0.7, 0.11, "square")
    return normalize(lowpass(s, 600) * envelope(len(s), curve=2.0), 0.55)


def beat_low():
    return beat(55)


def beat_high():
    return beat(65)


def hyperspace():
    a = sweep(200, 2400, 0.25, "tri")
    b = sweep(2400, 300, 0.3, "tri")
    s = np.concatenate([a * np.linspace(0.3, 1, len(a)), b * envelope(len(b), curve=1.5)])
    return normalize(s, 0.4)


def start_game():
    return notes([523, 659, 784, 1047], 0.09, "square", 0.4, 0.4)


def wave_clear():
    return notes([659, 880, 1319], 0.08, "tri", 0.4, 0.5)


def pause():
    return notes([880, 660], 0.06, "square", 0.3, 0.8)


SOUNDS = [
    ("fire", fire),
    ("bang_large", bang_large),
    ("bang_medium", bang_medium),
    ("bang_small", bang_small),
    ("ship_explode", ship_explode),
    ("thrust", thrust),
    ("saucer_big", saucer_big),
    ("saucer_small", saucer_small),
    ("saucer_fire", saucer_fire),
    ("extra_life", extra_life),
    ("beat_low", beat_low),
    ("beat_high", beat_high),
    ("hyperspace", hyperspace),
    ("start_game", start_game),
    ("wave_clear", wave_clear),
    ("pause", pause),
]


def write(name, samples, outdir):
    pcm = np.clip(samples * 32767, -32768, 32767).astype("<i2")
    stereo = np.repeat(pcm, 2)

    with wave.open(os.path.join(outdir, name + ".wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(stereo.tobytes())

    with open(os.path.join(outdir, name + ".vsnd"), "wb") as f:
        f.write(b"V32-VSND")
        f.write(struct.pack("<I", len(pcm)))
        f.write(stereo.tobytes())


if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sounds")
    os.makedirs(outdir, exist_ok=True)
    for i, (name, fn) in enumerate(SOUNDS):
        s = fn()
        write(name, s, outdir)
        print(f"{i:2d} {name:14s} {len(s) / RATE:.2f}s")
