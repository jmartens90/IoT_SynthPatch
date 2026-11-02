# IoT_SYNTHPATCH

A WiFi-connected Eurorack module: an ESP32-S3 generates LFO and envelope control voltages that you
set from a web/mobile dashboard or directly on the module's TFT screen and encoder. It also measures
the frequency of an oscillator patched into its PCM1808 input, tells played notes apart from tuning
drift, and graphs that drift together with the temperature from an SHT45 sensor.

Built for the IoT course project. Rubric mapping, architecture and roadmap:
[docs/iot-project-plan.md](docs/iot-project-plan.md).

## Repository

| Folder | Contents |
|---|---|
| [`hardware/`](hardware) | Altium projects: main board `IoT_SYNTHPATCH.PrjPcb`, analog board `ANALOG/`, Gerbers in `Project Outputs*`, Mouser BOM |
| [`firmware/`](firmware) | ESP32-S3 firmware (PlatformIO). `bringup/` = hardware test shell |
| [`docs/`](docs) | [Bring-up test plan](docs/bringup-test-plan.md), [IoT project plan](docs/iot-project-plan.md) |
| `backend/` | (planned) Docker compose: Mosquitto, API, PostgreSQL, dashboard |

## Hardware at a glance

| Block | Part | ESP32-S3 pins |
|---|---|---|
| 8 CV outputs (J10–J17) | 2x DAC124S085 + TL074 | SCLK 18, DIN 17, SYNC 15 / 21 |
| 2 V/oct outputs (J23, J22) | DAC8562 via 74HCT125 + TL074 | SCLK 18, DIN 17, SYNC 8 |
| Mute LEDs DS4–DS11 | TLC5916 | LE 16, OE 2 |
| 8 gate inputs, 8 mute + 2 menu buttons | 3x 74HC165 | PL 3, CP 47, Q7 48 |
| Audio/oscillator input (J26, J28) | PCM1808 | MCLK 35, BCK 36, LRCK 37, DOUT 42 |
| Display + micro-SD | DFR0664 ST7789 2.0" | SCLK 12, MOSI 11, MISO 9, CS 10, DC 6, RST 5, BLK 4, SD CS 7 |
| Encoder | PEC16 | A 39, B 40, SW 41 |
| MIDI in (J20) | HCPL-0700 | RX 13 |
| Temperature / humidity (J21, STEMMA QT) | SHT45 | SDA 38, SCL 14 |
| Status LED DS3 | | 1 |

The full wiring diagram is the schematic set in `hardware/` (export a PDF from Altium for the report).

## Getting started

1. Assemble and run the [bring-up test plan](docs/bringup-test-plan.md).
2. Build and flash the firmware: see [firmware/README.md](firmware/README.md).
