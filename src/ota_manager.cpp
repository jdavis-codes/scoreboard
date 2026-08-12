#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ota_manager.h"
#include "secrets.h"

#define OTA_HOSTNAME "scoreboard"

namespace {

void wait_for_wifi() {
    WiFi.mode(WIFI_STA);
    for (const auto& cred : wifi_credentials) {
        Serial.printf("[OTA] Trying to connect to WiFi SSID: %s\n", cred.ssid);
        WiFi.begin(cred.ssid, cred.password);

        uint32_t start_ms = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_ms < 10000) {
            delay(500);
            Serial.print('.');
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[OTA] Connected to WiFi SSID: %s. IP: %s\n", cred.ssid, WiFi.localIP().toString().c_str());
            return;
        } else {
            Serial.printf("[OTA] Failed to connect to WiFi SSID: %s. Trying next credentials...\n", cred.ssid);
        }
    }

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

void ota_task(void *) {
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.handle();
        }
        // Yield so the idle task can run and reset the watchdog.
        vTaskDelay(pdMS_TO_TICKS(5));
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
        // Serial writes block when no USB host is reading; skip them then,
        // and throttle so we're not printing on every chunk.
        static uint32_t last_ms = 0;
        uint32_t now = millis();
        if (Serial && now - last_ms >= 250) {
            Serial.printf("[OTA] Progress: %u%%\r", (progress * 100U) / total);
            last_ms = now;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("\n[OTA] Error[%u]\n", static_cast<unsigned>(error));
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready. Hostname: %s.local\n", OTA_HOSTNAME);

    xTaskCreate(&ota_task, "ota_task", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
}
