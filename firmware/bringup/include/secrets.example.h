// Not used by the bring-up firmware: WiFi credentials are entered over serial
// (`wifi set <ssid> <pass>`) and stored in the ESP32's NVS, so they never touch git.
// The app firmware will copy this file to secrets.h (git-ignored) for MQTT broker
// credentials and the TLS CA certificate.
#pragma once
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 8883
#define MQTT_USER "synthpatch-01"
#define MQTT_PASS "change-me"
