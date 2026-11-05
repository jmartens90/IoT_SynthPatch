# synthpatch app firmware, stage A1: the modulation engine

This is the first stage of the real firmware: the part that turns LFO and envelope settings into
voltages. It is written in plain C++17, with no Arduino or ESP32 code in it. That means you can compile,
run and test it on your PC, and the same files go into the ESP32 build later. It also doubles as a C++
lesson: every file starts with a comment explaining the concept it shows.

## Run it on your PC

**Qt Creator:** File → Open File or Project → choose `firmware/synthpatch/CMakeLists.txt` → pick a kit
(Desktop, MinGW or MSVC) → run the `tests` target, then `demo`.

**Terminal** (any g++ or clang):

```
cd firmware/synthpatch
cmake -B build
cmake --build build
./build/tests      # should print: all 39 checks passed
./build/demo       # ASCII plot of the LFO and envelope, plus demo.csv
```

Open `demo.csv` in Excel and make a line chart to see the LFO and the envelope curves.

## Read the code in this order

| # | File | C++ concepts | Course slides |
|---|---|---|---|
| 1 | `lib/engine/Param.h` | class, constructor + initializer list, private members, getters/setters, `const` | basics, OOP structures |
| 2 | `lib/engine/Waveform.h` | abstract class, `virtual` / `= 0`, `override`, `final`, Strategy pattern, `unique_ptr`, factory function | basics (abstraction, polymorphism), design patterns, dynamic memory |
| 3 | `lib/engine/Modulator.h/.cpp` | inheritance, `protected`, deleted copy, `std::map` + `std::vector`, .h vs .cpp | basics (inheritance), containers |
| 4 | `lib/engine/Lfo.h/.cpp` | child class, constructor overloading + delegation, composition (owns a Waveform), `std::move`, `std::array` | basics (constructor), dynamic memory |
| 5 | `lib/engine/Envelope.h/.cpp` | `enum class`, state machine with `switch` | |
| 6 | `lib/engine/CvOutput.h` | interface, Adapter pattern | design patterns (adapter) |
| 7 | `lib/engine/SignalSlot.h` | Observer pattern (Qt signals/slots without Qt), templates, `std::function`, lambdas | OOP structures (signals and slots) |
| 8 | `lib/engine/Command.h` | Command pattern, `struct`, static helper functions | design patterns (command) |
| 9 | `lib/engine/ModulatorFactory.h` | Factory pattern, `static` member function | design patterns (factory) |
| 10 | `lib/engine/Engine.h/.cpp` | ownership with `vector<unique_ptr<>>`, polymorphic loop, `std::queue`, `std::mutex` + `lock_guard` (RAII) | dynamic memory, containers |
| 10b | `lib/engine/PitchTracker.h/.cpp` | nested `struct` with default values, `std::deque` sliding window, `static` helpers, Signal for note events | containers |
| 11 | `desktop/demo.cpp` | using it all: lambdas as slots, posting commands | |
| 12 | `desktop/tests.cpp` | automatic tests, macros | |

The UML class diagram of the full design (including the TFT, MQTT and the pitch tracker that come next) is in
[docs/firmware-architecture.md](../../docs/firmware-architecture.md).

## Try this yourself

Small exercises that stay inside what you've seen above:

1. **Add a waveform:** a `RampDown` class in `Waveform.h` (`1 - 2*phase`), add it to `makeWaveform()` and
   to `kShapes` in `Lfo.cpp`, then write a check for it in `tests.cpp`.
2. **Add a parameter:** give the LFO a `phase` offset parameter (0…1) and use it in `Lfo::tick()`.
3. **Second slot:** in `demo.cpp`, connect a second lambda to `engine.paramChanged` that counts the changes
   per source ("app", "tft"), using a `std::map<std::string, int>`, and print the counts at the end.
4. **Undo:** store the old value in `Command`, push executed commands on a `std::vector`, and add
   `Engine::undo()`.

## What comes next

* A2: TFT pages + encoder (Composite pattern for the menu), on the ESP32.
* A3: SHT45 + PCM1808 pitch tracker with the note/drift classifier (also PC-testable with generated sine waves).
* A4: MQTT, A5: backend, A6: Qt app.

Note for the ESP32 build: PlatformIO compiles Arduino code as C++11 by default. The engine uses C++17
(`std::clamp`, `std::make_unique`), so the ESP32 `platformio.ini` will need
`build_unflags = -std=gnu++11` and `build_flags = -std=gnu++17`.
