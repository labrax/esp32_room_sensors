#!/usr/bin/env python3
"""Compare mono WAV recordings for the HS-3 microphone/case test.

Outputs overall RMS/peak levels and 1/3-octave band levels relative to a
baseline file, as a Markdown table, plus an optional long-format CSV.

Only the Python standard library is required (no numpy).

Example:
    python3 tools/mic_compare.py --baseline baseline.wav lid_open.wav lid_tape.wav

Self-test (synthetic sine, noise, and a -6 dB copy):
    python3 tools/mic_compare.py --self-test
"""

import argparse
import math
import os
import struct
import sys
import tempfile
import wave

# ISO 1/3-octave band centres (Hz), 50 Hz .. 10 kHz.
BAND_CENTERS = [
    50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
    800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000,
]

# 1/24-octave centres for the optional CSV spectrum dump.
SPECTRUM_CENTERS = [
    f for f in (50 * (2 ** (i / 24.0)) for i in range(0, 201)) if f <= 16000
]

EPS = 1e-20


def dbfs(value):
    """Amplitude ratio -> dBFS, floored to keep tables readable."""
    return 20.0 * math.log10(max(value, EPS))


# ---------------------------------------------------------------- WAV input

def read_wav(path):
    """Return (samples as floats in [-1, 1], sample rate, bits, duration)."""
    try:
        with wave.open(path, "rb") as wf:
            channels = wf.getnchannels()
            width = wf.getsampwidth()
            rate = wf.getframerate()
            frames = wf.getnframes()
            raw = wf.readframes(frames)
    except (wave.Error, OSError) as exc:
        raise SystemExit(f"error: cannot read {path}: {exc}")

    if channels != 1:
        raise SystemExit(f"error: {path} is not mono ({channels} channels)")
    if width not in (2, 4):
        raise SystemExit(
            f"error: {path} has {width * 8}-bit samples; only 16-bit and "
            "32-bit mono PCM are supported"
        )

    if width == 2:
        count = len(raw) // 2
        values = struct.unpack("<%dh" % count, raw[: count * 2])
        samples = [v / 32768.0 for v in values]
    else:
        count = len(raw) // 4
        values = struct.unpack("<%di" % count, raw[: count * 4])
        samples = [v / 2147483648.0 for v in values]

    duration = count / float(rate) if rate else 0.0
    return samples, rate, width * 8, duration


# --------------------------------------------------------------------- FFT

def _fft(values):
    """In-place iterative radix-2 FFT of a complex list (length is 2**k)."""
    n = len(values)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            values[i], values[j] = values[j], values[i]

    length = 2
    while length <= n:
        angle = -2.0 * math.pi / length
        wlen = complex(math.cos(angle), math.sin(angle))
        half = length >> 1
        for start in range(0, n, length):
            w = 1 + 0j
            for k in range(start, start + half):
                u = values[k]
                v = values[k + half] * w
                values[k] = u + v
                values[k + half] = u - v
                w *= wlen
        length <<= 1


def welch_psd(samples, rate, segment=8192):
    """One-sided Welch PSD. Returns (psd, bin_hz); sum(psd)*bin_hz = mean sq."""
    n = len(samples)
    if n < 64:
        raise SystemExit("error: recording too short for spectral analysis")

    seg = 1 << (min(segment, n).bit_length() - 1)
    seg = max(64, min(seg, 8192))
    if seg > n:
        seg = 1 << (n.bit_length() - 1)

    window = [0.5 - 0.5 * math.cos(2.0 * math.pi * i / (seg - 1))
              for i in range(seg)]
    window_energy = sum(w * w for w in window)
    bin_hz = rate / float(seg)
    psd = [0.0] * (seg // 2 + 1)
    segments = 0

    start = 0
    while start + seg <= n:
        block = [complex(samples[start + i] * window[i], 0.0)
                 for i in range(seg)]
        _fft(block)

        for k in range(seg // 2 + 1):
            power = (block[k].real ** 2 + block[k].imag ** 2) / (
                rate * window_energy
            )
            if 0 < k < seg // 2:
                power *= 2.0
            psd[k] += power

        segments += 1
        start += seg

    if not segments:
        raise SystemExit("error: recording too short for spectral analysis")
    psd = [p / segments for p in psd]
    return psd, bin_hz


def band_level(psd, bin_hz, center):
    """RMS level (dBFS) in a 1/3-octave band around center."""
    lo = center / (2.0 ** (1.0 / 6.0))
    hi = center * (2.0 ** (1.0 / 6.0))
    power = 0.0
    for k, value in enumerate(psd):
        freq = k * bin_hz
        if lo <= freq < hi:
            power += value * bin_hz
    return dbfs(math.sqrt(power))


def octave_level(psd, bin_hz, center, fraction=24):
    lo = center / (2.0 ** (1.0 / (2.0 * fraction)))
    hi = center * (2.0 ** (1.0 / (2.0 * fraction)))
    power = 0.0
    for k, value in enumerate(psd):
        freq = k * bin_hz
        if lo <= freq < hi:
            power += value * bin_hz
    return dbfs(math.sqrt(power))


# ----------------------------------------------------------------- analysis

def analyze(path):
    samples, rate, bits, duration = read_wav(path)
    if not samples:
        raise SystemExit(f"error: {path} contains no samples")

    sum_sq = 0.0
    peak = 0.0
    for s in samples:
        sum_sq += s * s
        if abs(s) > peak:
            peak = abs(s)

    rms = math.sqrt(sum_sq / len(samples))
    psd, bin_hz = welch_psd(samples, rate)

    bands = {}
    for center in BAND_CENTERS:
        if center * 1.15 > rate / 2.0:
            continue
        bands[center] = band_level(psd, bin_hz, center)

    spectrum = {}
    for center in SPECTRUM_CENTERS:
        if center * 1.03 > rate / 2.0:
            continue
        spectrum[center] = octave_level(psd, bin_hz, center)

    return {
        "path": path,
        "label": os.path.basename(path),
        "rate": rate,
        "bits": bits,
        "duration": duration,
        "rms": dbfs(rms),
        "peak": dbfs(peak),
        "bands": bands,
        "spectrum": spectrum,
    }


# ------------------------------------------------------------------- output

def fmt(value):
    return f"{value:+.1f}" if value > 0 else f"{value:.1f}"


def render_markdown(results, baseline_idx):
    base = results[baseline_idx]
    lines = []
    lines.append("# HS-3 microphone comparison")
    lines.append("")
    lines.append(f"Baseline: `{base['path']}`")
    lines.append("")
    lines.append("## Overall level")
    lines.append("")
    header = "| File | Rate (Hz) | Bits | Duration (s) | RMS (dBFS) | Peak (dBFS) | Δ RMS (dB) | Δ Peak (dB) |"
    lines.append(header)
    lines.append("|---|---|---|---|---|---|---|---|")
    for i, r in enumerate(results):
        drms = "" if i == baseline_idx else f"{r['rms'] - base['rms']:+.1f}"
        dpeak = "" if i == baseline_idx else f"{r['peak'] - base['peak']:+.1f}"
        lines.append(
            f"| `{r['label']}` | {r['rate']} | {r['bits']} | {r['duration']:.2f} "
            f"| {r['rms']:.1f} | {r['peak']:.1f} | {drms} | {dpeak} |"
        )

    lines.append("")
    lines.append("## 1/3-octave band levels (dBFS, Δ vs baseline)")
    lines.append("")
    header = "| Band (Hz) | " + " | ".join(f"`{r['label']}`" for r in results) + " |"
    lines.append(header)
    lines.append("|---" * (len(results) + 1) + "|")

    available = [b for b in BAND_CENTERS if b in base["bands"]]
    for band in available:
        cells = []
        for i, r in enumerate(results):
            level = r["bands"].get(band)
            if level is None:
                cells.append("—")
            elif i == baseline_idx:
                cells.append(f"{level:.1f}")
            else:
                cells.append(f"{level:.1f} ({level - base['bands'][band]:+.1f})")
        lines.append(f"| {band:g} | " + " | ".join(cells) + " |")

    lines.append("")
    lines.append("Δ is relative to the baseline. Uncalibrated levels are only "
                 "comparable between files recorded with the same gain and "
                 "speaker position.")
    return "\n".join(lines)


def write_csv(path, results, baseline_idx):
    base = results[baseline_idx]
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("file,kind,freq_hz,level_dbfs,delta_db\n")
        for i, r in enumerate(results):
            rms_delta = "" if i == baseline_idx else f"{r['rms'] - base['rms']:.3f}"
            peak_delta = "" if i == baseline_idx else f"{r['peak'] - base['peak']:.3f}"
            fh.write(f"{r['label']},overall_rms,,{r['rms']:.3f},{rms_delta}\n")
            fh.write(f"{r['label']},overall_peak,,{r['peak']:.3f},{peak_delta}\n")
            for band, level in r["bands"].items():
                delta = ("" if i == baseline_idx or band not in base["bands"]
                         else f"{level - base['bands'][band]:.3f}")
                fh.write(f"{r['label']},third_octave,{band:g},{level:.3f},{delta}\n")
            for center, level in r["spectrum"].items():
                delta = ("" if i == baseline_idx or center not in base["spectrum"]
                         else f"{level - base['spectrum'][center]:.3f}")
                fh.write(f"{r['label']},spectrum_24th,{center:.1f},{level:.3f},{delta}\n")


# ----------------------------------------------------------------- self test

def write_wav(path, samples, rate, sampwidth=2):
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(sampwidth)
        wf.setframerate(rate)
        if sampwidth == 2:
            data = bytearray()
            for s in samples:
                v = int(round(s * 32768.0))
                v = max(-32768, min(32767, v))
                data += struct.pack("<h", v)
        else:
            data = bytearray()
            for s in samples:
                v = int(round(s * 2147483648.0))
                v = max(-2147483648, min(2147483647, v))
                data += struct.pack("<i", v)
        wf.writeframes(bytes(data))


def self_test():
    rate = 16000
    seconds = 2.0
    count = int(rate * seconds)
    opts = []

    sine = [0.5 * math.sin(2.0 * math.pi * 1000.0 * i / rate)
            for i in range(count)]
    attenuated = [s * 0.5 for s in sine]  # -6.02 dB
    noise = []
    state = 12345
    for _ in range(count):
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        noise.append(((state / 1073741824.0) - 1.0) * 0.1732)  # ~-20 dBFS

    tempo = tempfile.mkdtemp(prefix="mic_compare_selftest_")
    paths = {}
    for label, data, width in (
        ("sine16", sine, 2),
        ("sine32", sine, 4),
        ("sine_minus6", attenuated, 2),
        ("noise16", noise, 2),
    ):
        paths[label] = os.path.join(tempo, label + ".wav")
        write_wav(paths[label], data, rate, width)

    results = [analyze(paths[k]) for k in ("sine16", "sine32", "sine_minus6", "noise16")]
    by_name = {r["label"][:-4]: r for r in results}

    failures = []

    def check(name, condition, detail):
        print(f"  {'PASS' if condition else 'FAIL'}  {name}: {detail}")
        if not condition:
            failures.append(name)

    print("Self-test (synthetic mono WAVs):")
    sine_rms = by_name["sine16"]["rms"]
    sine_peak = by_name["sine16"]["peak"]
    check("sine RMS", abs(sine_rms + 9.03) < 0.3, f"{sine_rms:.2f} dBFS (want -9.03)")
    check("sine peak", abs(sine_peak + 6.02) < 0.3, f"{sine_peak:.2f} dBFS (want -6.02)")

    band_1k = by_name["sine16"]["bands"].get(1000)
    check("sine peak band", band_1k is not None and band_1k == max(by_name["sine16"]["bands"].values()),
          f"1 kHz band {band_1k:.2f} dBFS is the maximum")

    delta = by_name["sine_minus6"]["rms"] - sine_rms
    check("-6 dB copy", abs(delta + 6.02) < 0.2, f"Δ RMS {delta:+.2f} dB")

    format_delta = by_name["sine32"]["rms"] - sine_rms
    check("32-bit read", abs(format_delta) < 0.05, f"Δ RMS {format_delta:+.3f} dB")

    noise_rms = by_name["noise16"]["rms"]
    check("noise RMS", abs(noise_rms + 20.0) < 0.6, f"{noise_rms:.2f} dBFS (want ~-20)")

    bands = by_name["noise16"]["bands"]
    # White-noise band power grows with bandwidth, so subtract the 10*log10
    # bandwidth to test for a flat FFT spectrum (+3 dB/octave tilt is expected).
    flat = [bands[b] - 10.0 * math.log10(b * (2 ** (1 / 6.0) - 2 ** (-1 / 6.0)))
            for b in (500, 1000, 2000, 4000) if b in bands]
    flat_spread = max(flat) - min(flat)
    check("noise spectrum is flat", flat_spread < 1.5,
          f"bandwidth-normalised spread {flat_spread:.1f} dB")

    markdown = render_markdown(results, 0)
    check("markdown output", "HS-3 microphone comparison" in markdown and "third" not in markdown.lower(),
          f"{len(markdown.splitlines())} lines")

    for path in paths.values():
        os.unlink(path)
    os.rmdir(tempo)

    if failures:
        print(f"FAILED: {len(failures)} check(s)")
        return 1
    print("PASSED: all checks")
    return 0


# --------------------------------------------------------------------- main

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Compare HS-3 mono WAV recordings against a baseline.",
        epilog="Example: python3 tools/mic_compare.py --baseline baseline.wav "
               "lid_open.wav lid_tape.wav",
    )
    parser.add_argument("wavs", nargs="*", help="WAV files to compare")
    parser.add_argument("--baseline", help="baseline WAV (default: first file)")
    parser.add_argument("--csv", help="write band levels and deltas to this CSV")
    parser.add_argument("--self-test", action="store_true",
                        help="run synthetic self-tests and exit")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    if not args.wavs:
        parser.error("no WAV files given (or use --self-test)")

    for path in args.wavs:
        if not os.path.isfile(path):
            parser.error(f"file not found: {path}")

    if args.baseline:
        if args.baseline not in args.wavs:
            args.wavs = [args.baseline] + args.wavs
        baseline_idx = args.wavs.index(args.baseline)
    else:
        baseline_idx = 0

    results = [analyze(path) for path in args.wavs]
    print(render_markdown(results, baseline_idx))

    if args.csv:
        write_csv(args.csv, results, baseline_idx)
        print(f"\nCSV written to {args.csv}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
