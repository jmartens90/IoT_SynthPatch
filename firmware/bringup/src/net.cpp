#include "net.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "audio_in.h"
#include "cvbus.h"
#include "inputs.h"
#include "pins.h"
#include "sht45.h"

namespace net {

static WebServer server(80);
static bool started = false;

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SynthPatch bring-up</title>
<style>body{font:15px system-ui;margin:16px;background:#111;color:#eee}td{padding:2px 10px}
input{width:70px}canvas{background:#000;width:100%;max-width:640px;height:200px}</style></head>
<body><h2>IoT_SYNTHPATCH bring-up</h2>
<table id="t"></table>
<h3>Set CV</h3>
<select id="o"></select> <input id="v" type="number" step="0.1" value="5"> V
<button onclick="setcv()">Set</button>
<h3>Input J26 scope</h3><canvas id="c" width="640" height="200"></canvas>
<script>
const jacks=["J10","J11","J12","J13","J14","J15","J16","J17","J23","J22"];
o.innerHTML=jacks.map(j=>`<option>${j}</option>`).join("");
async function tick(){
 const s=await (await fetch("/api/status")).json();
 t.innerHTML=Object.entries(s).map(([k,v])=>`<tr><td>${k}</td><td>${typeof v=="object"?JSON.stringify(v):v}</td></tr>`).join("");
 const w=await (await fetch("/api/scope?ch=0")).json();
 const g=c.getContext("2d");g.fillStyle="#000";g.fillRect(0,0,640,200);g.strokeStyle="#3f3";g.beginPath();
 const r=Math.max(1,...w.map(Math.abs));w.forEach((y,i)=>g.lineTo(i*640/w.length,100-y/r*95));g.stroke();
}
async function setcv(){await fetch(`/api/cv?out=${o.value}&volts=${v.value}`,{method:"POST"});tick();}
setInterval(tick,1000);tick();
</script></body></html>)HTML";

static void handleStatus() {
  auto t = sht45::read();
  auto in = inputs::read();
  auto a0 = audio::stats(0), a1 = audio::stats(1);
  String j = "{";
  j += "\"uptime_s\":" + String(millis() / 1000);
  j += ",\"rssi_dbm\":" + String(WiFi.RSSI());
  j += ",\"heap_free\":" + String(ESP.getFreeHeap());
  if (t.ok) {
    j += ",\"temperature_c\":" + String(t.tempC, 2);
    j += ",\"humidity_rh\":" + String(t.rh, 1);
  } else {
    j += ",\"sht45_error\":\"" + String(t.error) + "\"";
  }
  j += ",\"gates\":" + String(in.gates);
  j += ",\"mutes\":" + String(in.mutes);
  j += ",\"encoder\":" + String(inputs::encoderCount());
  if (audio::running()) {
    j += ",\"in_j26\":{\"freq_hz\":" + String(a0.freqHz, 2) + ",\"vpp\":" + String(a0.vpp, 3) + "}";
    j += ",\"in_j28\":{\"freq_hz\":" + String(a1.freqHz, 2) + ",\"vpp\":" + String(a1.vpp, 3) + "}";
  }
  j += ",\"cv_v\":[";
  for (int i = 0; i < NUM_CV; i++) {
    if (i) j += ",";
    j += String(cvbus::cvVoltsFromCode(i, cvbus::cvCode(i)), 2);
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleCv() {
  String out = server.arg("out");
  float volts = server.arg("volts").toFloat();
  for (int i = 0; i < NUM_CV; i++) {
    if (out == CV_OUTS[i].jack) {
      if (volts < (CV_OUTS[i].bipolar ? -5.0f : 0.0f) || volts > (CV_OUTS[i].bipolar ? 5.0f : 10.0f)) {
        server.send(400, "text/plain", "out of range");
        return;
      }
      cvbus::cvSetVolts(i, volts);
      server.send(200, "text/plain", "ok");
      return;
    }
  }
  for (int i = 0; i < 2; i++) {
    if (out == VOCT_JACK[i]) {
      cvbus::voctSetVolts(i, volts);
      server.send(200, "text/plain", "ok");
      return;
    }
  }
  server.send(404, "text/plain", "unknown output");
}

static void handleScope() {
  static float buf[audio::BLOCK];
  int ch = server.arg("ch").toInt();
  int n = audio::running() ? audio::snapshot(ch, buf, audio::BLOCK) : 0;
  String j = "[";
  const int points = 256;
  for (int k = 0; k < points && n; k++) {
    if (k) j += ",";
    j += String(buf[k * 2], 3);  // first ~10 ms at 24 kHz effective
  }
  j += "]";
  server.send(200, "application/json", j);
}

void setCredentials(const char *ssid, const char *pass) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}

void forget() {
  Preferences p;
  p.begin("wifi", false);
  p.clear();
  p.end();
}

bool begin(Print &out) {
  Preferences p;
  p.begin("wifi", true);
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  p.end();
  if (ssid.isEmpty()) {
    out.println("  no credentials: use  wifi set <ssid> <password>");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("synthpatch");
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  out.printf("  connecting to %s", ssid.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(250);
    out.print(".");
  }
  out.println();
  if (WiFi.status() != WL_CONNECTED) {
    out.println("  failed (2.4 GHz network? antenna area clear of copper/metal?)");
    return false;
  }
  if (!started) {
    MDNS.begin("synthpatch");
    MDNS.addService("http", "tcp", 80);
    server.on("/", [] { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/api/status", handleStatus);
    server.on("/api/cv", HTTP_POST, handleCv);
    server.on("/api/scope", handleScope);
    server.begin();
    started = true;
  }
  status(out);
  return true;
}

void loop() {
  if (started) server.handleClient();
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

void status(Print &out) {
  if (!connected()) {
    out.println("  WiFi not connected");
    return;
  }
  out.printf("  IP %s  RSSI %d dBm  ch %d  http://synthpatch.local/\n", WiFi.localIP().toString().c_str(),
             WiFi.RSSI(), WiFi.channel());
}

}  // namespace net
