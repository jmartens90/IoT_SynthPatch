# Prototype: the course demo on a bare ESP32-S3 dev kit

Runs the IoT course scope while the PCB parts are delayed. It uses the **same engine and pitch
tracker** as the real module (`../synthpatch/lib/engine`), with breadboard parts in place of the
PCB chips:

| On the PCB | In the prototype | Code |
|---|---|---|
| DAC124S085 → LFO jack | **MCP4725** 0x60, 0…3.3 V | `Mcp4725Output` |
| DAC124S085 → envelope jack | 2nd **MCP4725** 0x61, or PWM + RC on GPIO 2 | `Mcp4725Output` / `PwmOutput` |
| DAC8562 → V/oct | **AD5693R** 0x4C, 16-bit, internal 2.5 V ref, 0…5 V | `Ad5693Output` |
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
* **Dashboard + interaction:** web page at `http://synthpatch.local` with live values, a 10-minute
  chart (temperature + drift / Hz / volts, note changes as lines), LFO and envelope controls, gate
  button and a keyboard. CSV download of the log.
* **IoT communication:** HTTP REST on the device, plus MQTT telemetry / events / retained settings /
  commands with a last-will "offline" status (see `src/MqttLink.h` for the topics).
* **Recovery:** WiFi auto-reconnect, MQTT retry every 5 s, access-point fallback "SynthPatch" when
  there is no WiFi (works in a classroom).

**Backend + storage:** [`backend/`](../../backend) runs Mosquitto + a logger into SQLite + a history
page with `docker compose up` on your laptop.

## Build and flash

```
cd firmware/prototype
pio run -t upload
pio device monitor        # type: help
```

First boot has no WiFi stored, so it opens the access point **SynthPatch** (password `synthpatch`):
join it and browse to http://192.168.4.1. To use your own network:

```
wifi set <ssid> <password>          (reboots, then http://synthpatch.local)
mqtt set 192.168.1.20               (optional: your laptop running Mosquitto)
status                              (which DACs and which temperature sensor were found)
```

## Try it without an oscillator

Put a jumper wire from **GPIO 17 to GPIO 13**. Keep "also play the note on the test tone" ticked and
press keys on the web keyboard: the test tone plays the note, the frequency input measures it, and
the note events appear. For real drift, use a 40106 or 555 RC oscillator (see WIRING.md): warm it
with your finger or a hair dryer and the drift line moves with the temperature.

## Calibrate V/oct

Measure the AD5693R output with a multimeter at two codes, then enter both points:

```
voct code 13107        -> measure, e.g. 0.998 V
voct code 52428        -> measure, e.g. 3.995 V
voct cal 13107 0.998 52428 3.995
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
