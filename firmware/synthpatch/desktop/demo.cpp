// demo.cpp - run the synth engine on your PC, no ESP32 needed.
//
// It builds one LFO and one envelope (like on jacks J12 and J10), simulates 2 seconds at 1 kHz,
// changes settings halfway through with commands (as the Qt app or the TFT would), and prints an
// ASCII plot plus a CSV file (demo.csv) you can open in Excel to see the curves.
//
// Build in Qt Creator: File > Open File or Project > firmware/synthpatch/CMakeLists.txt, run "demo".
// Or in a terminal:  g++ -std=c++17 -I../lib/engine ../lib/engine/*.cpp demo.cpp -o demo && ./demo

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "CvOutput.h"
#include "Engine.h"
#include "ModulatorFactory.h"

using namespace synth;  // so we can write Engine instead of synth::Engine

// Draws one value as a row of characters: "|         *          |"
static std::string bar(float v, float lo, float hi, int width = 41) {
  std::string row(width, ' ');
  row.front() = '|';
  row.back() = '|';
  int pos = static_cast<int>((v - lo) / (hi - lo) * (width - 1) + 0.5f);
  if (pos >= 0 && pos < width) row[pos] = '*';
  return row;
}

int main() {
  // The outputs: on the module these would be DAC adapters, here they just remember the value.
  MemoryOutput j12(-5.0f, 5.0f);  // bipolar jack
  MemoryOutput j10(0.0f, 10.0f);  // unipolar jack

  Engine engine;

  // Observer: print every change the engine executes. The lambda is the "slot".
  engine.paramChanged.connect([](const ParamEvent &e) {
    std::cout << "  [" << e.source << "] " << e.target << "." << e.name << " = "
              << (e.text.empty() ? std::to_string(e.value) : e.text) << "\n";
  });
  engine.error.connect([](const std::string &msg) { std::cout << "  error: " << msg << "\n"; });

  // Factory creates the objects; std::move hands ownership to the engine.
  engine.add(ModulatorFactory::create("lfo", "lfo1"), j12);
  engine.add(ModulatorFactory::create("envelope", "env1"), j10);

  // Initial settings, sent the same way the app will send them.
  engine.post(Command::setParam("lfo1", "rate", 2.0f, "app"));
  engine.post(Command::setShape("lfo1", "triangle", "app"));
  engine.post(Command::setParam("env1", "attack", 0.1f, "app"));
  engine.post(Command::setParam("env1", "release", 0.3f, "app"));
  engine.post(Command::setParam("lfo1", "rate", 999.0f, "app"));  // too high: gets clamped to 50
  engine.post(Command::setParam("lfo1", "rate", 2.0f, "app"));
  engine.post(Command::setParam("lfo1", "colour", 1.0f, "app"));  // unknown: error

  std::ofstream csv("demo.csv");
  csv << "time_s,lfo1_V,env1_V\n";

  const float dt = 0.001f;  // 1 kHz, same as on the module
  std::cout << "\n time    J12 LFO (-5..+5 V)                          J10 envelope (0..10 V)\n";
  for (int i = 0; i < 2000; i++) {
    float t = i * dt;
    if (i == 100) engine.post(Command::gate("env1", true, "gate J2"));
    if (i == 700) engine.post(Command::gate("env1", false, "gate J2"));
    if (i == 1000) engine.post(Command::setShape("lfo1", "square", "tft"));  // turned on the panel
    if (i == 1200) engine.post(Command::gate("env1", true, "gate J2"));
    if (i == 1500) engine.post(Command::gate("env1", false, "gate J2"));

    engine.tick(dt);

    csv << t << "," << j12.last() << "," << j10.last() << "\n";
    if (i % 40 == 0)
      std::printf(" %.2fs  %s  %s %5.2f V\n", t, bar(j12.last(), -5, 5).c_str(),
                  bar(j10.last(), 0, 10, 21).c_str(), j10.last());
  }
  std::cout << "\nWrote demo.csv (" << 2000 << " rows)\n";
  return 0;
}
