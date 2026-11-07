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
//       {"type":"cv","dt":20,"lfo":[..10],"env":[..],"voct":[..]}   the 3 DAC outputs, 50x per s,
//                                                                  sent in packs of 10 (5x per s)
//       {"type":"notes","list":[{"n":7,"ms":1234,"note":60,"v":2.0,"code":26214,"src":"seq"}]}
//                                                                  every note sent to V/oct
//
//   GET  /                      the GUI (WebPage.h)
//   GET/POST /api/...           all commands: the list is in Api.cpp (shared with Bluetooth)
//   GET  /log.csv, /log-full.csv?days=14   downloads for Excel (HTTP only)
//
// WiFi: joins the network stored in NVS (shell: wifi set <ssid> <pass>). If there is none, or
// it can't connect in 15 s, it opens its own access point "SynthPatch" (password synthpatch),
// so the demo also works in a classroom where the school WiFi blocks devices.
#pragma once

#include <Arduino.h>

class App;
class Api;
#include "Api.h"

class WebUi {
 public:
  WebUi(App &app, Api &api) : app_(app), api_(api) {}
  void begin(Print &log);
  void loop();
  bool apMode() const { return apMode_; }

  static void saveWifi(const char *ssid, const char *pass);
  static void forgetWifi();

 private:
  void routes();
  App &app_;  // reference members: must be set in the constructor, can never be null
  Api &api_;
  Api::PushState push_;  // what the WebSocket clients already got
  bool apMode_ = false;
};
