# Bring-up test plan

Staged power-on and test procedure for the assembled IoT_SYNTHPATCH (main board + analog board).
Each stage only adds one thing, so when something fails you know where to look. Don't go to the next
stage until the current one passes.

Firmware for every stage: [`firmware/bringup`](../firmware/bringup) (one image, serial test shell).
Pin and channel facts used below come from the schematics in [`hardware/`](../hardware), see
[`firmware/bringup/include/pins.h`](../firmware/bringup/include/pins.h).

**Tools:** multimeter (ideally 4½ digit for V/oct), current-limited bench supply or a Eurorack case
with a ±12 V "smoke stopper", USB-C cable (data), USB power meter, oscilloscope if you have one,
a patch cable or two, MIDI keyboard + TRS-A adapter, any VCO or a phone headphone output for the audio input.

**Record evidence as you go.** Photos and short clips of the passing stages are your backup video
for the course demo (see [iot-project-plan.md](iot-project-plan.md#demo-and-video-shot-list)).

---

## Power jumpers (important)

| Jumper | What it does | USB only (stage 1) | Eurorack power (stage 2 on) |
|---|---|---|---|
| **J19** | USB-C LDO U1 → +3.3 V rail | **ON** | **OFF** |
| **J24** | +12 V → 3.3 V buck IC1 | **OFF** | **ON** |

Never fit both: the LDO and the buck would both drive +3.3 V. With USB only, there is no +5 V,
+5A or ±12 V, so the display, DACs, 74HCT125 and op-amps are unpowered. That is expected.

SW1 is the EN (reset) slide switch and SW2 the BOOT slide switch. Leave both in the "run" position
except when flashing for the first time.

---

## Stage 0: before any power

| # | Check | Pass |
|---|---|---|
| 0.1 | Visual check under magnification: pin 1 of every IC (IC1–IC17, U1, U2, Q9), diode direction (D1, D2, D11, D14, D15, BAT54S), polarity of C83/C85 (alu caps), LEDs. | All correct |
| 0.2 | Solder bridges on fine pitch parts: ESP32-S3 module (incl. GND pad), PCM1808 (TSSOP-14), DAC124S085 / DAC8562 (MSOP-10), TPS54331 (SO-8). | No bridges |
| 0.3 | Resistance to GND on **each board separately** (analog board unplugged): +12, −12, +5, +5A, +3.3, VREF_5. Measure both polarities, write the values down. | No rail below ~100 Ω; nothing near 0 Ω |
| 0.4 | +12 to −12 and +12 to +5 not shorted. | > 1 kΩ |
| 0.5 | Eurorack power header J31: red stripe = −12 V (pins 1–2). Mark it on the PCB. | Orientation marked |
| 0.6 | Mechanical: H1/H2 (2x20) and H3 (1x14) line up, the 11 mm stack fits, jacks J4–J7 don't hit the header sockets. | Boards mate without force |

## Stage 1: USB only, main board alone (J19 ON, J24 OFF)

| # | Do | Pass |
|---|---|---|
| 1.1 | Plug USB-C through the USB power meter. | DS2 (+3.3 V) on, current < 100 mA, +3.3 V = 3.27–3.35 V |
| 1.2 | PC sees the ESP32-S3 USB device (Windows: "USB JTAG/serial debug unit", VID 303A). | Device appears |
| 1.3 | Flash: `pio run -t upload -d firmware/bringup`. If it doesn't connect: SW2 BOOT on, toggle SW1 EN, flash, SW2 back, toggle SW1. | Upload OK |
| 1.4 | `pio device monitor`, then `info`. | ESP32-S3, flash 16 MB, PSRAM ≈ 2048 KB |
| 1.5 | Status LED DS3 blinks (heartbeat). `led on`, `led off`, `led blink`. | DS3 follows |
| 1.6 | SHT45 on J21 (STEMMA QT cable). `i2c`, then `sht watch`. Breathe on the sensor. | Device at 0x44; room temp ±1 °C of a reference; RH jumps when you breathe |
| 1.7 | Encoder: `enc`. Turn both ways, press. | ±1 per detent, no skipped/double counts, button DOWN/up |
| 1.8 | MIDI in (J20, TRS type A): `midi`. Play notes, move a CC. | Correct note names and velocities. If nothing: try a type B adapter |

**Evidence:** screenshot of `info` + `sht` output, photo of the board on USB.

## Stage 2: Eurorack power, main board alone (J19 OFF, J24 ON)

Use a current limit of ~150 mA per rail for the first power-on.

| # | Do | Pass |
|---|---|---|
| 2.1 | Power on. Watch the current. | No current limit hit; DS1 (+5 V) and DS2 (+3.3 V) on |
| 2.2 | Measure at the header sockets (main board): +12 (H1-1), −12 (H1-3), +5 (H1-35), +3.3 (H1-39), VREF_5 (H1-4). Measure +5A at C91. | ±12 V ±5 %; +5 V 4.9–5.1; +3.3 V 3.27–3.35; +5A 4.95–5.05; VREF_5 4.975–5.025 |
| 2.3 | Note the idle current on +12 and −12. | Write it down (needed for the power budget doc) |
| 2.4 | Flash and connect over USB as in stage 1 (USB data still works with J19 off). | Shell responds |
| 2.5 | Display: `tft test`, `bl 40`, `bl 255`. | Colour fills, bars, full white border, text readable; backlight dims |
| 2.6 | micro-SD in the display slot: `sd`. | Card size and file list |
| 2.7 | V/oct DAC: `voct init`. Measure VREF at tp8. | 2.495–2.505 V |
| 2.8 | Raw DAC outputs (op-amps are still on the unplugged analog board), measured on the main board H1 sockets: `cvcode J10 2048` → H1-24 ≈ 2.50 V; `cvcode J10 4095` → ≈ 5.00 V. Repeat for each jack (J10 H1-24, J12 H1-25, J14 H1-26, J16 H1-27, J11 H1-28, J13 H1-29, J15 H1-30, J17 H1-31). | Each pin moves; the others stay put |
| 2.9 | `voct a 5` (0..10 V scale) → H1-32 ≈ 2.50 V, `voct b 5` → H1-33 ≈ 2.50 V. | Within 10 mV |

**Evidence:** photo of the rails table filled in, display test photo.

## Stage 3: add the analog board

| # | Do | Pass |
|---|---|---|
| 3.1 | Power off. Resistance check on the analog board's own headers (+12, −12, +3.3, +5, VREF_5 to GND). | Same rule as 0.3 |
| 3.2 | Plug it in (check H1/H2/H3 pin 1). Power on with current limit. | No limit hit. +12 and −12 each rise by roughly 15–30 mA |
| 3.3 | Re-measure all rails and VREF_5 (now loaded by the LFO offset resistors). | Same limits as 2.2. VREF_5 must stay above 4.975 V |
| 3.4 | Measure VREF_BUF at tp7. | 4.99–5.01 V |

## Stage 4: buttons and gate inputs (74HC165 chain)

| # | Do | Pass |
|---|---|---|
| 4.1 | `in watch`. With nothing pressed: | All columns show `.` |
| 4.2 | Press each mute button SW4…SW11 and menu SW12/SW13. | Exactly one `1` in the right column for each |
| 4.3 | Gate inputs: patch CV out J10 to gate in J2, `cv J10 10`, then `cv J10 0`. Repeat J2…J9. | Matching gate bit goes 1/0. Threshold: about 0.8 V (try `cv J10 0.6` / `cv J10 1.0`) |

## Stage 5: mute LEDs (TLC5916)

| # | Do | Pass |
|---|---|---|
| 5.1 | `leds chase` | DS4…DS11 light one at a time in order, even brightness (≈ 9 mA from R80 2k) |
| 5.2 | `leds ff`, then `leds 0`. | All on / all off |
| 5.3 | `follow`: press mute buttons. | The LED above each button toggles |

## Stage 6: CV outputs J10…J17

| # | Do | Pass |
|---|---|---|
| 6.1 | `cvsteps`. Measure each jack at every step. | Unipolar J10/J11/J14/J15: 0, 2.5, 5, 7.5, 10 V. Bipolar J12/J13/J16/J17: −5, −2.5, 0, +2.5, +5 V. ±2 % before calibration |
| 6.2 | The LED next to each jack (DS12…DS19) brightens with positive voltage. | All 8 LEDs respond |
| 6.3 | Calibrate each output: `cvcode J10 400`, measure V1; `cvcode J10 3700`, measure V2; `cal J10 400 V1 3700 V2`. `cal show`. | After cal, `cv J10 5` reads 5.00 V ±10 mV |
| 6.4 | Short-circuit check: briefly short one output to GND through a patch cable. | Nothing gets hot, output recovers (1 kΩ output resistors) |

## Stage 7: V/oct outputs J22/J23 and LFO test

| # | Do | Pass |
|---|---|---|
| 7.1 | SW3 to the 0..10 V position, `voct range uni`, `voct octaves`. Measure J23 and J22. | 0…10 V in 1 V steps |
| 7.2 | SW3 to the ±5 V position, `voct range bi`, `voct octaves`. | −5…+5 V |
| 7.3 | Calibrate both with the best meter you have: `voct code a 6554` → V1, `voct code a 58982` → V2, `cal J23 6554 V1 58982 V2`. Same for J22. | `voct a 1`, `voct a 2`, … read within ±1 mV of each volt (≈ 1 cent) |
| 7.4 | `lfo on`. Scope J10…J17 (or patch into a VCA / LED). | Sine/triangle/saw/square/S&H at the rates printed in the source, no glitches |
| 7.5 | Bus speed margin: `spi 20000000`, repeat 6.1 on two outputs and 5.1. Then `spi 8000000`. | Still correct at 20 MHz |

**Evidence:** short video of `lfo on` with LEDs moving + scope.

## Stage 8: MIDI to CV

| # | Do | Pass |
|---|---|---|
| 8.1 | `midi cv`, patch J23 → VCO 1V/oct, J10 → envelope gate. Play C1, C2, C3. | Octaves in tune after 7.3; gate opens on key down |
| 8.2 | Velocity on J14. | 0…10 V with velocity |

## Stage 9: audio ADC (PCM1808) and scope view

| # | Do | Pass |
|---|---|---|
| 9.1 | Check jumpers J25/J27 (gain 1 = 100k, gain 0.27 = 27k) and set it: `adc gain 0.27`. | Saved |
| 9.2 | VCO (or phone tone generator, 440 Hz) into J26. `adc watch`. | J26 shows ≈ 440 Hz, Vpp near the source level. J28 stays near 0 |
| 9.3 | Same into J28. | Left/right not swapped (if swapped, note it in `pins.h`) |
| 9.4 | `tft scope`. | Stable waveform on the display with Hz and Vpp |
| 9.5 | Note: the input is AC coupled, so slow LFOs and DC CV read near 0. | Expected |

## Stage 10: WiFi and HTTP smoke test

| # | Do | Pass |
|---|---|---|
| 10.1 | `wifi set <ssid> <password>` (2.4 GHz network), `wifi on`. | IP printed; RSSI better than −70 dBm with the module in the case |
| 10.2 | Open `http://synthpatch.local/` (or the IP) on a phone. | Live table with temperature, humidity, gates, encoder, input Hz/Vpp; scope canvas |
| 10.3 | Set J12 to 3 V from the web page. | Jack reads 3.00 V |
| 10.4 | `curl http://synthpatch.local/api/status` from the laptop. | Valid JSON |

**Evidence:** screen recording of the phone page changing a CV while the meter shows it.

## Stage 11: soak test

Run `lfo on` and `wifi on` for at least one hour inside the case.
Pass: no resets (`info` reset reason stays power-on), WiFi stays up, buck converters and LP2985 are
warm but touchable, SHT45 temperature inside the case is noted (useful for the oscillator drift story).

---

## Results log

| Stage | Date | Result | Notes / measured values |
|---|---|---|---|
| 0 | | | |
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |
| 6 | | | |
| 7 | | | |
| 8 | | | |
| 9 | | | |
| 10 | | | |
| 11 | | | |
