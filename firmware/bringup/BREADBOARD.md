# Breadboard prototype (before the PCBs arrive)

Wire a plain ESP32-S3 dev kit with the **same GPIOs as the PCB**, so the bring-up firmware and all later
code run unchanged. Only the TFT, SHT45, encoder and an optional LED are needed now.

## Wiring

| Part | Part pin | ESP32-S3 GPIO | Note |
|---|---|---|---|
| **DFR0664 TFT** | VCC | 3V3 (or 5V) | the module has its own regulator |
| | GND | GND | |
| | SCLK | **12** | |
| | MOSI | **11** | |
| | MISO | **9** | only used by the SD card |
| | CS | **10** | |
| | RES | **5** | |
| | DC | **6** | |
| | BLK | **4** | backlight, PWM |
| | SDCS | **7** | micro-SD chip select |
| **SHT45** (STEMMA QT cable) | VIN (red) | 3V3 | |
| | GND (black) | GND | |
| | SDA (blue) | **38** | Adafruit board already has I2C pull-ups |
| | SCL (yellow) | **14** | |
| **Encoder** PEC16 | A | **39** | internal pull-ups are switched on in the firmware |
| | B | **40** | |
| | C (common) | GND | |
| | switch, one side | **41** | |
| | switch, other side | GND | |
| Status LED (optional) | anode via 1k | **1** | cathode to GND |

Optional, same as on the PCB: 10 kΩ pull-up to 3V3 + 10 nF to GND on A, B and the switch, for less contact bounce.

## Check your dev kit first

* **Which module is on it?** Look at the metal can: `ESP32-S3-WROOM-1-N8R8`, `N16R8`, `N8`, ... The
  octal-PSRAM versions (`R8`) use GPIO 35–37 internally. That doesn't matter for the parts above, but
  those pins can't be used for the PCM1808 on that kit.
* **GPIO 48** drives the RGB LED on many DevKitC boards. It's not used in this prototype.
* **Which USB port:** use the one marked **USB** (native USB, same as the PCB), not **UART/COM**.

## Build and flash

```
cd firmware/bringup
pio run -e devkit -t upload
pio device monitor
```

The `devkit` environment assumes no PSRAM and 8 MB flash, so it boots on any S3 dev kit.

## What to test

| Command | Expect |
|---|---|
| `info` | chip ESP32-S3, flash size of your kit |
| `led blink` | LED on GPIO 1 blinks |
| `i2c`, `sht watch` | device at 0x44, plausible °C and %RH |
| `enc` | ±1 per detent, button DOWN/up |
| `tft test`, `bl 40` | colour test, backlight dims |
| `sd` | lists the card, if one is inserted |
| `wifi set <ssid> <pw>`, `wifi on` | web page at http://synthpatch.local shows temperature and encoder |

The DAC, 74HC165, MIDI and ADC commands also run, but read or drive nothing without the PCB.
