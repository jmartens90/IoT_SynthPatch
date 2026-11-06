#include "WebUi.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "App.h"
#include "MqttLink.h"
#include "WebPage.h"

using synth::Command;

static WebServer server(80);
extern MqttLink mqtt;  // defined in main.cpp, only asked "are you connected?"

void WebUi::saveWifi(const char *ssid, const char *pass) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}

void WebUi::forgetWifi() {
  Preferences p;
  p.begin("wifi", false);
  p.clear();
  p.end();
}

void WebUi::begin(Print &log) {
  Preferences p;
  p.begin("wifi", false);  // read-write: creates the namespace on first boot (no NOT_FOUND error)
  String ssid = (p.isKey("ssid") ? p.getString("ssid") : String());
  String pass = (p.isKey("pass") ? p.getString("pass") : String());
  p.end();

  WiFi.setHostname("synthpatch");
  if (!ssid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);  // automatic recovery when the router drops out
    WiFi.begin(ssid.c_str(), pass.c_str());
    log.printf("WiFi: connecting to %s", ssid.c_str());
    for (uint32_t t0 = millis(); WiFi.status() != WL_CONNECTED && millis() - t0 < 15000;) {
      delay(250);
      log.print(".");
    }
    log.println();
  }
  if (WiFi.status() == WL_CONNECTED) {
    log.printf("WiFi: http://synthpatch.local  or  http://%s\n", WiFi.localIP().toString().c_str());
    // Real time from the internet (NTP), in Belgian time with summer/winter time. The ESP keeps
    // it up to date by itself from now on. Needed for the time stamps in the flash log.
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
  } else {
    apMode_ = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SynthPatch", "synthpatch");
    log.printf("WiFi: access point 'SynthPatch' (password synthpatch), open http://%s\n",
               WiFi.softAPIP().toString().c_str());
  }
  MDNS.begin("synthpatch");
  MDNS.addService("http", "tcp", 80);
  routes();
  server.begin();
}

void WebUi::loop() { server.handleClient(); }

static void sendJson(const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebUi::routes() {
  // Each route is a lambda. [this] captures the WebUi object so the lambda can use app_.
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });

  server.on("/api/live", HTTP_GET, [this] {
    Live l = app_.live();
    JsonDocument d;
    d["temp_c"] = l.tempC;  // NAN becomes null in the JSON
    d["temp_sensor"] = l.tempSensor;
    d["rh"] = l.humidity;
    d["hz"] = l.hz;
    d["note"] = l.note;
    d["cents_off"] = l.centsOff;
    d["drift"] = l.drift;
    d["drift_per_min"] = l.driftPerMin;
    d["pitch_state"] = l.pitchState;
    d["volts"] = l.volts;
    d["lfo_v"] = l.lfoV;
    d["env_v"] = l.envV;
    d["env_out"] = l.envOut;
    d["voct_v"] = l.voctV;
    d["voct_note"] = l.voctNote;
    d["tone_hz"] = l.toneHz;
    d["events_total"] = app_.noteEventCount();
    d["uptime"] = millis() / 1000;
    d["time"] = App::timeValid() ? static_cast<uint32_t>(time(nullptr)) : 0;
    d["log_count"] = app_.log().count();
    d["wifi"] = apMode_ ? "AP SynthPatch" : WiFi.SSID();
    d["rssi"] = apMode_ ? 0 : WiFi.RSSI();
    d["mqtt"] = mqtt.connected();
    sendJson(d);
  });

  server.on("/api/history", HTTP_GET, [this] {
    // Built by hand instead of with JsonDocument: 600 samples would need ~50 kB of RAM as a
    // document, as text it is streamed into one String.
    String t, temp, rh, hz, drift, volts;
    auto add = [](String &s, float v, int dec) {
      if (s.length()) s += ',';
      if (std::isnan(v)) s += "null"; else s += String(v, dec);
    };
    app_.history().forEach([&](const Sample &x) {
      if (t.length()) t += ',';
      t += x.t;
      add(temp, x.tempC, 2);
      add(rh, x.rh, 1);
      add(hz, x.hz, 2);
      add(drift, x.drift, 2);
      add(volts, x.volts, 3);
    });
    server.send(200, "application/json",
                "{\"t\":[" + t + "],\"temp\":[" + temp + "],\"rh\":[" + rh + "],\"hz\":[" + hz +
                    "],\"drift\":[" + drift + "],\"volts\":[" + volts + "]}");
  });

  // Stored history from flash: /api/log?hours=336&points=800 (336 h = 14 days).
  // Same field names as /api/history, but t is real time (unix seconds) and long ranges are
  // averaged down to at most `points` points, so the browser gets ~40 kB instead of megabytes.
  server.on("/api/log", HTTP_GET, [this] {
    float hours = server.hasArg("hours") ? server.arg("hours").toFloat() : 24;
    uint32_t points = server.hasArg("points") ? server.arg("points").toInt() : 800;
    uint32_t since = App::timeValid() ? time(nullptr) - static_cast<uint32_t>(hours * 3600) : 0;
    String t, temp, rh, hz, drift, volts;
    auto add = [](String &s, float v, int dec) {
      if (s.length()) s += ',';
      if (std::isnan(v)) s += "null"; else s += String(v, dec);
    };
    app_.log().forEach(since, constrain(points, 10u, 3000u), [&](const LogPoint &p) {
      if (t.length()) t += ',';
      t += p.t;
      add(temp, p.tempC, 2);
      add(rh, p.rh, 1);
      add(hz, p.hz, 2);
      add(drift, p.drift, 2);
      add(volts, p.volts, 3);
    });
    server.send(200, "application/json",
                "{\"t\":[" + t + "],\"temp\":[" + temp + "],\"rh\":[" + rh + "],\"hz\":[" + hz +
                    "],\"drift\":[" + drift + "],\"volts\":[" + volts + "]}");
  });

  // Full-resolution CSV of the flash log: /log-full.csv?days=14. Sent in pieces ("chunked"),
  // because 14 days = 120 000 lines would not fit in RAM as one String.
  server.on("/log-full.csv", HTTP_GET, [this] {
    float days = server.hasArg("days") ? server.arg("days").toFloat() : 14;
    uint32_t since = App::timeValid() ? time(nullptr) - static_cast<uint32_t>(days * 86400) : 0;
    server.sendHeader("Content-Disposition", "attachment; filename=synthpatch-log-full.csv");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/csv", "");
    String chunk = "time,temperature_c,humidity_rh,frequency_hz,drift_cents,adc_v\n";
    auto num = [](float v, int dec) { return std::isnan(v) ? String("") : String(v, dec); };
    app_.log().forEach(since, UINT32_MAX, [&](const LogPoint &p) {
      char when[24];
      time_t tt = p.t;
      strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&tt));
      chunk += String(when) + "," + num(p.tempC, 2) + "," + num(p.rh, 1) + "," + num(p.hz, 2) + "," +
               num(p.drift, 2) + "," + num(p.volts, 3) + "\n";
      if (chunk.length() > 4000) {
        server.sendContent(chunk);
        chunk = "";
      }
    });
    server.sendContent(chunk);
    server.sendContent("");  // empty chunk = end of the response
  });

  server.on("/api/events", HTTP_GET, [this] {
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    app_.noteEvents().forEach([&](const NoteEvent &e) {
      JsonObject o = a.add<JsonObject>();
      o["t"] = e.t;
      o["from"] = e.from;
      o["to"] = e.to;
    });
    sendJson(d);
  });

  server.on("/api/settings", HTTP_GET, [this] {
    JsonDocument d;
    app_.settings().toJson(d.to<JsonObject>());
    sendJson(d);
  });

  // Changes go through the engine's command queue, exactly like MQTT and (later) the TFT.
  server.on("/api/set", HTTP_POST, [this] {
    app_.post(Command::setParam(server.arg("target").c_str(), server.arg("name").c_str(),
                                server.arg("value").toFloat(), "http"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/shape", HTTP_POST, [this] {
    app_.post(Command::setShape(server.arg("target").c_str(), server.arg("shape").c_str(), "http"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/gate", HTTP_POST, [this] {
    app_.gate(server.arg("on") == "1", "http");
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/note", HTTP_POST, [this] {
    int n = server.arg("n").toInt();
    bool on = server.arg("on") == "1";
    app_.playNote(n, on, "http");
    if (on && server.arg("tone") == "1") app_.setToneHz(440.0f * powf(2.0f, (n - 69) / 12.0f));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/tone", HTTP_POST, [this] {
    app_.setToneHz(server.arg("hz").toFloat());
    server.send(200, "text/plain", "ok");
  });

  server.on("/log.csv", HTTP_GET, [this] {
    String csv = "time_s,temperature_c,frequency_hz,drift_cents,adc_v\n";
    app_.history().forEach([&](const Sample &x) {
      csv += String(x.t) + "," + (std::isnan(x.tempC) ? String("") : String(x.tempC, 2)) + "," + String(x.hz, 2) +
             "," + String(x.drift, 2) + "," + String(x.volts, 3) + "\n";
    });
    server.sendHeader("Content-Disposition", "attachment; filename=synthpatch-log.csv");
    server.send(200, "text/csv", csv);
  });

  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
}
