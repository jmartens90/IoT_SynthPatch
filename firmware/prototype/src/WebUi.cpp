#include "WebUi.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "Api.h"
#include "App.h"
#include "MqttLink.h"
#include "WebPage.h"

static WebServer server(80);
static WebSocketsServer ws(81);  // the page connects to ws://<device>:81/
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
    // No internet = no NTP. Belgian time zone anyway, so the CSV gets local times once the
    // browser or the app has given us the time (POST /api/time, see Api.cpp).
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SynthPatch", "synthpatch");
    log.printf("WiFi: access point 'SynthPatch' (password synthpatch), open http://%s\n",
               WiFi.softAPIP().toString().c_str());
  }
  MDNS.begin("synthpatch");
  MDNS.addService("http", "tcp", 80);
  routes();
  server.begin();

  // What WiFi adds to the live values.
  api_.netInfo = [this](JsonDocument &d) {
    d["wifi"] = apMode_ ? "AP SynthPatch" : WiFi.SSID();
    d["rssi"] = apMode_ ? 0 : WiFi.RSSI();
    d["mqtt"] = mqtt.connected();
  };

  // A new browser connects: send it everything right away (to that client only), so it doesn't
  // wait for the next push. Commands still go over REST, so incoming messages are ignored.
  ws.onEvent([this](uint8_t client, WStype_t type, uint8_t *, size_t) {
    if (type != WStype_CONNECTED) return;
    Api::PushState fresh;
    api_.snapshot(fresh, [client](const String &msg) {
      String m = msg;  // sendTXT wants a non-const String&
      ws.sendTXT(client, m);
    });
  });
  ws.begin();
}

void WebUi::loop() {
  server.handleClient();
  ws.loop();
  // Pushes go out here, from the loop() task: the WebSocket library is not thread-safe, so the
  // engine task never sends anything itself.
  if (ws.connectedClients() == 0) {
    push_.notesSent = app_.voctCount();  // nobody listening (a new page gets a snapshot instead)
    return;
  }
  api_.collect(push_, [](const String &msg) {
    String m = msg;
    ws.broadcastTXT(m);
  });
}

// Every /api/... request goes to the Api, the same code the Bluetooth link uses.
void WebUi::routes() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });

  server.onNotFound([this] {
    ApiRequest r;
    r.post = server.method() == HTTP_POST;
    r.path = server.uri().c_str();
    for (int i = 0; i < server.args(); i++) {
      if (server.argName(i) == "plain") r.body = server.arg(i).c_str();  // the raw body
      else r.args[server.argName(i).c_str()] = server.arg(i).c_str();
    }
    ApiReply rep = api_.handle(r);
    server.send(rep.status, rep.type, rep.body);
  });

  // ---- HTTP only: downloads for Excel ----

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

  server.on("/log.csv", HTTP_GET, [this] {
    String csv = "time_s,temperature_c,frequency_hz,drift_cents,adc_v\n";
    app_.history().forEach([&](const Sample &x) {
      csv += String(x.t) + "," + (std::isnan(x.tempC) ? String("") : String(x.tempC, 2)) + "," + String(x.hz, 2) +
             "," + String(x.drift, 2) + "," + String(x.volts, 3) + "\n";
    });
    server.sendHeader("Content-Disposition", "attachment; filename=synthpatch-log.csv");
    server.send(200, "text/csv", csv);
  });
}
