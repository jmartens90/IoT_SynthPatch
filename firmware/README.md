# Firmware

ESP32-S3-WROOM-1-N16R2 firmware for IoT_SYNTHPATCH, built with [PlatformIO](https://platformio.org/).

| Folder | What |
|---|---|
| [`bringup/`](bringup) | Hardware test shell used with [docs/bringup-test-plan.md](../docs/bringup-test-plan.md). One image, one command per subsystem. Breadboard dev kit wiring: [bringup/BREADBOARD.md](bringup/BREADBOARD.md) |
| [`prototype/`](prototype) | Course demo on a bare ESP32-S3 dev kit while the PCB parts are delayed: MCP4725 + AD5693R I2C DACs, SHT45/DS18B20/DHT11/internal temperature, GPIO frequency counter, web GUI over HTTP REST + WebSocket, BLE app link, flash log (MQTT optional, off). Same engine as the app. See [prototype/README.md](prototype/README.md) |
| [`synthpatch/`](synthpatch) | The application. Stage A1 (LFO/envelope engine, plain C++17, runs and tests on a PC) is in; ESP32 integration on the PCB and the TFT follow. |

## Install

1. Install VS Code + the PlatformIO extension, or `pip install platformio`.
2. Open `firmware/bringup` (the folder with `platformio.ini`).
3. Build: `pio run`. The first build downloads the ESP32 toolchain (~1 GB).

## Flash

1. Connect USB-C. For USB-only power fit jumper **J19** and remove **J24**; on Eurorack power do the opposite (see the test plan).
2. `pio run -t upload`. If the port isn't found: slide SW2 (BOOT) on, toggle SW1 (EN), upload, slide SW2 back, toggle SW1.
3. `pio device monitor` and type `help`.

## Bring-up shell commands

```
Stage 1  info | led on|off|blink | spi <hz>
Stage 2  i2c | sht [watch]
Stage 3  in [watch] | enc
Stage 4  leds <hex>|chase|off
Stage 5  cv <J10..J17|all> <volts> | cvcode <jack> <0-4095> | cvsteps
         cal <jack> <code1> <volts1> <code2> <volts2> | cal show | cal reset
Stage 6  voct init | voct <a|b> <volts> | voct code <a|b> <0-65535>
         voct range uni|bi (match SW3) | voct octaves
Stage 7  lfo on|off | follow
Stage 8  midi [cv]
Stage 9  adc [watch] | adc gain <1|0.27> | adc stop | tft test|scope | bl <0-255> | sd
Stage 10 wifi set <ssid> <pass> | wifi on | wifi status | wifi forget
```

(The shell's stage numbers group commands; the test plan's stage numbers add power and analog-board steps in between.)

## Secrets

WiFi credentials are typed into the shell and stored in the ESP32's NVS flash, never in the source.
Later firmware reads broker credentials from `include/secrets.h`, which is git-ignored; copy
`include/secrets.example.h` to create it.

## Notes from the schematics

* CV bus: SCLK IO18, DIN IO17 are shared by both DAC124S085s, the DAC8562 (via 74HCT125) and the
  TLC5916. Every DAC frame also shifts the TLC5916, so an LED update (shift + LE pulse) is done as one
  locked step. DACs use SPI mode 1, the TLC5916 mode 0.
* DAC channels A and C are 0…10 V outputs, B and D are −5…+5 V. At power-up the code sets the bipolar
  ones to mid-scale so they don't sit at −5 V.
* The PCM1808 input is AC coupled, so it reads oscillators and audio, not DC control voltages.
