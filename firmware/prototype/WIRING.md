# Prototype wiring (ESP32-S3 DevKitC)

Works on N8, N8R8, N16R8, ...: none of these pins are used by flash or PSRAM (the R8 octal
PSRAM takes GPIO 35–37, which the prototype leaves alone).

All signals are **3.3 V**. Never put more than 3.3 V on a GPIO.

## I2C bus (shared): SDA = GPIO 38, SCL = GPIO 14

Same pins as J21 on the PCB.

| Board | VIN / VDD | GND | SDA | SCL | Address | Output |
|---|---|---|---|---|---|---|
| **SHT45** (STEMMA QT) | 3V3 | GND | 38 | 14 | 0x44 | |
| **MCP4725** #1, LFO | **3V3** | GND | 38 | 14 | 0x60 (A0 to GND) | VOUT = LFO, 0…3.3 V |
| MCP4725 #2, envelope (optional) | 3V3 | GND | 38 | 14 | 0x61 (A0 to VCC) | VOUT = envelope |
| **AD5693R**, V/oct | 3V3 or 5V | GND | 38 | 14 | 0x4C (A0 to GND) | VOUT = V/oct, 0…2.5 V |

* Run the MCP4725 from 3.3 V: its output then never goes above 3.3 V, so you can wire it straight
  back into the ADC input to graph it.
* AD5693R (Adafruit): the output is the **VOUT hole** of the 2-hole terminal footprint (3.5 mm
  pitch), not A0 or LDAC (those are inputs). The board regulates its own 3.3 V, so the chip's
  range is 0…2.5 V with the internal reference (gain 1, the default in `include/config.h`):
  0 V = C2, 2.5 V = F#4. For 0…5 V add an op-amp stage with gain 2 (like the PCB).
* Breakouts usually have I2C pull-ups already. With three or four boards in parallel that's fine at 400 kHz.

## Envelope without a second MCP4725

GPIO 2 → 10 kΩ → VOUT, with 1 µF from VOUT to GND (RC low-pass on the PWM). An LED + 1 kΩ from
GPIO 2 to GND also shows the envelope as brightness.

## Temperature without the SHT45

DS18B20: VDD to 3V3, GND to GND, DQ to **GPIO 21**, with **4.7 kΩ from DQ to 3V3**. Detected
automatically when no SHT45 answers.

DHT11 (3-pin breakout): + to 3V3, - to GND, S/OUT to **GPIO 16** (a bare 4-pin DHT11 also needs
10 kΩ from data to 3V3). Used when there is no SHT45 and no DS18B20. It only has 1 °C steps.

With none of these, the ESP32's internal sensor is used (chip temperature, only as a last resort).

## Frequency input (instead of the PCM1808): GPIO 13

A 0…3.3 V square wave. Options:

* **Test:** jumper GPIO 17 (test tone) → GPIO 13.
* **Real drift:** a 40106 (CD40106) Schmitt-trigger RC oscillator on 3.3 V: R from output to input,
  C from input to GND, output → GPIO 13. f ≈ 1/(0.8·R·C), e.g. 33 kΩ + 68 nF ≈ 560 Hz. A ceramic
  (Y5V) cap or an NTC in place of R makes it drift clearly with temperature.
* A synth oscillator only through a comparator or clamp circuit that limits it to 0…3.3 V.

## ADC input: GPIO 8

0…3.1 V. Wire the LFO MCP4725 VOUT here to graph the output, or a 10 kΩ pot between 3V3 and GND
with the wiper on GPIO 8.

## Gate button

The **BOOT** button on the dev kit (GPIO 0) gates the envelope.

## Pin summary

| GPIO | Use |
|---|---|
| 38, 14 | I2C SDA, SCL |
| 2 | envelope PWM (only without a 2nd MCP4725) |
| 8 | ADC input |
| 13 | frequency input |
| 17 | test tone output |
| 21 | DS18B20 |
| 16 | DHT11 |
| 0 | BOOT button = gate |
