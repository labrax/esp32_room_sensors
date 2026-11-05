#!/usr/bin/env python3
"""Generate a logarithmic 100 Hz - 10 kHz sweep WAV for HS-3 mic tests.

48 kHz / 16-bit mono, 10 s by default: playable from a phone or PC speaker.
Standard library only.

Example:
    python3 tools/gen_sweep.py
    python3 tools/gen_sweep.py --out /tmp/sweep.wav --duration 10
"""

import argparse
import math
import struct
import wave


def sweep_samples(rate, duration, f0, f1, amplitude):
    count = int(round(rate * duration))
    if count <= 0:
        raise SystemExit("error: duration too short")
    ratio = f1 / f0
    if ratio <= 1.0:
        raise SystemExit("error: --f1 must be greater than --f0")

    # Exponential sweep: f(t) = f0 * (f1/f0) ** (t/duration).
    k = duration / math.log(ratio)
    fade = max(1, int(0.02 * rate))  # 20 ms fades, no start/end click

    samples = []
    for i in range(count):
        t = i / rate
        phase = 2.0 * math.pi * f0 * k * (math.exp(t / k) - 1.0)
        gain = 1.0
        if i < fade:
            gain = 0.5 - 0.5 * math.cos(math.pi * i / fade)
        elif i >= count - fade:
            gain = 0.5 - 0.5 * math.cos(math.pi * (count - 1 - i) / fade)
        samples.append(amplitude * gain * math.sin(phase))
    return samples


def write_wav(path, samples, rate):
    data = bytearray()
    for sample in samples:
        value = int(round(sample * 32768.0))
        value = max(-32768, min(32767, value))
        data += struct.pack("<h", value)

    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(rate)
        wf.writeframes(bytes(data))


def zero_cross_rate(samples, rate):
    """Rough frequency estimate from zero crossings (for the summary only)."""
    if len(samples) < 2:
        return 0.0
    crossings = 0
    for a, b in zip(samples, samples[1:]):
        if (a < 0.0 <= b) or (a >= 0.0 > b):
            crossings += 1
    return crossings * rate / (2.0 * len(samples))


def verify(path, rate, duration, f0, f1):
    with wave.open(path, "rb") as wf:
        channels = wf.getnchannels()
        width = wf.getsampwidth()
        actual_rate = wf.getframerate()
        frames = wf.getnframes()
        raw = wf.readframes(frames)

    values = struct.unpack("<%dh" % (len(raw) // 2), raw)
    peak = max(abs(v) for v in values) / 32768.0
    window = int(0.1 * actual_rate)
    start_hz = zero_cross_rate([v / 32768.0 for v in values[:window]], actual_rate)
    end_hz = zero_cross_rate([v / 32768.0 for v in values[-window:]], actual_rate)
    seconds = frames / float(actual_rate)

    print(f"Wrote {path}")
    print(f"  {channels} channel, {width * 8}-bit, {actual_rate} Hz, "
          f"{frames} frames ({seconds:.2f} s)")
    print(f"  peak {20.0 * math.log10(max(peak, 1e-9)):.1f} dBFS, "
          f"start ~{start_hz:.0f} Hz, end ~{end_hz:.0f} Hz")

    ok = (channels == 1 and width == 2 and actual_rate == rate
          and abs(seconds - duration) < 0.01
          and abs(start_hz - f0) / f0 < 0.1
          and abs(end_hz - f1) / f1 < 0.1)
    print("  verify: " + ("OK" if ok else "MISMATCH"))
    return 0 if ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate a logarithmic sweep WAV for HS-3 mic tests.")
    parser.add_argument("--out", default="mic_sweep_100-10k_10s.wav",
                        help="output WAV (default: mic_sweep_100-10k_10s.wav)")
    parser.add_argument("--duration", type=float, default=10.0,
                        help="seconds (default: 10)")
    parser.add_argument("--rate", type=int, default=48000,
                        help="sample rate in Hz (default: 48000)")
    parser.add_argument("--f0", type=float, default=100.0,
                        help="start frequency in Hz (default: 100)")
    parser.add_argument("--f1", type=float, default=10000.0,
                        help="end frequency in Hz (default: 10000)")
    parser.add_argument("--amplitude", type=float, default=0.5,
                        help="peak amplitude 0..1 (default: 0.5)")
    args = parser.parse_args(argv)

    if not 0.0 < args.amplitude <= 0.99:
        parser.error("--amplitude must be in (0, 0.99]")

    samples = sweep_samples(args.rate, args.duration, args.f0, args.f1,
                            args.amplitude)
    write_wav(args.out, samples, args.rate)
    return verify(args.out, args.rate, args.duration, args.f0, args.f1)


if __name__ == "__main__":
    raise SystemExit(main())
