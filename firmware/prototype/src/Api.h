// Api.h - every command and query of the device, independent of HOW it arrives.
//
// The same requests come in over two transports:
//   * WiFi:      HTTP (WebUi.cpp, port 80)          GET /api/settings, POST /api/set?target=..
//   * Bluetooth: BLE (BleLink.cpp, the Qt app)      {"id":7,"m":"GET","p":"/api/settings"}
// Both turn their request into an ApiRequest and call Api::handle(). So there is ONE list of
// commands, and the web page and the Android app can do exactly the same things.
//
// Pushes (the device sends without being asked) also live here: live values every second, the
// sequencer position when it changes, the CV scope (3 DAC outputs every 20 ms) and the V/oct
// note log. Each transport keeps its own PushState ("what did I already send?") and calls
// collect() from loop(); the WebSocket and the BLE link only differ in how they send a message.
//
// C++ lesson: SEPARATION OF CONCERNS. The Api knows WHAT the device can do, the transports know
// HOW bytes travel. Adding a third transport (MQTT, USB serial) means writing only the "how".
// std::function lets a transport hand in its own send function (a lambda) without Api knowing
// anything about WebSockets or Bluetooth.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>
#include <map>
#include <string>

class App;

struct ApiRequest {
  bool post = false;
  std::string path;                          // "/api/set"
  std::map<std::string, std::string> args;   // target=lfo1, name=rate, ...
  std::string body;                          // raw body (JSON for POST /api/seq)
  const char *source = "http";               // "http" or "ble": shown in the logs

  bool has(const char *k) const { return args.count(k) != 0; }
  std::string arg(const char *k) const {
    auto it = args.find(k);
    return it == args.end() ? std::string() : it->second;
  }
  int argInt(const char *k) const { return atoi(arg(k).c_str()); }
  float argFloat(const char *k) const { return atof(arg(k).c_str()); }

  // "/api/set?target=lfo1&name=rate&value=2" -> path + args (used by the BLE link).
  static ApiRequest parse(bool post, const std::string &pathAndQuery, const std::string &body);
};

struct ApiReply {
  int status = 200;
  const char *type = "text/plain";  // or "application/json"
  String body = "ok";
};

class Api {
 public:
  explicit Api(App &app) : app_(app) {}

  ApiReply handle(const ApiRequest &r);

  // What the transport adds about itself to the live values (WiFi name, signal...).
  std::function<void(JsonDocument &)> netInfo;

  // ---- pushes ----
  struct PushState {
    uint32_t lastLive = 0, lastSeq = 0, notesSent = 0;
    uint32_t cvSent = 0;   // scope index of the next CV sample this client still needs
    unsigned seqEpoch = ~0u;
  };
  using Send = std::function<void(const String &)>;
  void collect(PushState &st, const Send &send);  // send whatever is due for this client
  void snapshot(PushState &st, const Send &send); // a new client: send everything now

 private:
  void liveJson(JsonDocument &d);
  void seqJson(JsonDocument &d);
  void notesJson(JsonDocument &d, uint32_t after);
  static String toString(const JsonDocument &d);

  App &app_;
  static constexpr int CV_PACK = 10;  // send "cv" once 10 new samples are there (200 ms)
  static constexpr int CV_MAX = 25;   // at most 25 samples per message (catching up after a stall)
};
