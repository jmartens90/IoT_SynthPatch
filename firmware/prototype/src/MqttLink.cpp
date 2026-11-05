#include "MqttLink.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "App.h"

static WiFiClient tcp;
static PubSubClient client(tcp);

void MqttLink::save(const char *host, uint16_t port, const char *user, const char *pass) {
  Preferences p;
  p.begin("mqtt", false);
  p.putString("host", host);
  p.putUShort("port", port);
  p.putString("user", user);
  p.putString("pass", pass);
  p.end();
}

void MqttLink::forget() {
  Preferences p;
  p.begin("mqtt", false);
  p.clear();
  p.end();
}

void MqttLink::begin(Print &log) {
  Preferences p;
  p.begin("mqtt", false);  // read-write: creates the namespace on first boot (no NOT_FOUND error)
  host_ = p.getString("host", "");
  port_ = p.getUShort("port", 1883);
  user_ = p.getString("user", "");
  pass_ = p.getString("pass", "");
  p.end();
  enabled_ = !host_.isEmpty();
  if (!enabled_) {
    log.println("MQTT: off (shell: mqtt set <host> [port] [user] [pass])");
    return;
  }
  client.setServer(host_.c_str(), port_);
  client.setBufferSize(1024);  // the settings JSON is larger than the 256-byte default
  // PubSubClient wants a plain callback; the lambda forwards it to this object's member.
  client.setCallback([this](char *t, uint8_t *p, unsigned int n) { onMessage(t, p, n); });
  app_.noteEvent.connect([this](const NoteEvent &e) { publishEvent(e); });
  log.printf("MQTT: broker %s:%u\n", host_.c_str(), port_);
}

bool MqttLink::connected() { return enabled_ && client.connected(); }

bool MqttLink::connect() {
  String status = topic("status");
  bool ok = user_.isEmpty()
                ? client.connect("synthpatch-proto", status.c_str(), 1, true, "offline")
                : client.connect("synthpatch-proto", user_.c_str(), pass_.c_str(), status.c_str(), 1, true, "offline");
  if (!ok) return false;
  client.publish(status.c_str(), "online", true);
  client.subscribe(topic("cmd").c_str());
  settingsVersion_ = ~0u;  // publish the settings again after every reconnect
  return true;
}

void MqttLink::loop() {
  if (!enabled_ || WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) {
    // Automatic recovery, without blocking the rest: one attempt every 5 s.
    if (millis() - lastTry_ < 5000) return;
    lastTry_ = millis();
    if (!connect()) return;
  }
  client.loop();

  if (app_.settings().version() != settingsVersion_) publishSettings();

  if (millis() - lastTelemetry_ >= 2000) {
    lastTelemetry_ = millis();
    Live l = app_.live();
    JsonDocument d;
    d["temp_c"] = l.tempC;
    d["rh"] = l.humidity;
    d["hz"] = l.hz;
    d["note"] = l.note;
    d["cents_off"] = l.centsOff;
    d["drift_cents"] = l.drift;
    d["drift_per_min"] = l.driftPerMin;
    d["adc_v"] = l.volts;
    d["lfo_v"] = l.lfoV;
    d["env_v"] = l.envV;
    d["voct_v"] = l.voctV;
    d["uptime_s"] = millis() / 1000;
    String out;
    serializeJson(d, out);
    client.publish(topic("telemetry").c_str(), out.c_str());
  }
}

void MqttLink::publishSettings() {
  settingsVersion_ = app_.settings().version();
  JsonDocument d;
  app_.settings().toJson(d.to<JsonObject>());
  String out;
  serializeJson(d, out);
  client.publish(topic("settings").c_str(), out.c_str(), true);
}

void MqttLink::publishEvent(const NoteEvent &e) {
  if (!connected()) return;
  JsonDocument d;
  d["t"] = e.t;
  d["from"] = e.from;
  d["to"] = e.to;
  String out;
  serializeJson(d, out);
  client.publish(topic("event").c_str(), out.c_str());
}

void MqttLink::onMessage(char *, uint8_t *payload, unsigned len) {
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return;  // not valid JSON: ignore
  using synth::Command;
  if (d["note"].is<int>()) {
    app_.playNote(d["note"], d["on"] | 1, "mqtt");
  } else if (d["gate"].is<int>()) {
    app_.gate(d["gate"].as<int>() != 0, "mqtt");
  } else if (d["shape"].is<const char *>()) {
    app_.post(Command::setShape(d["target"] | "", d["shape"].as<const char *>(), "mqtt"));
  } else if (d["name"].is<const char *>()) {
    app_.post(Command::setParam(d["target"] | "", d["name"].as<const char *>(), d["value"] | 0.0f, "mqtt"));
  }
}
