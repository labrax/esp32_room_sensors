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

- module out of the case (baseline),
- lid on with the 4 mm MIC1 hole open,
- lid on with the hole taped,
- existing case vents open vs taped,
- a 4 mm ID tube in the ~16 mm gap between the module port and the lid hole,
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
- 4 mm ID tube: printed `case/esp32_case_mic_tube.stl` (16 mm long) plus a
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
| SD CS | GPIO5 |
| Record button | GPIO13 -> 3V3 (active high) |
| Status LED | GPIO2 |

INMP441 VDD **3.3 V**, L/R to GND (left channel), common ground.

Flash and start the serial monitor:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 test/mic_case
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 test/mic_case
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Each recording is 10 s at 32 kHz / 16-bit mono. The sketch writes
`/mic_000.wav`, `/mic_001.wav`, ... and prints the file name, peak, and RMS.

## 4. Bench setup

1. Put the module on a fixed stand at the intended height inside the case.
   Do not move the stand between the no-lid and lid-on runs.
2. Place the speaker **on axis** with the MIC1 hole, **30 cm** away, at the same
   height. Mark the speaker position with tape on the bench.
3. Remove nearby reflectors (monitor, metal parts). Turn off HVAC and fans
   except the noise source.
4. Measure and record the actual gap between the INMP441 port and the lid
   inner face (expected ~16 mm).
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
| E | on | open | open | 16 mm | no | foam ring at module port, do not seal the port |
| F | on | open | open | 16 mm | yes | mesh over the lid hole, on top of the tube flange |
| G | on | open | open | no | yes | mesh only |
| S | on | open | open | optional | no | only if a side-vent prototype is already printed |

Additional checks:

- Confirm the module port is open and the tube does not cover or touch it.
- Record whether the tube is centred and whether the flange sits flat on the
  lid inner face.
- For D, tape the existing VACG top slots and the right-side slots; do not
  cover the MIC1 hole.

### Tube resonance note

A 4 mm ID, 16 mm tube has a quarter-wave pipe resonance near
`343 / (4 * 0.016) = 5.4 kHz` (and related modes). Treat any 5–6 kHz band
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
| Tube adds a 5–6 kHz peak/dip | Tube resonance | Shorten the tube (e.g. 10 mm) and repeat; do not adopt the 16 mm tube |
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
- The 16 mm tube can add its own resonance; report it separately from the case.
- Dust mesh attenuation depends on the material; record the exact mesh.
- Side vents are only tested if a prototype already exists.

## 12. Results

**Pending — no bench measurements have been performed.** Add the completed
run log, the comparison tables/CSV, and the decision here when the measurements
are done.
