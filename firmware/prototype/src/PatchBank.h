// PatchBank.h - named patches (all LFO + envelope settings) saved in the ESP32's flash.
//
// 4 banks (A..D) x 8 patches = 32 slots, like the memory buttons on a hardware synth.
// Each slot is a small JSON file in LittleFS (the same file system as the data log):
//
//   /patches/05.json   {"name":"Slow wobble","lfo1":{"shape":"sine","rate":0.5,...},"env1":{...},
//                       "seq":{"bpm":120,"mode":"retrig","steps":[...]}}
//
// Saving copies the current values from the SettingsMirror. Loading turns every stored value back
// into a Command for the engine queue: the same path as a knob turn on the web page, so the page,
// the TFT and the log all see the change without extra code.
//
// C++ lesson: a class that hides HOW something is stored (files, JSON) behind a small interface
// (save / load / name). If the patches move to NVS or an SD card later, only PatchBank.cpp changes.
#pragma once

#include <ArduinoJson.h>

#include <string>

#include "SettingsMirror.h"

class App;

class PatchBank {
 public:
  static constexpr int BANKS = 4, PER_BANK = 8, SLOTS = BANKS * PER_BANK;
  static constexpr size_t MAX_NAME = 16;  // fits on one line of the TFT later

  // "A1".."D8" for slot 0..31
  static std::string label(int slot);

  bool save(int slot, const std::string &name, App &app);  // LFO + envelope + sequence
  bool load(int slot, App &app);  // false if the slot is empty or broken
  bool remove(int slot);

  // [{"slot":0,"label":"A1","name":"Slow wobble"}, {"slot":1,"label":"A2","name":null}, ...]
  void list(JsonArray out);

 private:
  static bool valid(int slot) { return slot >= 0 && slot < SLOTS; }
  static std::string path(int slot);
  static std::string cleanName(const std::string &name);
};
