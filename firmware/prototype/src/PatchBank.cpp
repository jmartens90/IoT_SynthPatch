#include "PatchBank.h"

#include <LittleFS.h>

#include "App.h"

using synth::Command;

static const char *DIR = "/patches";

std::string PatchBank::label(int slot) {
  char s[4];
  snprintf(s, sizeof(s), "%c%d", 'A' + slot / PER_BANK, slot % PER_BANK + 1);
  return s;
}

std::string PatchBank::path(int slot) {
  char s[24];
  snprintf(s, sizeof(s), "%s/%02d.json", DIR, slot);
  return s;
}

// Keep printable characters only and cut to MAX_NAME, so a name can't break the file or the TFT.
std::string PatchBank::cleanName(const std::string &name) {
  std::string out;
  for (char c : name)
    if (c >= 32 && c < 127 && out.size() < MAX_NAME) out += c;
  return out.empty() ? "Init" : out;
}

bool PatchBank::save(int slot, const std::string &name, SettingsMirror &settings) {
  if (!valid(slot)) return false;
  JsonDocument doc;
  settings.values(doc.to<JsonObject>());  // {"lfo1":{...},"env1":{...}}
  doc["name"] = cleanName(name);

  if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
  fs::File f = LittleFS.open(path(slot).c_str(), "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool PatchBank::load(int slot, App &app) {
  // exists() first: opening a missing file for reading prints an error on the serial log
  if (!valid(slot) || !LittleFS.exists(path(slot).c_str())) return false;
  fs::File f = LittleFS.open(path(slot).c_str(), "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  // {"lfo1":{"shape":"sine","rate":0.5}} -> setShape(lfo1, sine), setParam(lfo1, rate, 0.5)
  for (JsonPair mod : doc.as<JsonObject>()) {
    if (!mod.value().is<JsonObject>()) continue;  // skips "name"
    const char *target = mod.key().c_str();
    for (JsonPair p : mod.value().as<JsonObject>()) {
      if (strcmp(p.key().c_str(), "shape") == 0)
        app.post(Command::setShape(target, p.value().as<const char *>(), "patch"));
      else
        app.post(Command::setParam(target, p.key().c_str(), p.value().as<float>(), "patch"));
    }
  }
  return true;
}

bool PatchBank::remove(int slot) {
  return valid(slot) && LittleFS.exists(path(slot).c_str()) && LittleFS.remove(path(slot).c_str());
}

void PatchBank::list(JsonArray out) {
  for (int slot = 0; slot < SLOTS; slot++) {
    JsonObject o = out.add<JsonObject>();
    o["slot"] = slot;
    o["label"] = label(slot);
    fs::File f;
    if (LittleFS.exists(path(slot).c_str())) f = LittleFS.open(path(slot).c_str(), "r");
    if (!f) {
      o["name"] = nullptr;  // empty slot
      continue;
    }
    // Only read the name: a filter tells ArduinoJson to skip everything else.
    JsonDocument filter, doc;
    filter["name"] = true;
    // Assign the JsonVariant itself, not a const char*: ArduinoJson 7 stores a const char* as a
    // pointer (no copy), and `doc` is gone after this loop turn. A variant is copied.
    if (!deserializeJson(doc, f, DeserializationOption::Filter(filter))) o["name"] = doc["name"];
    else o["name"] = nullptr;
    f.close();
  }
}
