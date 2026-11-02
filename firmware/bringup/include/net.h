// Stage 10 smoke test: WiFi (credentials kept in NVS, never in the repo) + a tiny HTTP API.
#pragma once
#include <Arduino.h>

namespace net {

void setCredentials(const char *ssid, const char *pass);
void forget();
bool begin(Print &out);   // connect (10 s timeout) and start HTTP + mDNS synthpatch.local
void loop();              // call often
bool connected();
void status(Print &out);

}  // namespace net
