#!/usr/bin/env python3
"""Synthesises Spark's factory sound library from scratch (no samples, so everything is royalty-free).

Writes Resources/Sounds/<id>.flac (mono, 44.1 kHz, 16-bit) and Resources/Sounds/manifest.json.
Run from the repo root:  python3 tools/factory_sounds/make_sounds.py
Needs numpy, scipy and ffmpeg.
"""
import json, os, subprocess, tempfile, wave
import numpy as np
from scipy import signal

SR = 44100
OUT = os.path.join("Resources", "Sounds")
rng = np.random.default_rng(2026)

def mtof(m): return 440.0 * 2 ** ((m - 69) / 12)
C3, C1 = 60, 36          # Spark's C3 = MIDI 60

def t_of(sec): return np.arange(int(sec * SR)) / SR

# ------------------------------------------------------------------ oscillators (band-limited, additive)
def phase_of(freq):
    """freq: scalar or array (Hz) -> running phase in cycles"""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (len(freq),) if np.ndim(freq) else (1,))
    return np.cumsum(f) / SR

def additive(freq, n, amps, phases=None):
    """sum of harmonics k=1.. with amplitude amps(k); freq may vary over time (array of length n)"""
    freq = np.full(n, freq) if np.isscalar(freq) else freq
    ph = np.cumsum(freq) / SR
    fmax = float(np.max(freq))
    out = np.zeros(n)
    k = 1
    while k * fmax < SR * 0.45:
        a = amps(k)
        if a != 0.0:
            p0 = 0.0 if phases is None else phases(k)
            out += a * np.sin(2 * np.pi * (k * ph + p0))
        k += 1
    return out

def saw(freq, n):    return additive(freq, n, lambda k: (1.0 / k) * (2 / np.pi) * (-1) ** (k + 1))
def square(freq, n): return additive(freq, n, lambda k: (4 / np.pi / k) if k % 2 else 0.0)
def tri(freq, n):    return additive(freq, n, lambda k: (8 / np.pi ** 2) * ((-1) ** ((k - 1) // 2)) / k ** 2 if k % 2 else 0.0)
def sine(freq, n):
    freq = np.full(n, freq) if np.isscalar(freq) else freq
    return np.sin(2 * np.pi * np.cumsum(freq) / SR)

def pulse(freq, n, width):
    return additive(freq, n, lambda k: (2 / (np.pi * k)) * np.sin(np.pi * k * width))

def supersaw(freq, n, voices=7, cents=20.0):
    out = np.zeros(n)
    for v in range(voices):
        d = cents * (2 * v / (voices - 1) - 1)
        f = freq * 2 ** (d / 1200)
        out += saw(f, n) * (1.0 if v == voices // 2 else 0.75)
    return out / voices * 1.6

def noise(n): return rng.uniform(-1, 1, n)

# ------------------------------------------------------------------ envelopes and filters
def adsr(n, a, d, s, r_start, r, curve=4.0):
    t = np.arange(n) / SR
    e = np.where(t < a, t / max(a, 1e-4), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4) * curve / 4))
    rel = t >= r_start
    level_at = s + (1 - s) * np.exp(-(r_start - a) / max(d, 1e-4) * curve / 4) if r_start > a else r_start / max(a, 1e-4)
    e[rel] = level_at * np.exp(-(t[rel] - r_start) / max(r, 1e-4) * 4.6)
    return e

def decay_env(n, tau, attack=0.002):
    t = np.arange(n) / SR
    return np.minimum(1.0, t / attack) * np.exp(-t / tau)

def tv_filter(x, cutoff, q=0.707, kind="low", block=64):
    """time-varying biquad (cutoff: scalar or array), processed in blocks carrying state"""
    n = len(x)
    cutoff = np.full(n, cutoff) if np.isscalar(cutoff) else cutoff
    y = np.zeros(n)
    zi = np.zeros(2)
    for i in range(0, n, block):
        fc = float(np.clip(cutoff[i], 20.0, SR * 0.45))
        if kind == "band":
            b, a = signal.iirpeak(fc, max(q, 0.3), fs=SR)
        else:
            b, a = signal.butter(2, fc, btype="low" if kind == "low" else "high", fs=SR)
            if q != 0.707 and kind == "low":
                # resonant low-pass from the RBJ cookbook
                w0 = 2 * np.pi * fc / SR
                alpha = np.sin(w0) / (2 * q)
                cw = np.cos(w0)
                b = np.array([(1 - cw) / 2, 1 - cw, (1 - cw) / 2])
                a = np.array([1 + alpha, -2 * cw, 1 - alpha])
                b, a = b / a[0], a / a[0]
        seg, zi = signal.lfilter(b, a, x[i:i + block], zi=zi * 1.0 if zi.shape == (2,) else np.zeros(2))
        y[i:i + block] = seg
    return y

def formant(x, formants):
    """parallel band-pass filters (freq, gain, q) - vowels"""
    out = np.zeros_like(x)
    for f, g, q in formants:
        b, a = signal.iirpeak(f, q, fs=SR)
        out += g * signal.lfilter(b, a, x)
    return out

VOWELS = {
    "a": [(730, 1.0, 6), (1090, 0.5, 8), (2440, 0.25, 12), (3400, 0.1, 14)],
    "o": [(570, 1.0, 6), (840, 0.45, 8), (2410, 0.15, 12), (3300, 0.06, 14)],
    "e": [(270, 1.0, 6), (2290, 0.45, 10), (3010, 0.3, 12), (3600, 0.12, 14)],
    "u": [(300, 1.0, 6), (870, 0.3, 8), (2240, 0.08, 12), (3200, 0.04, 14)],
}

def reverb(x, size=0.5, mix=0.25):
    """small Schroeder-ish reverb so pads and textures have some air"""
    out = x.copy()
    wet = np.zeros(len(x) + SR * 2)
    for d, g in [(0.0297, 0.77), (0.0371, 0.74), (0.0411, 0.72), (0.0437, 0.7)]:
        dl = int(d * SR * (0.6 + size))
        comb = np.zeros(len(wet))
        comb[:len(x)] = x
        for i in range(dl, len(comb), dl):
            comb[i:i + dl] += comb[i - dl:i][: len(comb[i:i + dl])] * g * (0.7 + 0.3 * size)
        wet += comb
    wet = wet[: len(x)] / 4
    return out * (1 - mix) + wet * mix

def saturate(x, drive): return np.tanh(x * drive) / np.tanh(drive)

def fade(x, fin=0.002, fout=0.02):
    n = len(x); a = int(fin * SR); b = int(fout * SR)
    if a > 0: x[:a] *= np.linspace(0, 1, a)
    if b > 0: x[-b:] *= np.linspace(1, 0, b)
    return x

def ks_pluck(freq, sec, bright=0.5, decay=0.996):
    """Karplus-Strong string"""
    n = int(sec * SR); period = int(SR / freq)
    buf = noise(period) * 0.8
    buf = tv_filter(buf, 1500 + 6000 * bright)
    out = np.zeros(n)
    for i in range(n):
        v = buf[i % period]
        out[i] = v
        buf[i % period] = decay * 0.5 * (v + buf[(i + 1) % period])
    return out

def partials(freq, sec, ratios, amps, taus):
    t = t_of(sec)
    out = np.zeros(len(t))
    for r, a, tau in zip(ratios, amps, taus):
        if freq * r < SR * 0.45:
            out += a * np.sin(2 * np.pi * freq * r * t + rng.uniform(0, 6.28)) * np.exp(-t / tau)
    return out

def fm(carrier, ratio, index, n, index_env=None):
    t = np.arange(n) / SR
    idx = index if index_env is None else index * index_env
    return np.sin(2 * np.pi * carrier * t + idx * np.sin(2 * np.pi * carrier * ratio * t))

# ------------------------------------------------------------------ the library
SOUNDS = []   # (id, name, category, root, table_frame, fn)
def sound(id_, name, cat, root=C3, table=0):
    def deco(fn):
        SOUNDS.append((id_, name, cat, root, table, fn)); return fn
    return deco

# ---- Bass (root C1)
@sound("bass_808_long", "808 Long", "Bass", C1)
def _():
    n = int(2.8 * SR); t = t_of(2.8); f0 = mtof(C1)
    f = f0 * (1 + 1.6 * np.exp(-t / 0.035))
    x = sine(f, n) * np.exp(-t / 1.1)
    x += 0.5 * np.exp(-t / 0.004) * noise(n) * 0.3
    return saturate(x * 1.4, 1.6)

@sound("bass_808_punch", "808 Punch", "Bass", C1)
def _():
    n = int(1.4 * SR); t = t_of(1.4); f0 = mtof(C1)
    f = f0 * (1 + 3 * np.exp(-t / 0.02))
    x = sine(f, n) * np.exp(-t / 0.45)
    return saturate(x * 2.5, 2.2)

@sound("bass_sub", "Sub Sine", "Bass", C1)
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C1)
    x = sine(f0, n) + 0.12 * sine(2 * f0, n)
    return x * adsr(n, 0.005, 0.4, 0.85, 1.8, 0.15)

@sound("bass_reese", "Reese", "Bass", C1)
def _():
    n = int(2.6 * SR); f0 = mtof(C1)
    x = saw(f0 * 2 ** (-12 / 1200), n) + saw(f0 * 2 ** (12 / 1200), n) + 0.6 * sine(f0, n)
    x = tv_filter(x, 900 + 300 * np.sin(np.linspace(0, 6, n)), 1.2)
    return x * adsr(n, 0.01, 0.5, 0.9, 2.3, 0.25)

@sound("bass_growl", "Growl", "Bass", C1)
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C1)
    x = fm(f0, 1.0, 3.5, n, 0.6 + 0.4 * np.sin(2 * np.pi * 3 * t)) + 0.5 * saw(f0, n)
    vow = np.clip((np.sin(2 * np.pi * 1.5 * t) + 1) / 2, 0, 1)
    y = (1 - vow) * formant(x, VOWELS["o"]) + vow * formant(x, VOWELS["a"])
    y = saturate(y * 3, 2.5) + 0.4 * sine(f0, n)
    return y * adsr(n, 0.005, 0.3, 0.9, 1.8, 0.15)

@sound("bass_wobble", "Wobble", "Bass", C1 - 12)   # its square sub sits an octave below
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C1)
    x = saw(f0, n) + saw(f0 * 1.006, n) + 0.5 * square(f0 / 2, n)
    lfo = 0.5 - 0.5 * np.cos(2 * np.pi * 4.13 * t)
    y = tv_filter(x, 150 + 2600 * lfo ** 2, 3.0)
    return saturate(y * 1.5, 1.5) * adsr(n, 0.005, 0.3, 0.95, 1.85, 0.1)

@sound("bass_pluck", "Pluck Bass", "Bass", C1)
def _():
    n = int(1.1 * SR); t = t_of(1.1); f0 = mtof(C1)
    x = saw(f0, n) + 0.6 * square(f0, n)
    y = tv_filter(x, 120 + 3500 * np.exp(-t / 0.08), 2.0)
    return y * decay_env(n, 0.35)

@sound("bass_analog", "Analog Bass", "Bass", C1)
def _():
    n = int(1.8 * SR); t = t_of(1.8); f0 = mtof(C1)
    x = saw(f0, n) + 0.5 * saw(f0 * 1.003, n) + 0.4 * sine(f0 / 2, n)
    y = tv_filter(x, 300 + 900 * np.exp(-t / 0.25), 1.4)
    return y * adsr(n, 0.004, 0.5, 0.75, 1.6, 0.18)

@sound("bass_fm", "FM Bass", "Bass", C1)
def _():
    n = int(1.2 * SR); t = t_of(1.2); f0 = mtof(C1)
    x = fm(f0, 1.0, 4.0, n, np.exp(-t / 0.12)) + 0.6 * sine(f0, n)
    return x * decay_env(n, 0.5)

@sound("bass_brass", "Brass Bass", "Bass", C1)
def _():
    n = int(1.8 * SR); t = t_of(1.8); f0 = mtof(C1)
    x = supersaw(f0, n, 5, 10)
    env_f = 200 + 1800 * (1 - np.exp(-t / 0.08)) * np.exp(-t / 0.6)
    y = tv_filter(x, env_f, 1.1)
    return y * adsr(n, 0.03, 0.6, 0.8, 1.5, 0.25)

@sound("bass_tape", "Tape Bass", "Bass", C1)
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C1)
    wow = f0 * (1 + 0.003 * np.sin(2 * np.pi * 0.7 * t))
    x = tri(wow, n) + 0.3 * saw(wow, n)
    y = tv_filter(saturate(x * 2.0, 2.0), 700, 0.9)
    return y * adsr(n, 0.008, 0.6, 0.8, 1.8, 0.18)

@sound("bass_neuro", "Neuro Bass", "Bass", C1)
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C1)
    x = saw(f0, n) + saw(f0 * 2.01, n) * 0.5 + square(f0 * 0.5, n) * 0.4
    sweep = 400 + 3000 * (0.5 + 0.5 * np.sin(2 * np.pi * 2.1 * t + 1))
    y = tv_filter(x, sweep, 5.0, "band")
    y = saturate(y * 4, 3) + 0.35 * sine(f0, n)
    return y * adsr(n, 0.004, 0.3, 0.95, 1.85, 0.1)

# ---- Pads (root C3)
def pad_env(n, sec): return adsr(n, 0.35, 1.0, 0.85, sec - 0.9, 0.8)

@sound("pad_supersaw", "Supersaw Pad", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); f0 = mtof(C3)
    x = supersaw(f0, n, 7, 22) + 0.4 * supersaw(f0 * 2, n, 5, 16)
    y = tv_filter(x, 4200, 0.8)
    return reverb(y * pad_env(n, sec), 0.7, 0.3)

@sound("pad_analog", "Warm Analog", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = saw(f0 * 2 ** (-6 / 1200), n) + saw(f0 * 2 ** (7 / 1200), n) + 0.5 * pulse(f0 / 2, n, 0.35)
    y = tv_filter(x, 900 + 400 * np.sin(2 * np.pi * 0.25 * t), 1.0)
    return reverb(y * pad_env(n, sec), 0.6, 0.25)

@sound("pad_glass", "Glass Pad", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = fm(f0, 3.5, 1.2, n, 0.6 + 0.4 * np.sin(2 * np.pi * 0.3 * t)) + 0.7 * sine(f0 * 2, n) + 0.4 * sine(f0 * 3.01, n)
    return reverb(x * pad_env(n, sec) * 0.6, 0.8, 0.35)

@sound("pad_strings", "String Ensemble", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = np.zeros(n)
    for k, d in enumerate([-9, -3, 4, 10]):
        vib = f0 * 2 ** (d / 1200) * (1 + 0.004 * np.sin(2 * np.pi * (5.1 + k * 0.3) * t + k))
        x += saw(vib, n)
    y = tv_filter(x / 3, 3200, 0.7)
    y = y - 0.8 * tv_filter(y, 180)
    return reverb(y * adsr(n, 0.5, 1.0, 0.9, sec - 0.9, 0.8), 0.7, 0.3)

@sound("pad_choir", "Choir Aah", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = np.zeros(n)
    for k, d in enumerate([-8, -2, 3, 9]):
        vib = f0 * 2 ** (d / 1200) * (1 + 0.005 * np.sin(2 * np.pi * (4.8 + 0.4 * k) * t + k))
        x += saw(vib, n)
    y = formant(x, VOWELS["a"]) + 0.06 * tv_filter(noise(n), 4000, 0.7, "band")
    return reverb(y * adsr(n, 0.4, 1.0, 0.9, sec - 0.9, 0.8), 0.8, 0.35)

@sound("pad_air", "Air Pad", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = 0.5 * sine(f0, n) + 0.35 * sine(f0 * 2, n) + 0.2 * sine(f0 * 3, n)
    air = tv_filter(noise(n), f0 * 4 * (1 + 0.3 * np.sin(2 * np.pi * 0.2 * t)), 8.0, "band") * 0.9
    return reverb((x + air) * pad_env(n, sec), 0.8, 0.35)

@sound("pad_dark", "Dark Drone", "Pads", C3 - 12)
def _():
    sec = 4.5; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3 - 12)
    x = saw(f0, n) + saw(f0 * 1.004, n) + saw(f0 * 1.498, n) * 0.5
    y = tv_filter(x, 260 + 180 * np.sin(2 * np.pi * 0.15 * t), 2.0)
    return reverb(y * adsr(n, 0.8, 1.0, 0.95, sec - 1.0, 0.9), 0.9, 0.35)

@sound("pad_organ", "Drawbar Organ", "Pads")
def _():
    sec = 3.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = sum(a * sine(f0 * r * (1 + 0.002 * np.sin(2 * np.pi * 6 * t)), n) for r, a in [(0.5, 0.5), (1, 1), (2, 0.7), (3, 0.4), (4, 0.3), (6, 0.15), (8, 0.1)])
    return x * adsr(n, 0.01, 0.2, 1.0, sec - 0.2, 0.08) * 0.4

@sound("pad_shimmer", "Shimmer", "Pads")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); f0 = mtof(C3)
    x = np.zeros(n)
    for r, a in [(1, 0.6), (2, 0.5), (4, 0.35), (6, 0.2), (8, 0.15)]:
        x += a * sine(f0 * r * (1 + 0.003 * np.sin(2 * np.pi * (0.3 + r * 0.07) * t)), n) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.2 * r * t))
    return reverb(x * pad_env(n, sec), 0.9, 0.45)

# ---- Keys & plucks
@sound("key_epiano", "Electric Piano", "Keys & Plucks")
def _():
    n = int(2.5 * SR); t = t_of(2.5); f0 = mtof(C3)
    x = fm(f0, 1.0, 1.8, n, np.exp(-t / 0.35)) + 0.35 * fm(f0 * 14, 1.0, 0.0, n) * np.exp(-t / 0.02)
    return x * decay_env(n, 1.1) * (0.9 + 0.1 * np.sin(2 * np.pi * 4 * t))

@sound("key_bell", "Bell", "Keys & Plucks")
def _():
    f0 = mtof(C3)
    return reverb(partials(f0, 3.5, [1, 2.0, 2.76, 5.4, 8.93, 11.8], [1, 0.6, 0.5, 0.3, 0.2, 0.12], [2.5, 1.6, 1.2, 0.6, 0.35, 0.2]), 0.7, 0.25)

@sound("key_marimba", "Marimba", "Keys & Plucks")
def _():
    f0 = mtof(C3)
    return partials(f0, 1.2, [1, 3.99, 10.7], [1, 0.35, 0.08], [0.45, 0.12, 0.03])

@sound("key_kalimba", "Kalimba", "Keys & Plucks")
def _():
    f0 = mtof(C3)
    x = partials(f0, 2.0, [1, 5.4, 11.2], [1, 0.25, 0.1], [0.9, 0.08, 0.03])
    x[: int(0.004 * SR)] += noise(int(0.004 * SR)) * 0.4
    return x

@sound("key_musicbox", "Music Box", "Keys & Plucks", C3 + 12)
def _():
    f0 = mtof(C3 + 12)
    return reverb(partials(f0, 2.5, [1, 2.0, 3.0, 4.2], [1, 0.3, 0.15, 0.1], [1.2, 0.6, 0.3, 0.15]), 0.6, 0.3)

@sound("key_harp", "Harp", "Keys & Plucks")
def _(): return reverb(ks_pluck(mtof(C3), 2.5, 0.4, 0.998), 0.6, 0.2)

@sound("key_guitar", "Nylon Pluck", "Keys & Plucks")
def _(): return ks_pluck(mtof(C3), 2.0, 0.7, 0.997)

@sound("key_pizz", "Pizzicato", "Keys & Plucks")
def _():
    x = ks_pluck(mtof(C3), 0.8, 0.3, 0.992)
    return reverb(tv_filter(x, 2500), 0.6, 0.25)

@sound("key_piano", "Soft Piano", "Keys & Plucks")
def _():
    f0 = mtof(C3); sec = 3.0
    ratios = [k * np.sqrt(1 + 0.0004 * k * k) for k in range(1, 14)]
    x = partials(f0, sec, ratios, [1 / k ** 1.3 for k in range(1, 14)], [2.2 / (1 + 0.35 * k) for k in range(1, 14)])
    x[: int(0.003 * SR)] += noise(int(0.003 * SR)) * 0.15
    return reverb(x, 0.5, 0.18)

@sound("key_stab", "Chord Stab", "Keys & Plucks")
def _():
    n = int(1.2 * SR); t = t_of(1.2)
    x = sum(saw(mtof(C3 + i), n) + 0.5 * square(mtof(C3 + i), n) for i in (0, 3, 7, 10))
    y = tv_filter(x, 500 + 4000 * np.exp(-t / 0.12), 1.3)
    return y * decay_env(n, 0.35) * 0.4

# ---- Leads
@sound("lead_saw", "Saw Lead", "Leads")
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C3)
    vib = f0 * (1 + 0.005 * np.sin(2 * np.pi * 5.5 * t) * np.clip((t - 0.4) * 2, 0, 1))
    x = saw(vib, n) + 0.6 * saw(vib * 1.005, n)
    return tv_filter(x, 5000, 0.9) * adsr(n, 0.01, 0.3, 0.9, 1.8, 0.15)

@sound("lead_square", "Square Lead", "Leads")
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C3)
    x = pulse(f0 * (1 + 0.004 * np.sin(2 * np.pi * 5 * t)), n, 0.5)
    return tv_filter(x, 2400, 0.8) * adsr(n, 0.01, 0.3, 0.9, 1.8, 0.15)

OS = 8   # oversampling for hard-edged waveforms, so they don't alias

def oversampled (fn, sec):
    """fn(t, n) evaluated at OS x the rate, then filtered down"""
    n = int(sec * SR * OS); t = np.arange(n) / (SR * OS)
    return signal.resample_poly (fn (t, n), 1, OS)

@sound("lead_sync", "Sync Lead", "Leads")
def _():
    f0 = mtof(C3)
    def gen(t, n):
        ratio = 1.5 + 2.5 * (0.5 + 0.5 * np.sin(2 * np.pi * 0.8 * t))
        master = np.cumsum(np.full(n, f0)) / (SR * OS)
        return 2 * ((master % 1.0) * ratio % 1.0) - 1
    x = tv_filter(oversampled(gen, 2.0), 7000)
    return x * adsr(len(x), 0.005, 0.3, 0.9, 1.8, 0.15) * 0.8

@sound("lead_chip", "Chip Pulse", "Leads")
def _():
    f0 = mtof(C3)
    def gen(t, n):
        ph = np.cumsum(np.full(n, f0)) / (SR * OS)
        return np.where((ph % 1.0) < 0.25, 1.0, -1.0) * 0.6
    x = oversampled(gen, 1.5)
    return x * adsr(len(x), 0.002, 0.2, 0.8, 1.35, 0.1)

@sound("lead_whistle", "Whistle", "Leads", C3 + 12)
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C3 + 12)
    x = sine(f0 * (1 + 0.006 * np.sin(2 * np.pi * 5.8 * t)), n) + 0.08 * tv_filter(noise(n), f0, 10, "band")
    return x * adsr(n, 0.06, 0.3, 0.9, 1.75, 0.2)

@sound("lead_flute", "Breath Flute", "Leads")
def _():
    n = int(2.2 * SR); t = t_of(2.2); f0 = mtof(C3)
    tone = sine(f0 * (1 + 0.004 * np.sin(2 * np.pi * 5 * t)), n) + 0.3 * sine(2 * f0, n) + 0.1 * sine(3 * f0, n)
    breath = tv_filter(noise(n), 2500, 1.5, "band") * 0.25
    return (tone + breath) * adsr(n, 0.08, 0.4, 0.85, 1.9, 0.2)

@sound("lead_scream", "Screamer", "Leads")
def _():
    n = int(2.0 * SR); t = t_of(2.0); f0 = mtof(C3)
    x = saw(f0, n) + saw(f0 * 1.01, n) + saw(f0 * 2.0, n) * 0.5
    y = saturate(tv_filter(x, 2500, 3.0) * 4, 3)
    return tv_filter(y, 6000) * adsr(n, 0.005, 0.2, 0.95, 1.85, 0.1) * 0.8

# ---- Vocal (synthesised formant voices)
def voice(vowel, sec, f0=mtof(C3), vib=True):
    n = int(sec * SR); t = t_of(sec)
    f = f0 * (1 + (0.007 * np.sin(2 * np.pi * 5.2 * t) * np.clip(t * 2, 0, 1) if vib else 0))
    glottal = additive(f, n, lambda k: 1.0 / k ** 1.1)
    y = formant(glottal, VOWELS[vowel]) + 0.04 * tv_filter(noise(n), 3500, 1.0, "band")
    return y * adsr(n, 0.05, 0.4, 0.9, sec - 0.25, 0.2)

@sound("vox_ah", "Vowel Ah", "Vocal")
def _(): return reverb(voice("a", 2.2), 0.5, 0.2)
@sound("vox_oh", "Vowel Oh", "Vocal")
def _(): return reverb(voice("o", 2.2), 0.5, 0.2)
@sound("vox_ee", "Vowel Ee", "Vocal")
def _(): return reverb(voice("e", 2.2), 0.5, 0.2)
@sound("vox_oo", "Vowel Oo", "Vocal")
def _(): return reverb(voice("u", 2.2), 0.5, 0.2)

@sound("vox_breath", "Breath", "Vocal")
def _():
    n = int(2.0 * SR)
    x = formant(noise(n), VOWELS["a"]) * 0.8
    return x * adsr(n, 0.25, 0.5, 0.7, 1.4, 0.5)

@sound("vox_phrase", "Vowel Phrase", "Vocal")
def _():
    parts = []
    for v, st in [("a", 0), ("o", 3), ("e", 7), ("a", 5), ("u", 0)]:
        seg = voice(v, 0.5, mtof(C3 + st), vib=False)
        parts.append(fade(seg, 0.01, 0.06))
    return reverb(np.concatenate(parts), 0.5, 0.2)

# ---- Textures
@sound("tex_wind", "Wind", "Textures")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec)
    gust = 0.5 + 0.5 * np.sin(2 * np.pi * 0.23 * t) * np.sin(2 * np.pi * 0.07 * t + 1)
    return tv_filter(noise(n), 300 + 1200 * gust, 4.0, "band") * (0.3 + 0.7 * gust) * adsr(n, 0.5, 1, 1, sec - 0.6, 0.5)

@sound("tex_rain", "Rain", "Textures")
def _():
    sec = 4.0; n = int(sec * SR)
    bed = tv_filter(noise(n), 5000, 0.7) * 0.15
    drops = np.zeros(n)
    for _ in range(900):
        i = rng.integers(0, n - 800); f = rng.uniform(1500, 6000)
        drops[i:i + 800] += np.sin(2 * np.pi * f * np.arange(800) / SR) * np.exp(-np.arange(800) / 90) * rng.uniform(0.05, 0.3)
    return fade(bed + drops, 0.3, 0.3)

@sound("tex_vinyl", "Vinyl Crackle", "Textures")
def _():
    sec = 4.0; n = int(sec * SR)
    hiss = tv_filter(noise(n), 4000, 0.7) * 0.06
    crackle = np.zeros(n)
    for _ in range(420):
        i = rng.integers(0, n - 60)
        crackle[i:i + 60] += noise(60) * np.exp(-np.arange(60) / 8) * rng.uniform(0.2, 1.0)
    return fade(tv_filter(hiss + crackle, 9000) + 0.02 * sine(60, n), 0.2, 0.2)

@sound("tex_static", "Radio Static", "Textures")
def _():
    sec = 3.5; n = int(sec * SR); t = t_of(sec)
    tuning = 800 + 2000 * (0.5 + 0.5 * np.sin(2 * np.pi * 0.4 * t))
    x = tv_filter(noise(n), tuning, 6.0, "band") + 0.4 * sine(tuning, n) * (np.sin(2 * np.pi * 7 * t) > 0.6)
    return fade(saturate(x, 1.5) * 0.7, 0.1, 0.2)

@sound("tex_metal", "Metal Drone", "Textures")
def _():
    f0 = mtof(C3)
    x = partials(f0, 4.0, [1, 1.47, 2.09, 2.56, 3.51, 4.66, 5.87], [1, 0.8, 0.6, 0.5, 0.4, 0.3, 0.2], [3, 2.5, 2, 1.5, 1.2, 1, 0.8])
    return reverb(fade(x * adsr(len(x), 0.4, 1, 1, 3.4, 0.5), 0.3, 0.3), 0.8, 0.4)

@sound("tex_hum", "Machine Hum", "Textures")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec)
    x = sum(a * sine(55 * k, n) for k, a in [(1, 1), (2, 0.5), (3, 0.35), (5, 0.2), (7, 0.1)])
    x += tv_filter(noise(n), 400, 0.7) * 0.4
    x *= 1 + 0.15 * np.sin(2 * np.pi * 1.3 * t)
    return fade(x * 0.5, 0.3, 0.3)

@sound("tex_underwater", "Underwater", "Textures")
def _():
    sec = 4.0; n = int(sec * SR)
    bed = tv_filter(noise(n), 300, 0.7) * 0.5
    bubbles = np.zeros(n)
    for _ in range(80):
        i = rng.integers(0, n - 4000); L = 4000; tt = np.arange(L) / SR
        f = rng.uniform(300, 900) * (1 + 3 * tt)
        bubbles[i:i + L] += np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt / 0.02) * 0.3
    return fade(tv_filter(bed + bubbles, 1200), 0.3, 0.3)

@sound("tex_insects", "Night Insects", "Textures")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec)
    x = np.zeros(n)
    for k in range(5):
        f = rng.uniform(3500, 6500); rate = rng.uniform(12, 30)
        chirp = (np.sin(2 * np.pi * rate * t + k) > 0.3).astype(float) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.3 * t + k))
        x += sine(f, n) * tv_filter(chirp, 200) * 0.25
    x += tv_filter(noise(n), 800, 0.7) * 0.05
    return fade(x, 0.3, 0.3)

@sound("tex_digital", "Digital Glitch", "Textures")
def _():
    sec = 2.5; n = int(sec * SR)
    x = np.zeros(n); i = 0
    while i < n:
        L = int(rng.uniform(0.02, 0.15) * SR); seg = np.arange(min(L, n - i)) / SR
        kind = rng.integers(0, 3); f = rng.uniform(100, 3000)
        if kind == 0: s = np.sign(np.sin(2 * np.pi * f * seg))
        elif kind == 1: s = noise(len(seg))
        else: s = np.sin(2 * np.pi * f * seg * (1 + 20 * seg))
        s = np.round(s * 4) / 4
        x[i:i + len(seg)] = s * rng.uniform(0.2, 0.8) * (rng.random() > 0.2)
        i += L
    return tv_filter(x, 9000) * 0.6

# ---- Drums & perc (root C3: play C3 for the original pitch)
@sound("drm_kick", "Kick", "Drums & Perc")
def _():
    n = int(0.7 * SR); t = t_of(0.7)
    f = 50 + 180 * np.exp(-t / 0.03)
    x = sine(f, n) * np.exp(-t / 0.25) + noise(n) * np.exp(-t / 0.003) * 0.4
    return saturate(x * 1.6, 1.5)

@sound("drm_kick_deep", "Deep Kick", "Drums & Perc")
def _():
    n = int(1.2 * SR); t = t_of(1.2)
    f = 42 + 150 * np.exp(-t / 0.04)
    return saturate(sine(f, n) * np.exp(-t / 0.5) * 1.8, 1.8)

@sound("drm_snare", "Snare", "Drums & Perc")
def _():
    n = int(0.6 * SR); t = t_of(0.6)
    body = sine(185 * (1 + 0.3 * np.exp(-t / 0.02)), n) * np.exp(-t / 0.08)
    wires = tv_filter(noise(n), 4500, 0.8) * np.exp(-t / 0.15)
    return reverb(body * 0.8 + wires * 0.9, 0.4, 0.15)

@sound("drm_clap", "Clap", "Drums & Perc")
def _():
    n = int(0.7 * SR); t = t_of(0.7); x = np.zeros(n)
    for k, d in enumerate([0.0, 0.011, 0.022, 0.035]):
        i = int(d * SR); L = n - i
        x[i:] += noise(L) * np.exp(-np.arange(L) / SR / (0.01 if k < 3 else 0.18))
    return reverb(tv_filter(x, 1400, 1.5, "band") * 2.2, 0.5, 0.25)

def metallic(sec, tau, hp=6000):
    n = int(sec * SR); t = t_of(sec)
    x = sum(np.sign(np.sin(2 * np.pi * f * t)) for f in [205.3, 304.4, 369.6, 522.7, 540, 800])
    return tv_filter(tv_filter(x, hp, 0.7, "high"), 12000) * np.exp(-t / tau) * 0.25

@sound("drm_hat_closed", "Closed Hat", "Drums & Perc")
def _(): return metallic(0.25, 0.035)
@sound("drm_hat_open", "Open Hat", "Drums & Perc")
def _(): return metallic(1.0, 0.3)

@sound("drm_tom", "Tom", "Drums & Perc")
def _():
    n = int(0.9 * SR); t = t_of(0.9)
    f = mtof(C3 - 12) * (1 + 0.5 * np.exp(-t / 0.05))
    return sine(f, n) * np.exp(-t / 0.3) + noise(n) * np.exp(-t / 0.005) * 0.2

@sound("drm_rim", "Rim", "Drums & Perc")
def _():
    n = int(0.25 * SR); t = t_of(0.25)
    return (sine(1700, n) * 0.6 + sine(500, n)) * np.exp(-t / 0.02) + noise(n) * np.exp(-t / 0.003) * 0.3

@sound("drm_shaker", "Shaker", "Drums & Perc")
def _():
    n = int(0.35 * SR); t = t_of(0.35)
    env = np.minimum(t / 0.04, 1) * np.exp(-np.maximum(t - 0.04, 0) / 0.06)
    return tv_filter(noise(n), 7000, 0.7, "high") * env

@sound("drm_cowbell", "Metal Bell", "Drums & Perc")
def _():
    n = int(0.8 * SR); t = t_of(0.8)
    x = np.sign(np.sin(2 * np.pi * 540 * t)) + np.sign(np.sin(2 * np.pi * 800 * t))
    return tv_filter(x, 900, 3, "band") * np.exp(-t / 0.18) * 0.7

@sound("drm_perc", "FM Perc", "Drums & Perc")
def _():
    n = int(0.5 * SR); t = t_of(0.5)
    return fm(mtof(C3), 2.4, 5, n, np.exp(-t / 0.03)) * np.exp(-t / 0.12)

@sound("drm_loop", "Drum Loop 124", "Drums & Perc")
def _():
    beat = 60 / 124; n = int(beat * 8 * SR); x = np.zeros(n)
    def put(sample, at, gain):
        i = int(at * SR); L = min(len(sample), n - i); x[i:i + L] += sample[:L] * gain
    kick = SOUND_CACHE["drm_kick"]; snare = SOUND_CACHE["drm_snare"]; hat = SOUND_CACHE["drm_hat_closed"]; openh = SOUND_CACHE["drm_hat_open"]
    for b in range(8):
        if b in (0, 2, 3, 4, 6): put(kick, b * beat + (0.25 * beat if b == 3 else 0), 0.9)
        if b in (1, 3, 5, 7): put(snare, b * beat, 0.7)
        for s in range(2): put(hat, b * beat + s * beat / 2, 0.35 if s else 0.5)
    put(openh, 7.5 * beat, 0.4)
    return x

def pattern_loop(pieces):
    beat = 60 / 124; n = int(beat * 8 * SR); x = np.zeros(n)
    for sample, at, gain in pieces:
        i = int(at * beat * SR); L = min(len(sample), n - i); x[i:i + L] += sample[:L] * gain
    return x

@sound("drm_hat_loop", "Hat Loop 124", "Drums & Perc")
def _():
    hat = SOUND_CACHE["drm_hat_closed"]; openh = SOUND_CACHE["drm_hat_open"]
    pieces = [(hat, s / 4, 0.8 if s % 2 == 0 else 0.45) for s in range(32) if s % 8 != 6]
    pieces += [(openh, b * 2 + 1.5, 0.5) for b in range(4)]
    return pattern_loop(pieces)

@sound("drm_shaker_loop", "Shaker Loop 124", "Drums & Perc")
def _():
    sh = SOUND_CACHE["drm_shaker"]
    return pattern_loop([(sh, s / 4, [0.9, 0.4, 0.6, 0.4][s % 4]) for s in range(32)])

# ---- FX & risers
@sound("fx_riser", "Riser", "FX & Risers")
def _():
    sec = 4.0; n = int(sec * SR); t = t_of(sec); u = t / sec
    x = tv_filter(noise(n), 300 * 30 ** u, 3.0, "band") + 0.4 * sine(mtof(C3) * 2 ** (u * 2), n)
    return x * u ** 1.5

@sound("fx_downlifter", "Downlifter", "FX & Risers")
def _():
    sec = 3.0; n = int(sec * SR); t = t_of(sec); u = t / sec
    x = tv_filter(noise(n), 8000 * 0.03 ** u, 2.0, "band") + 0.5 * sine(mtof(C3 + 12) * 2 ** (-u * 3), n)
    return x * (1 - u) ** 1.2

@sound("fx_impact", "Impact", "FX & Risers")
def _():
    n = int(3.0 * SR); t = t_of(3.0)
    boom = sine(35 + 80 * np.exp(-t / 0.08), n) * np.exp(-t / 0.9)
    crack = tv_filter(noise(n), 2000) * np.exp(-t / 0.05)
    return reverb(saturate(boom * 1.4 + crack, 1.5), 0.9, 0.3)

@sound("fx_laser", "Laser", "FX & Risers")
def _():
    n = int(0.8 * SR); t = t_of(0.8)
    f = mtof(C3 + 24) * np.exp(-t / 0.12) + mtof(C3 - 12)
    return pulse(f, n, 0.3) * np.exp(-t / 0.25) * 0.8

@sound("fx_sweep", "White Sweep", "FX & Risers")
def _():
    sec = 3.0; n = int(sec * SR); t = t_of(sec)
    swp = 200 * 60 ** (0.5 - 0.5 * np.cos(2 * np.pi * t / sec))
    return tv_filter(noise(n), swp, 4.0, "band") * np.sin(np.pi * t / sec)

@sound("fx_reverse", "Reverse Swell", "FX & Risers")
def _():
    f0 = mtof(C3)
    x = reverb(partials(f0, 2.5, [1, 2, 3, 4.01, 5.02], [1, 0.5, 0.35, 0.2, 0.1], [1.2, 0.8, 0.5, 0.35, 0.25]), 0.9, 0.5)
    return fade(x[::-1].copy(), 0.01, 0.01)

@sound("fx_siren", "Siren", "FX & Risers")
def _():
    n = int(3.0 * SR); t = t_of(3.0)
    f = mtof(C3) * 2 ** (np.sin(2 * np.pi * 0.7 * t) * 0.5)
    return tv_filter(saw(f, n), 3000) * adsr(n, 0.05, 0.5, 0.95, 2.7, 0.25)

# ---- Wavetables (64 frames x 2048 samples, root = MIDI 60 by convention)
def table(frames_fn, count=64):
    N = 2048; ph = np.arange(N) / N
    frames = [frames_fn(i / (count - 1), ph) for i in range(count)]
    out = []
    for f in frames:
        f = f - np.mean(f)
        f = f / (np.max(np.abs(f)) + 1e-9) * 0.9
        out.append(f)
    return np.concatenate(out)

def harm(ph, amps):
    return sum(a * np.sin(2 * np.pi * k * ph) for k, a in enumerate(amps, start=1) if a)

@sound("wt_basic", "Basic Shapes", "Wavetable", table=2048)
def _():
    def fr(u, ph):
        sinew = np.sin(2 * np.pi * ph)
        triw = harm(ph, [(8 / np.pi ** 2) * ((-1) ** ((k - 1) // 2)) / k ** 2 if k % 2 else 0 for k in range(1, 64)])
        saww = harm(ph, [(-1) ** (k + 1) / k for k in range(1, 256)])
        sq = harm(ph, [1 / k if k % 2 else 0 for k in range(1, 256)])
        seg = u * 3
        if seg < 1: return sinew * (1 - seg) + triw * seg
        if seg < 2: return triw * (2 - seg) + saww * (seg - 1)
        return saww * (3 - seg) + sq * (seg - 2)
    return table(fr)

@sound("wt_pwm", "Pulse Width", "Wavetable", table=2048)
def _(): return table(lambda u, ph: harm(ph, [np.sin(np.pi * k * (0.5 - 0.45 * u)) / k for k in range(1, 256)]))

@sound("wt_formant", "Formant Sweep", "Wavetable", table=2048)
def _():
    order = ["a", "o", "u", "e", "a"]
    def fr(u, ph):
        pos = u * (len(order) - 1); i = min(int(pos), len(order) - 2); w = pos - i
        A = dict((f, g) for f, g, q in VOWELS[order[i]]); B = dict((f, g) for f, g, q in VOWELS[order[i + 1]])
        f0 = 130.8
        amps = []
        for k in range(1, 180):
            fk = k * f0
            ga = sum(g / (1 + ((fk - f) / (f * 0.12)) ** 2) for f, g in A.items())
            gb = sum(g / (1 + ((fk - f) / (f * 0.12)) ** 2) for f, g in B.items())
            amps.append(((1 - w) * ga + w * gb) / k ** 0.3)
        return harm(ph, amps)
    return table(fr)

@sound("wt_harmonic", "Harmonic Stack", "Wavetable", table=2048)
def _(): return table(lambda u, ph: harm(ph, [1 / k if k <= 1 + u * 40 else 0 for k in range(1, 64)]))

@sound("wt_fm", "FM Sweep", "Wavetable", table=2048)
def _(): return table(lambda u, ph: np.sin(2 * np.pi * ph + (u * 6) * np.sin(2 * np.pi * 2 * ph)))

@sound("wt_fold", "Wavefolder", "Wavetable", table=2048)
def _(): return table(lambda u, ph: np.sin((1 + u * 7) * np.sin(2 * np.pi * ph)))

@sound("wt_digital", "Digital Steps", "Wavetable", table=2048)
def _():
    def fr(u, ph):
        steps = int(64 - u * 58)
        x = np.sin(2 * np.pi * ph) + 0.5 * np.sin(2 * np.pi * 3 * ph + u * 3)
        return np.round(x * steps / 4) / (steps / 4)
    return table(fr)

@sound("wt_soft", "Soft Saws", "Wavetable", table=2048)
def _(): return table(lambda u, ph: harm(ph, [np.exp(-k / (2 + u * 30)) / k for k in range(1, 128)]))

# ------------------------------------------------------------------ render
SOUND_CACHE = {}

def normalise(x, cat, is_table):
    x = np.asarray(x, dtype=float)
    x = x - np.mean(x) if is_table else x
    peak = np.max(np.abs(x)) + 1e-9
    if is_table:
        return x / peak * 0.9
    # loudness-ish: bring RMS of the loud part to a per-category target, never above -1 dBFS peak
    w = int(0.3 * SR)
    rms = max(np.sqrt(np.mean(x[i:i + w] ** 2)) for i in range(0, max(1, len(x) - w), w // 2)) + 1e-9
    if cat == "Textures":
        rms = np.sqrt(np.mean(x ** 2)) + 1e-9   # sparse by nature (crackle, insects): match the average, not the loudest moment
    target = {"Drums & Perc": 0.35, "FX & Risers": 0.22, "Textures": 0.18}.get(cat, 0.22)
    g = min(target / rms, 0.89 / peak)
    return x * g

def write_flac(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        with wave.open(tmp.name, "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", tmp.name, "-compression_level", "12", path], check=True)
        os.unlink(tmp.name)

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".flac"): os.unlink(os.path.join(OUT, f))
    manifest = []
    # drums first so the loop can reuse them
    order = sorted(SOUNDS, key=lambda s: (s[0] in ("drm_loop", "drm_hat_loop", "drm_shaker_loop"), 0))
    for id_, name, cat, root, tbl, fn in order:
        x = fn()
        x = normalise(x, cat, tbl > 0)
        if not tbl: x = fade(x, 0.001, 0.01)
        SOUND_CACHE[id_] = x
        write_flac(os.path.join(OUT, id_ + ".flac"), x)
        manifest.append({"id": id_, "name": name, "category": cat, "root": root, "table": tbl, "seconds": round(len(x) / SR, 2)})
        print(f"{id_:18s} {name:18s} {cat:14s} {len(x) / SR:5.2f} s")
    cats = ["Bass", "Pads", "Keys & Plucks", "Leads", "Vocal", "Textures", "Drums & Perc", "FX & Risers", "Wavetable"]
    manifest.sort(key=lambda m: (cats.index(m["category"]), m["name"]))
    with open(os.path.join(OUT, "manifest.json"), "w") as f:
        json.dump({"sounds": manifest}, f, indent=1)
    size = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
    print(f"{len(manifest)} sounds, {size / 1e6:.1f} MB")

if __name__ == "__main__":
    main()
