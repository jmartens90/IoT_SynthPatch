# Prototype: the course demo on a bare ESP32-S3 dev kit

Runs the IoT course scope while the PCB parts are delayed. It uses the **same engine and pitch
tracker** as the real module (`../synthpatch/lib/engine`), with breadboard parts in place of the
PCB chips:

| On the PCB | In the prototype | Code |
|---|---|---|
| DAC124S085 → LFO jack | **MCP4725** 0x60, 0…3.3 V | `Mcp4725Output` |
| DAC124S085 → envelope jack | 2nd **MCP4725** 0x61, or PWM + RC on GPIO 2 | `Mcp4725Output` / `PwmOutput` |
| DAC8562 → V/oct | **AD5693R** 0x4C, 16-bit, internal 2.5 V ref, gain 2, 0…3.3 V | `Ad5693Output` |
| SHT45 on J21 | SHT45, or **DS18B20**, or **DHT11**, or the ESP32's internal sensor (auto-detected) | `TemperatureSensor::detect()` |
| PCM1808 audio ADC | **frequency counter** on GPIO 13 (0…3.3 V square wave) | `FrequencyInput` |
| TFT + encoder | (not yet; the web page has the same settings) | |

Wiring: [WIRING.md](WIRING.md).

## What it does (mapped to the course must-haves)

* **Sensor + edge processing:** temperature every second, range-checked, spike-filtered and averaged
  over 10 readings. Input frequency every 50 ms, converted to note + cents, and the
  note-vs-drift classifier (`PitchTracker`) on the device.
* **Actuator:** LFO and envelope (engine at 1 kHz in its own FreeRTOS task) on the I2C DACs; V/oct
  from the web keyboard on the AD5693R.
* **Synth panel:** knobs for the LFO (rate, depth, offset around the 1.65 V middle, with a scope
  that shows in red where the 0…3.3 V DAC clips) and the envelope (A/D/S/R/peak, with its curve).
  Drag up/down (Shift = fine), mouse wheel, double-click = default.
* **Patch memory:** 4 banks x 8 named patches (A1…D8) saved as small JSON files in LittleFS
  (`src/PatchBank.h`). Click a slot to load it, type a name and press SAVE to store.
* **Sequencer:** 16 steps (note + on/off), 20–300 BPM, 1–16 steps, run by the ESP in the 1 kHz
  engine task (`../synthpatch/lib/engine/Sequencer.h`, tested on the PC). RETRIG = a new gate every
  step (GATE knob = gate length), LEGATO = one gate per loop and only the pitch changes. TRIG, the
  BOOT button or a rising edge on GPIO 5 restarts it at step 1. A key on the web keyboard (with no
  step selected) restarts it transposed: the steps are written relative to C3, so pressing E3 plays
  everything 4 semitones up. The sequence is saved in the patch; shell `seq` prints the steps.
* **Outputs view:** a 10-second scope of the three DAC outputs (LFO, envelope, V/oct with note
  names), sampled every 20 ms on the ESP, and a log of every note sent to V/oct (time, note,
  volts, DAC code, source). Both arrive over the WebSocket.
* **Dashboard + interaction:** web page at `http://synthpatch.local` with live values, a 10-minute
  chart (temperature + drift / Hz / volts, note changes as lines), LFO and envelope controls, gate
  button and a keyboard. CSV download of the log.
* **IoT communication:** HTTP REST API on the device (the chosen protocol, see the table below),
  plus a **WebSocket** on port 81 for push: the ESP sends the live values every second and the
  sequencer position when it starts/stops/restarts, so the page never polls. Commands still go
  over REST. (`src/WebUi.h` explains why both.)
  Optional, off by default: MQTT telemetry / events / retained settings /
  commands with a last-will "offline" status (see `src/MqttLink.h` for the topics).
* **Recovery:** WiFi auto-reconnect, MQTT retry every 5 s, access-point fallback "SynthPatch" when
  there is no WiFi (works in a classroom).

**Backend + storage on the device itself:** every 10 s one 16-byte record (time, temperature,
humidity, Hz, drift, volts) goes into a ring-buffer file `/log.bin` in the ESP32's flash
(LittleFS, 400 000 records = ~46 days, `src/DataLog.h`). It survives reboots and power cuts; when
full, the oldest record is overwritten. The web page shows it with a range picker (live, 1 h,
24 h, 7 d, 14 d, 46 d), and `/log-full.csv?days=14` downloads it. No server or Raspberry Pi needed.
(The optional [`backend/`](../../backend) with Mosquitto + SQLite is for later.)

Logging needs the real time, which comes from NTP, so the ESP must be on your home WiFi
(`wifi set`). In access-point mode nothing is stored (there is no clock).

### REST API

| Endpoint | Returns |
|---|---|
| `GET /api/live` | current values (JSON), incl. `time` and `log_count` |
| `GET /api/history` | last 10 minutes from RAM, one point per second |
| `GET /api/log?hours=336&points=800` | stored log, averaged down to `points` (10…3000) |
| `GET /log-full.csv?days=14` | stored log as CSV, every record, local time |
| `GET /api/settings`, `POST /api/set`, `/api/shape` | LFO / envelope settings |
| `GET /api/patches` | the 32 patch slots with their names (`null` = empty) |
| `POST /api/patch/save?slot=0&name=Slow%20wobble`, `/api/patch/load?slot=0`, `/api/patch/delete?slot=0` | patch memory |
| `GET /api/seq`, `POST /api/seq` (JSON body, only the fields that change) | sequencer pattern |
| `POST /api/seq/run?on=1`, `/api/seq/trig`, `GET /api/seq/pos` | transport, running light |
| `POST /api/note`, `/api/gate` | keyboard and gate |

### Flash layout (16 MB, `partitions_16MB.csv`)

| Partition | Size | For |
|---|---|---|
| app0 / app1 | 2 x 4 MB | firmware (two slots, so OTA updates are possible later) |
| spiffs (LittleFS) | 7.9 MB | the data log (6.4 MB) + room for screen images / fonts later |
| nvs, otadata, coredump | small | WiFi + calibration settings, crash dump |

## Build and flash

```
cd firmware/prototype
pio run -t upload
pio device monitor        # type: help
```

**First boot with the 16 MB layout** formats the LittleFS partition (a few seconds, `status`
shows the log). If anything behaves oddly after the switch, wipe the flash once with
`pio run -t erase` and upload again; then WiFi and V/oct calibration must be entered again.

First boot has no WiFi stored, so it opens the access point **SynthPatch** (password `synthpatch`):
join it and browse to http://192.168.4.1. To use your own network:

```
wifi set <ssid> <password>          (reboots, then http://synthpatch.local)
mqtt set 192.168.1.20               (optional: your laptop running Mosquitto)
status                              (which DACs and which temperature sensor were found)
log                                 (how many records are stored, oldest / newest time)
log clear                           (delete the stored log)
```

## Try it without an oscillator

Put a jumper wire from **GPIO 17 to GPIO 13**. Keep "also play the note on the test tone" ticked and
press keys on the web keyboard: the test tone plays the note, the frequency input measures it, and
the note events appear. For real drift, use a 40106 or 555 RC oscillator (see WIRING.md): warm it
with your finger or a hair dryer and the drift line moves with the temperature.

## Calibrate V/oct

Measure the AD5693R output with a multimeter at two codes, then enter both points:

```
voct code 6554         -> measure, e.g. 0.499 V
voct code 39322        -> measure, e.g. 2.998 V
voct cal 6554 0.499 39322 2.998
voct cal show
note 60                -> C4 = 2.000 V (0 V = C2)
```

The calibration is stored in flash and survives a reboot.

## Code tour (C++ course concepts)

| File | Shows |
|---|---|
| `src/Outputs.h` | Adapter pattern: three hardware outputs behind the engine's `CvOutput` |
| `src/TemperatureSensor.h/.cpp` | Interface + Strategy + Factory (`detect()`), virtual with a default (`humidity()`), pimpl |
| `src/I2cBus.h` | Singleton, RAII lock class, deleted copy |
| `src/History.h` | Class template with a size parameter, function template taking a lambda |
| `src/SettingsMirror.h` | Observer (connected to `engine.paramChanged`), why tasks need a copy + mutex |
| `src/Inputs.h/.cpp` | `static` members, an interrupt routine, critical sections |
| `src/App.h/.cpp` | Composition, a FreeRTOS task started from a member function |
| `src/WebUi.cpp` | Lambdas capturing `this` as HTTP handlers, Command pattern from the web |
| `../synthpatch/lib/engine/PitchTracker.h` | Nested config struct, `std::deque` sliding window, tested on the PC |
