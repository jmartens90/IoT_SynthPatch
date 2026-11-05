// MqttLink.h - telemetry out, commands in, over MQTT (e.g. Mosquitto on your laptop).
//
// Topics (<id> = "proto" by default):
//   synthpatch/<id>/status     "online" / "offline"   retained; "offline" is the LAST WILL, the
//                                                     broker sends it when the ESP disappears
//   synthpatch/<id>/telemetry  JSON every 2 s: temperature, Hz, note, cents, drift, ADC volts
//   synthpatch/<id>/event      JSON per note change
//   synthpatch/<id>/settings   JSON of all LFO/envelope settings, retained, on every change
//   synthpatch/<id>/cmd        JSON in:  {"target":"lfo1","name":"rate","value":2}
//                                        {"target":"lfo1","shape":"saw"}
//                                        {"gate":1}   {"note":60,"on":1}
//
// Broker address is stored in NVS from the shell: mqtt set <host> [port] [user] [pass].
// No broker configured = MQTT is simply off; the web GUI works without it.
#pragma once

#include <Arduino.h>

class App;
struct NoteEvent;

class MqttLink {
 public:
  explicit MqttLink(App &app) : app_(app) {}
  void begin(Print &log);
  void loop();
  bool connected();

  static void save(const char *host, uint16_t port, const char *user, const char *pass);
  static void forget();

 private:
  bool connect();
  void onMessage(char *topic, uint8_t *payload, unsigned len);
  void publishSettings();
  void publishEvent(const NoteEvent &e);
  String topic(const char *leaf) const { return "synthpatch/" + id_ + "/" + leaf; }

  App &app_;
  String host_, user_, pass_, id_ = "proto";
  uint16_t port_ = 1883;
  bool enabled_ = false;
  uint32_t lastTry_ = 0, lastTelemetry_ = 0;
  unsigned settingsVersion_ = ~0u;
};
