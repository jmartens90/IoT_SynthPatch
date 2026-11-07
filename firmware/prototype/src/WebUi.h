// WebUi.h - WiFi + the HTTP server (the GUI page and a JSON REST API) + a WebSocket server.
//
// Two ways to talk to the device:
//   * REST (port 80): the browser ASKS, the ESP answers. Used for everything the page does
//     (settings, patches, notes) and for anyone who wants the data (curl, Excel, a script).
//   * WebSocket (port 81): one connection that stays open, so the ESP can SEND without being
//     asked. It pushes the live values every second and the sequencer position whenever it
//     starts, stops, restarts or changes tempo (+ every 5 s against clock drift). No polling.
//       {"type":"live", ...same fields as /api/live...}
//       {"type":"seq","run":true,"pos":0,"len":8,"step_ms":125,"in_ms":0,"tr":4}
//
//   GET  /                      the GUI (WebPage.h)
//   GET  /api/live              live values (the page gets them over the WebSocket)
//   GET  /api/history           last 10 minutes, one sample per second
//   GET  /api/events            last note events
//   GET  /api/settings          all LFO/envelope parameters with min/max/unit
//   POST /api/set?target=lfo1&name=rate&value=2.5
//   POST /api/shape?target=lfo1&shape=saw
//   POST /api/gate?on=1|0
//   POST /api/note?n=60&on=1&tone=1   V/oct + gate (+ the test tone)
//   POST /api/tone?hz=440             test tone, 0 = off
//   GET  /log.csv               the history as CSV, for Excel
//
// WiFi: joins the network stored in NVS (shell: wifi set <ssid> <pass>). If there is none, or
// it can't connect in 15 s, it opens its own access point "SynthPatch" (password synthpatch),
// so the demo also works in a classroom where the school WiFi blocks devices.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class App;

class WebUi {
 public:
  explicit WebUi(App &app) : app_(app) {}
  void begin(Print &log);
  void loop();
  bool apMode() const { return apMode_; }

  static void saveWifi(const char *ssid, const char *pass);
  static void forgetWifi();

 private:
  void routes();
  void liveJson(JsonDocument &d);  // the live values, for /api/live and the WebSocket
  void seqJson(JsonDocument &d);   // the sequencer position, for /api/seq/pos and the WebSocket
  void push();                     // called from loop(): send over the WebSocket when needed
  uint32_t lastLivePush_ = 0, lastSeqPush_ = 0;
  unsigned seqEpoch_ = ~0u;        // the sequencer epoch we last pushed (~0 = none yet)
  App &app_;  // a reference member: must be set in the constructor, can never be null
  bool apMode_ = false;
};
