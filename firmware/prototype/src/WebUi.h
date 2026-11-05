// WebUi.h - WiFi + the HTTP server: the GUI page and a small JSON REST API.
//
//   GET  /                      the GUI (WebPage.h)
//   GET  /api/live              live values (polled every second by the page)
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
  App &app_;  // a reference member: must be set in the constructor, can never be null
  bool apMode_ = false;
};
