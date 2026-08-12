#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ota_manager.h"
#include "secrets.h"

#define OTA_HOSTNAME "scoreboard"

namespace {

volatile bool s_ota_in_progress = false;

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
            WiFi.setSleep(false);
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
        WiFi.setSleep(false);
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
        vTaskDelay(pdMS_TO_TICKS(s_ota_in_progress ? 1 : 10));
    }
}

}  // namespace

void ota_manager_setup() {
    wait_for_wifi();

    ArduinoOTA.setHostname(OTA_HOSTNAME);

    ArduinoOTA.onStart([]() {
        s_ota_in_progress = true;
        Serial.println("[OTA] Start");
    });

    ArduinoOTA.onEnd([]() {
        s_ota_in_progress = false;
        Serial.println("[OTA] End");
    });

    ArduinoOTA.onProgress([](unsigned int, unsigned int) {});

    ArduinoOTA.onError([](ota_error_t error) {
        s_ota_in_progress = false;
        Serial.printf("\n[OTA] Error[%u]\n", static_cast<unsigned>(error));
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready. Hostname: %s.local\n", OTA_HOSTNAME);

    xTaskCreate(&ota_task, "ota_task", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
}
