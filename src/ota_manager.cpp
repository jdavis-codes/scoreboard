#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

#include "ota_manager.h"
#include "secrets.h"

#define OTA_HOSTNAME "scoreboard"

namespace {

void wait_for_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[OTA] Connecting to WiFi");
    uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_ms < 30000) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[OTA] Connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[OTA] WiFi connection timeout. OTA disabled until reconnect.");
    }
}

}  // namespace

void ota_manager_setup() {
    wait_for_wifi();

    ArduinoOTA.setHostname(OTA_HOSTNAME);

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Start");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] End");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress * 100U) / total);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("\n[OTA] Error[%u]\n", static_cast<unsigned>(error));
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready. Hostname: %s.local\n", OTA_HOSTNAME);
}

void ota_manager_loop() {
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
    }
}
