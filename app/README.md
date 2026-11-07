# SynthPatch app (Android, Qt 6 + Bluetooth)

The same controls as the web page, in an Android app that talks to the ESP over Bluetooth
Low Energy. No WiFi is needed: the phone connects straight to the module.

| Page      | What it does                                                                    |
|-----------|---------------------------------------------------------------------------------|
| Synth     | LFO (scope with clip display, 4 shapes, RATE/DEPTH/OFFSET) and envelope (ADSR + peak, GATE button) |
| Seq       | PLAY/STOP, TRIG, RETRIG/LEGATO, BPM/GATE/STEPS, piano roll with running light, keyboard (transpose or step entry) |
| Patches   | Banks A..D x 8 slots: load, save with a name, delete                            |
| Monitor   | Live values, history chart (10 min live up to 46 days from the flash log), CV scope of the 3 DACs, V/oct note log |

## How it fits together

```
 QML pages (qml/*.qml)          what you see and touch
        |  device.setParam(...), device.live.temp_c
 Device (src/Device.*)          the module's state + one function per command
        |  {"id":7,"m":"POST","p":"/api/set?target=lfo1&name=rate&value=2"}
 BleLink (src/BleLink.*)        Bluetooth: scan, connect, send/receive JSON lines
        |  BLE service 6e5a0001-5b1d-4c2a-9e43-53594e544850
 ESP: BleLink.cpp -> Api.cpp    the SAME Api the web page uses over HTTP
```

The ESP answers each request with `{"type":"reply","id":7,"status":200,"body":...}` and pushes
on its own: `live` (1/s), `seq` (when the sequencer changes), `cv` (DAC outputs, 5/s) and
`notes` (each V/oct note). The protocol is described in `firmware/prototype/src/BleLink.h`.

## Try it on the PC first (no ESP needed)

```
SynthPatchApp --demo
```

`--demo` replaces Bluetooth by a pretend ESP inside the app (`src/DemoEsp.*`): the sequencer
plays, the scope moves, saving patches works. Handy to learn the code and the pages.
`--page 1` opens a page directly (0 Synth, 1 Seq, 2 Patches, 3 Monitor).
In Qt Creator: Projects > Run > Command line arguments.

## Build for Android

1. Install **Qt 6.8 LTS** (at least 6.5) with the Qt Online Installer. Tick
   *Android* and *Qt Connectivity* (that is Qt Bluetooth) under the Qt version, and
   *Qt Creator*.
2. In Qt Creator: *Edit > Preferences > Devices > Android*. Let it download the JDK,
   Android SDK and NDK ("Set Up SDK"). Everything should get a green tick.
3. On the phone: *Settings > About phone*, tap *Build number* 7 times, then turn on
   *Developer options > USB debugging*. Connect it with USB and allow the PC.
4. *File > Open File or Project* > `app/CMakeLists.txt`. Pick the kit
   **Android Qt 6.8 arm64-v8a** (most phones) and a Desktop kit if you also want the PC version.
5. Choose the phone at the bottom left (the kit selector) and press the green Run arrow.
   Qt Creator builds the .apk, installs it and starts it.

The first time, Android asks for the **Nearby devices** permission (Bluetooth). Allow it,
otherwise the app cannot scan. On Android 11 and older it asks for **location** instead (that
is how old Android versions protect Bluetooth scanning) and location must be switched on.

The .apk ends up in `build-.../android-build/build/outputs/apk/debug/`. You can copy it to
another phone and install it there (allow "install unknown apps").

## Connecting

Power the ESP (firmware from `firmware/prototype`) and open the app. It scans for 10 s for a
device called **SynthPatch** (or with the SynthPatch service), connects, gives the ESP the
phone's clock (needed for the flash log in AP mode) and loads all settings. The dot at the top
turns green. If the link drops it tries again by itself; ↻ forces a reload or a new scan.

WiFi and Bluetooth run at the same time on the ESP. The web page and the app can be open
together and see each other's changes (a knob turned on the phone moves on the web page).

## Files

| File               | What it is                                                         |
|--------------------|--------------------------------------------------------------------|
| `src/main.cpp`     | starts Qt, creates the Device, loads `Main.qml`                    |
| `src/BleLink.*`    | Bluetooth Low Energy: scan, connect, notifications, MTU chunking   |
| `src/Device.*`     | requests/replies, pushes -> Q_PROPERTYs for QML                    |
| `src/DemoEsp.*`    | the pretend ESP for `--demo`                                       |
| `qml/Main.qml`     | window, status bar, tabs                                           |
| `qml/Knob.qml`     | the synth knob (drag up/down, wheel, double-tap = default)         |
| `qml/Keyboard.qml` | two octaves C3..C5                                                 |
| `qml/Panel.qml`    | the titled box around each module                                  |
| `qml/*Page.qml`    | the four pages                                                     |

## Troubleshooting

* **"Bluetooth permission denied"**: Android settings > Apps > SynthPatch > Permissions >
  Nearby devices > Allow.
* **Stays on "scanning"**: is the ESP powered, and does its serial log say
  `Bluetooth: advertising as 'SynthPatch'`? Is the ESP still connected to another phone? (one BLE
  client at a time).
* **Desktop build on Linux**: needs BlueZ 5 and a Bluetooth adapter; `--demo` works without.
