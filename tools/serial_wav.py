#!/usr/bin/env python3
"""Capture the HS-3 INMP441 serial stream to a WAV file.

The test sketch (`test/mic_case_serial`) waits for 'S', then sends:
  b"HSA1" + <uint32 sample_rate><uint16 channels><uint16 bits>
  + <uint32 total_samples> + raw little-endian PCM
It prints "HSA_READY" every 2 s while idle.

Example:
  python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/A1.wav
  python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/A --count 3
"""

import argparse
import array
import math
import os
import struct
import sys
import time
import wave

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: python3 -m pip install pyserial")

MAGIC = b"HSA1"
READY = b"HSA_READY"


class Reader:
    def __init__(self, ser, chunk=4096):
        self.ser = ser
        self.chunk = chunk
        self.buf = b""

    def _fill(self):
        data = self.ser.read(self.chunk)
        if data:
            self.buf += data
        return bool(data)

    def wait_for(self, needle, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            idx = self.buf.find(needle)
            if idx >= 0:
                self.buf = self.buf[idx + len(needle):]
                return True
            self._fill()
        return False

    def read_exact(self, n, timeout):
        deadline = time.time() + timeout
        while len(self.buf) < n and time.time() < deadline:
            self._fill()
        if len(self.buf) < n:
            raise TimeoutError(f"wanted {n} bytes, got {len(self.buf)}")
        data = self.buf[:n]
        self.buf = self.buf[n:]
        return data


def level(data, channels, bits):
    if bits != 16:
        return None, None
    samples = array.array("h")
    samples.frombytes(data)
    if sys.byteorder == "big":
        samples.byteswap()
    if channels > 1:
        samples = samples[::channels]
    if not samples:
        return None, None
    peak = max(abs(s) for s in samples)
    rms = math.sqrt(sum(float(s) * s for s in samples) / len(samples))
    peak_db = 20.0 * math.log10(peak / 32768.0) if peak else -120.0
    rms_db = 20.0 * math.log10(rms / 32768.0) if rms else -120.0
    return peak_db, rms_db


def output_name(base, index, count):
    if count == 1:
        return base
    root, ext = os.path.splitext(base)
    if not ext:
        ext = ".wav"
    return f"{root}_{index:03d}{ext}"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--out", default="mic.wav")
    ap.add_argument("--count", type=int, default=1)
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="seconds to wait for a header or its data")
    ap.add_argument("--ready-timeout", type=float, default=10.0,
                    help="seconds to wait for HSA_READY before starting anyway")
    ap.add_argument("--start-char", default="S")
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
    except serial.SerialException as exc:
        sys.exit(f"cannot open {args.port}: {exc}")

    reader = Reader(ser)
    print(f"Opened {args.port} at {args.baud} baud")
    print("Waiting for HSA_READY...")
    if not reader.wait_for(READY, args.ready_timeout):
        print("No HSA_READY seen; sending start anyway (reset the board if needed)")

    for index in range(args.count):
        print(f"\n[{index + 1}/{args.count}] Sending start...")
        ser.write(args.start_char.encode("ascii"))
        ser.flush()

        if not reader.wait_for(MAGIC, args.timeout):
            sys.exit("no HSA1 header received")

        header = reader.read_exact(12, args.timeout)
        sample_rate, channels, bits, total = struct.unpack("<IHHI", header)
        if not (1000 <= sample_rate <= 192000 and channels in (1, 2)
                and bits in (8, 16, 24, 32)):
            sys.exit(f"unexpected header: rate={sample_rate} ch={channels} bits={bits}")

        data_bytes = total * channels * (bits // 8)
        print(f"Recording {total} samples, {sample_rate} Hz, {channels} ch, "
              f"{bits}-bit — start playback now")
        data = reader.read_exact(data_bytes, args.timeout)

        out = output_name(args.out, index, args.count)
        parent = os.path.dirname(os.path.abspath(out))
        os.makedirs(parent, exist_ok=True)
        with wave.open(out, "wb") as wav:
            wav.setnchannels(channels)
            wav.setsampwidth(bits // 8)
            wav.setframerate(sample_rate)
            wav.writeframes(data)

        peak_db, rms_db = level(data, channels, bits)
        if peak_db is None:
            print(f"Saved {out} ({len(data)} bytes)")
        else:
            print(f"Saved {out}: peak {peak_db:.1f} dBFS, RMS {rms_db:.1f} dBFS")

        if index + 1 < args.count:
            print("Waiting for the next HSA_READY...")
            if not reader.wait_for(READY, args.ready_timeout):
                print("No HSA_READY seen; sending next start anyway")

    ser.close()


if __name__ == "__main__":
    main()
