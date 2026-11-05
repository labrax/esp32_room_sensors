# HS-3: INMP441 microphone with the case lid — bench protocol

- **Status:** prepared; **no physical measurements have been taken yet.**
- **Ticket:** HS-3 (measure the INMP441 with and without the case lid)
- **Related:** HS-1 (MIC1 opening), HS-2 (acoustic path open question),
  `case/README.md`, `test/mic_case/README.md`

This document is the runnable protocol and the data-recording template. The
decision (leave as-is / gasket-tube / side vents / mesh) is made only after the
runs below are complete. Do not fill in results from estimates.

## 1. Goal and scope

Determine whether the closed case changes the microphone level or frequency
response in a way that matters for voice/level detection. Compare these
configurations with the same microphone, speaker, and positions:

- lid off, PCB/module left in the base (baseline),
- lid on with the 4 mm MIC1 hole open,
- lid on with the hole taped,
- existing case vents open vs taped,
- a 4 mm ID tube in the ~8.6 mm gap between the module port and the lid hole (12.4 mm module top),
- acoustic dust mesh over the hole,
- a side-vent prototype only if one is already printed.

Out of scope: absolute SPL calibration, firmware architecture changes, PCB
changes, production case redesign, and implementing side vents.

## 2. Equipment

- ESP32 board (the project board or a bare ESP32 dev board).
- INMP441 I2S microphone module, port facing up.
- microSD card, FAT32, 32 GB or smaller, plus a card reader.
- USB cable and a 5 V USB source (same source for every run).
- Speaker with a flat-ish response and fixed volume, or a phone/PC output
  through the same speaker. A small full-range speaker is enough.
- Player for WAV files (phone or PC), with the volume locked.
- Ruler/tape measure, tape, marker, tripod or fixed supports.
- 4 mm ID tube: printed `case/esp32_case_mic_tube.stl` (8.6 mm long) plus a
  soft foam ring at the module port.
- Acoustic dust mesh (thin polyester/metal mesh), tape.
- Calipers (record the actual module-to-lid gap).
- Optional: SPL meter or phone SPL app — for repeatability only, not for
  absolute calibration.

## 3. Wiring and firmware

Follow `test/mic_case/README.md`:

| Signal | ESP32 |
|---|---|
| I2S SCK | GPIO14 |
| I2S WS | GPIO15 |
| I2S SD (mic data) | GPIO4 |
| SD CS | GPIO5 (SD path only; unused in serial mode) |
| Record button | GPIO13 -> 3V3 (active high) |
| Status LED | GPIO2 |

INMP441 VDD **3.3 V**, common ground. L/R is board-dependent: the project board ties it high, so the test sketches record the **right** channel. If your module ties L/R low, change `ONLY_RIGHT` to `ONLY_LEFT` in the sketch.

Flash and start the serial monitor:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 test/mic_case
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 test/mic_case
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Each recording is 10 s at 32 kHz / 16-bit mono. The SD sketch writes
`/mic_000.wav`, `/mic_001.wav`, ... and prints the file name, peak, and RMS.
See `test/mic_case/README.md`.

### Serial capture (no SD)

If no SD card is available, use the independent serial sketch instead:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 test/mic_case_serial
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 test/mic_case_serial

# Host capture, 921600 baud:
python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/hs3_serial/A1.wav
# three repeats without changing the setup:
python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/hs3_serial/A --count 3
```

The sketch prints `HSA_READY` every 2 s while idle. The host sends `S`; the
device replies with a header and 10 s of PCM. The script waits for a 1 s
pre-roll and prints `Recording ... start playback now`; start the sweep at that
line. No SD card is used. The serial link must stay connected, and no text is
printed during the PCM stream. UART is adequate for this relative A/B test;
SD is more robust for long logs. See `test/mic_case_serial/README.md`.

## 4. Bench setup

1. Put the module on a fixed stand at the intended height inside the case.
   Do not move the stand between the no-lid and lid-on runs.
2. Place the speaker **on axis** with the MIC1 hole, **30 cm** away, at the same
   height. Mark the speaker position with tape on the bench.
3. Remove nearby reflectors (monitor, metal parts). Turn off HVAC and fans
   except the noise source.
4. Measure and record the actual gap between the INMP441 port and the lid
   inner face (expected 8.6 mm = 21.0 − 12.4).
5. Lock the player volume and never change it during the set. Check the
   recorder peak: `-30 .. -12 dBFS` is a good target; never clip.
6. Tighten the lid screws with the same torque/feel every time. Record any
   change in lid seating.

For the no-lid baseline, keep the PCB and module in the same position and
remove only the lid.

## 5. Test signals

| Signal | Source | Purpose |
|---|---|---|
| Sweep | `python3 tools/gen_sweep.py --out sweep_100-10k_10s.wav` | 100 Hz–10 kHz log sweep, 10 s, 48 kHz/16-bit mono |
| Voice | the same spoken sentence for every run ("The quick brown fox jumps over the lazy dog", counted 1–20) | speech-band level and coloration |
| Noise | steady broadband noise (fan, or a noise WAV played through the same speaker) | broadband level and resonances |

Start the player and the recording in the same order for every run (press the
button, then start playback immediately). Keep that order identical across all
configurations and repeats. A few hundred milliseconds at the sweep ends may
be clipped; that is acceptable because only baseline-relative levels are used,
but it must be consistent.

## 6. Configuration matrix

Run **three repeats** per configuration and signal.

| ID | Lid | MIC1 hole | Case vents (VACG) | Tube | Mesh | Notes |
|---|---|---|---|---|---|---|
| A | off | — | — | no | no | baseline, module in place |
| B | on | open | open | no | no | current case as built |
| C | on | taped | open | no | no | tape the 4 mm hole |
| D | on | open | taped | no | no | tape all top + right-side VACG vents |
| E | on | open | open | 8.6 mm | no | foam ring at module port, do not seal the port |
| F | on | open | open | 8.6 mm | yes | mesh over the lid hole, on top of the tube flange |
| G | on | open | open | no | yes | mesh only |
| S | on | open | open | optional | no | only if a side-vent prototype is already printed |

Additional checks:

- Confirm the module port is open and the tube does not cover or touch it.
- Record whether the tube is centred and whether the flange sits flat on the
  lid inner face.
- For D, tape the existing VACG top slots and the right-side slots; do not
  cover the MIC1 hole.

### Tube resonance note

A 4 mm ID, 8.6 mm tube has a quarter-wave pipe resonance near
`343 / (4 * 0.0086) = 10.0 kHz` (and related modes). Treat any 9–10 kHz band
change as a possible tube artifact, not automatically as a case effect. Report
it; a shorter or wider bore is the follow-up if it matters.

## 7. File naming and labels

Use `{config}{repeat}_{signal}.wav`, for example:

```
A1_sweep.wav  A2_sweep.wav  A3_sweep.wav
B1_sweep.wav  B2_sweep.wav  B3_sweep.wav
A1_voice.wav  B1_voice.wav  ...
A1_noise.wav  B1_noise.wav  ...
```

Copy files off the card after each set. In the run log, record the original
`/mic_NNN.wav` name next to the label so the data can be traced back.

## 8. Analysis

Per signal, compare all configurations against the no-lid baseline:

```sh
python3 tools/mic_compare.py --baseline A1_sweep.wav \
  B1_sweep.wav C1_sweep.wav D1_sweep.wav E1_sweep.wav F1_sweep.wav G1_sweep.wav \
  --csv sweep_compare.csv
```

Repeat-consistency check within one configuration:

```sh
python3 tools/mic_compare.py --baseline A1_sweep.wav A2_sweep.wav A3_sweep.wav
```

The script prints overall RMS/peak and 1/3-octave band levels with Δ vs
baseline. `--csv` also writes the 1/3-octave bands and a 1/24-octave FFT
spectrum for finding narrow resonances/comb patterns.

Validate the tool once before the bench session:

```sh
python3 tools/mic_compare.py --self-test
```

## 9. Run log template

Copy this table into the ticket or a results note.

### Session metadata

| Field | Value |
|---|---|
| Date | |
| Operator | |
| Board / firmware commit | |
| Module (INMP441 variant) | |
| Module port height / measured lid gap | |
| Speaker model and distance | |
| Player volume setting | |
| Room / background noise | |
| Lid screw torque / feel | |
| Tube part (`mic_tube_len`) | |
| Mesh material | |

### Measurements

| Config | Repeat | Signal | File | Peak dBFS | RMS dBFS | Speech-band Δ vs A | 1/3-oct max Δ | Notes |
|---|---|---|---|---|---|---|---|---|
| A | 1 | sweep | | | | 0 | 0 | |
| A | 1 | voice | | | | 0 | 0 | |
| A | 1 | noise | | | | 0 | 0 | |
| B | 1 | sweep | | | | | | |
| ... | | | | | | | | |

Fill the speech-band Δ from the 1/3-octave bands 315–3150 Hz, and the maximum
band Δ from the same output.

## 10. Decision criteria

Use these as the agreed thresholds; report the actual numbers either way.
A change is "meaningful" only if it is larger than the repeat-to-repeat spread.

| Observation | Interpretation | Action |
|---|---|---|
| All bands within ±3 dB of A; no band beyond ±4 dB | Case is acoustically transparent enough | Leave as-is; record in `case/README.md` and HS-2 |
| 3–6 dB loss in speech bands, no narrow notch, tube recovers to within 2 dB of A | Cavity/port loss dominates | Adopt gasket/tube; update case docs and create a part-change ticket |
| Narrow dip/peak > 6 dB at any 300–3400 Hz band | Coloration/resonance (case cavity or tube) | Try tube; if it persists, test vents; record the resonance frequency |
| Loss only with vents taped, vents open close to baseline | Cavity pressure/leakage | Keep vents open; no new part |
| Mesh costs > 3 dB above 4 kHz | Mesh too resistive | Reject that mesh or use a thinner acoustic mesh |
| Tube adds a 9–10 kHz peak/dip | Tube resonance | Shorten the tube (e.g. 8.0 mm) and repeat; do not adopt the 8.6 mm tube |
| Three repeats differ by > 1 dB RMS or > 2 dB in a band | Setup instability | Repeat the set; fix the stand/speaker/volume first |

If no configuration recovers the level, record the result and open a follow-up
for a port/side-vent design; do not modify the production case in HS-3.

## 11. Risks and limitations

- Uncalibrated speaker and room: results are **relative**, not absolute SPL.
- Speaker response is not flat; baseline subtraction removes most of it, but
  not room modes or directivity changes.
- Lid screw torque and seating change the cavity; use the same assembly and
  record any deviation.
- INMP441 module variants and port orientation differ; confirm the top-facing
  port is unobstructed before every set.
- The 8.6 mm tube can add its own resonance (~10 kHz); report it separately from the case.
- Dust mesh attenuation depends on the material; record the exact mesh.
- Side vents are only tested if a prototype already exists.

## 12. Results

### A/B sweep set — 2026-11-04 (serial capture)

Files: `/tmp/opencode/hs3_serial/A1.wav` … `B3.wav`. Three repeats per
configuration; 32 kHz/16-bit mono; 100 Hz–10 kHz sweep. Session metadata
(speaker, volume, measured gap, operator) still to be filled in section 9.

| Config | Run | RMS dBFS | Peak dBFS |
|---|---|---|---|
| A — lid off | 1 | −61.2 | −45.6 |
| A — lid off | 2 | −61.0 | −45.9 |
| A — lid off | 3 | −60.7 | −45.4 |
| B — lid on | 1 | −57.9 | −38.6 |
| B — lid on | 2 | −59.9 | −43.2 |
| B — lid on | 3 | −59.7 | −42.6 |

- Overall B is **+1.4 to +3.3 dB** relative to A: the case is not broadly
  muffling.
- Speech-band average B−A = **+0.6 dB**, but the range is **−12.7 to +7.0 dB**.
- 1/3-octave: 800 Hz **+7.0 dB**, 1.25 kHz **−12.7 dB**, 2 kHz **+3.6 dB**,
  630 Hz **+4.2 dB**.
- 1/24-octave: deep dips at **1.17–1.47 kHz** (−12 to −19.5 dB; worst
  1307 Hz −19.5 dB) and peaks at **0.6–0.98, 1.96, 2.77–2.85 kHz**
  (+11 to +13.8 dB). This is a comb/resonance, not a level loss.
- B1's <200 Hz bands are an outlier (+15–20 dB; B2/B3 +3–8 dB). Check the run
  log for handling/wind/lid seating; the 1.25 kHz notch is present in all B
  runs.
- A repeats are tight (0.3–0.6 dB RMS).

**Decision:** the >6 dB narrow notch in 300–3400 Hz triggers the “try tube; if
it persists, test vents” row in section 10. The current case is usable for
level detection but not flat for voice/ASR; **not leave-as-is**. Next: C (hole
taped), E (tube), then D (vents taped) if needed; add voice/noise sets.

Analysis: `/tmp/opencode/hs3_serial/compare.md` and `compare.csv`.

### C — MIC1 hole sealed (Blu-Tack), vents open — 2026-11-04

Files: `/tmp/opencode/hs3_serial/C1.wav` … `C3.wav`; same sweep and setup.

| Config | Run | RMS dBFS | Peak dBFS |
|---|---|---|---|
| C — hole sealed | 1 | −59.8 | −42.5 |
| C — hole sealed | 2 | −58.8 | −41.3 |
| C — hole sealed | 3 | −59.8 | −42.7 |

- Overall C is **+1.4 to +2.4 dB** vs A — similar to B; not muffled.
- **1.25 kHz notch persists:** C1 **−10.2 dB**, C2 **−6.0 dB**, C3 **−12.3 dB**
  (B: −10.7 to −13.1 dB). Sealing the hole does not remove the comb.
- C loses **400 Hz** (−2.8 to −3.9 dB) and **5–6.3 kHz** (−3.5 to −6.5 dB)
  relative to B: the 4 mm hole contributes a direct low-mid/HF path.
- 630/800 Hz and 2/2.5 kHz peaks are similar to B (+3 to +7 dB).
- C1/C2/C3 speech-band average is roughly +0.4 to +2.3 dB, with the notch.

**C conclusion:** the 4 mm hole is **not** the primary cause of the 1.25 kHz
comb; it persists with the hole sealed, so the case cavity/vents (or the
module/port mounting) dominate. Next: **D** (vents taped, hole open) and **E**
(tube) — the tube is the decisive test that bypasses the case cavity.

Analysis: `/tmp/opencode/hs3_serial/compare_abc.md` and `compare_abc.csv`.

### D — VACG lid + side vents taped, MIC1 hole open — 2026-11-04

Files: `/tmp/opencode/hs3_serial/D1.wav` … `D3.wav`; same sweep and setup.

| Config | Run | RMS dBFS | Peak dBFS |
|---|---|---|---|
| D — vents taped | 1 | −59.3 | −41.0 |
| D — vents taped | 2 | −59.1 | −41.1 |
| D — vents taped | 3 | −59.2 | −40.8 |

- Overall D is **+2.0 to +2.2 dB** vs A; the most repeatable set so far
  (0.2 dB RMS spread).
- **1.25 kHz notch persists:** D1 **−12.4 dB**, D2 **−11.2 dB**, D3 **−10.7 dB**
  (B: −10.7 to −13.1; C: −6.0 to −12.3). Taping the vents does not remove it.
- 5–6.3 kHz matches B more than C (D 6.3 kHz −78.6/−81.0/−80.6 dBFS; B
  −79.8/−81.5/−87.7; C −86.7/−88.9/−88.3), confirming the MIC1 hole was open
  for D.
- 630/800 Hz and 2 kHz peaks remain (+5 to +9 dB); <200 Hz +3–9 dB (case
  cavity, not the VACG vents).

**D conclusion:** the VACG lid/side vents are **not** the cause of the comb.
Together with C (hole sealed) this leaves the case cavity/port geometry or the
module/port mounting. **E (tube) is the decisive test.** Adding side vents is
not supported by D; taping the existing side vents did not flatten the notch.

Analysis: `/tmp/opencode/hs3_serial/compare_abcd.md` and `compare_abcd.csv`.

### 1.25 kHz notch — cavity depth mode

The 1/24-octave data puts the notch at **~1.17–1.47 kHz** (deepest 1.307 kHz)
with a harmonic at **2.614 kHz**. The case inner depth is **67 mm**:

- quarter-wave: `343 / (4 × 0.067) = 1.28 kHz`
- half-wave: `343 / (2 × 0.067) = 2.56 kHz`

The measured 1.31/2.61 kHz matches the depth mode. The width (78 mm) gives
1.10/2.20 kHz, and the height (21 mm) gives 4.08/8.17 kHz; the 3.84/5.12 kHz
depth harmonics and the 4.08 kHz height mode line up with the 4–5 kHz features.
A (lid off) is clean; C (hole sealed) and D (vents taped) do not remove the
notch, so this is the enclosed lid cavity depth mode — not the 4 mm hole and
not the VACG vents.

An 8.6 mm tube from the mic port to the lid hole bypasses this cavity. A no-print
equivalent is a **4 mm ID aquarium/silicone tube** (8.6 mm, foam ring at the
port). If that is unavailable, any of these work for the first E check:

- a **drinking straw** (≈5 mm ID), cut to ≈8.6 mm; butt its top against the
  inside of the 4 mm lid hole and seal with foam/tape;
- a **rolled card/paper tube**, 4–5 mm ID × 8.6 mm, taped;
- a **Blu-Tack tube** moulded around a 4 mm mandrel (pen refill/drill bit),
  mandrel removed; keep it off the 0.25 mm mic port.

**Open-cell foam** filling the lid (clear of the hole and port) is the other
damping test. The printed `mic_tube` is not required for the first E check.

### E — 8.6 mm tube, plus a null — 2026-11-04

Files: `/tmp/opencode/hs3_serial/E1.wav` … `E3.wav`, and `null.wav` (tube fitted,
no playback). **Session note:** A–D were recorded in a noisier part of the day;
null and E were recorded late at night. A–D vs E is therefore a cross-session
comparison; the 1.25 kHz notch and the 8–10 kHz feature are the robust results,
the overall level is less certain.

| Config | Run | RMS dBFS | Peak dBFS |
|---|---|---|---|
| E — tube | 1 | −60.7 | −43.7 |
| E — tube | 2 | −60.8 | −43.7 |
| E — tube | 3 | −60.9 | −43.8 |
| null — no playback | 1 | −74.9 | −59.7 |

- E is **+0.4 to +0.6 dB vs A** — the tube restores the overall level
  (B/D were +1.4 to +3.3). The null is 13.7 dB below A and ~14 dB below E.
- **1.25 kHz notch:** E1 **−8.7**, E2 **−7.0**, E3 **−7.1 dB**. The tube cuts
  the notch by roughly 3–6 dB versus B/D, but does **not** remove it.
- Speech-band average is **+0.0 to +0.5 dB**; 400 Hz is −4.5 to −5.1 dB;
  630/800 Hz and 2 kHz peaks remain.
- **New 8–10 kHz resonance:** E 8 kHz ≈ −80 dBFS (A −98; B −94 to −98), and
  E2/E3 10 kHz ≈ −79/−81 dBFS. The null at 8–10 kHz is ≈ −98 dBFS, so this is
  the tube, not the background. Consistent with the 8.6 mm tube’s ~10 kHz
  quarter-wave.
- Null interpretation: above ~315 Hz E is signal-dominated; at 50–250 Hz the
  E/null difference is only 1–9 dB, so the low-frequency E data is unreliable.

**Decision (2026-11-04): stop the experiment; use the 8.6 mm tube.** The tube
was a real improvement — overall level back to A and the 1.25 kHz notch reduced
— but a residual −7 dB notch remains and the tube adds an 8–10 kHz resonance.
For background-noise recording (trains/road/snoring) that is acceptable; the
1.25 kHz notch is not acceptable for high-quality voice/ASR. The standard lid
now carries the same 8.6 mm tube integrated into the lid; the separate
`esp32_case_mic_tube.stl` is a test/alternative part, not to be used with the
integrated lid. F (tube + mesh) and the 8.0 mm tube were **not run**; they
remain documented options if the voice use case becomes important. The residual
1.25 kHz may be the module/port or the remaining cavity.
