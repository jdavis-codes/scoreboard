#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct GameModeMessage {
    char current_mode[16];
    char active_event[16];
    char event_triggered_at[32];
};

inline QueueHandle_t gameModeQueue = NULL;

inline void initGameModeQueue() {
    if (gameModeQueue == NULL) {
        gameModeQueue = xQueueCreate(5, sizeof(GameModeMessage));
    }
}

inline bool postGameModeEvent(const char* mode, const char* event, const char* triggeredAt) {
    initGameModeQueue();
    if (gameModeQueue == NULL) {
        Serial.println("[GameModeQueue] Queue is null; dropping mode/event update");
        return false;
    }

    GameModeMessage msg;
    strncpy(msg.current_mode, mode ? mode : "NORMAL", sizeof(msg.current_mode) - 1);
    msg.current_mode[sizeof(msg.current_mode) - 1] = '\0';

    strncpy(msg.active_event, event ? event : "", sizeof(msg.active_event) - 1);
    msg.active_event[sizeof(msg.active_event) - 1] = '\0';

    strncpy(msg.event_triggered_at, triggeredAt ? triggeredAt : "", sizeof(msg.event_triggered_at) - 1);
    msg.event_triggered_at[sizeof(msg.event_triggered_at) - 1] = '\0';

    BaseType_t ok = xQueueSend(gameModeQueue, &msg, 0);
    if (ok != pdTRUE) {
        Serial.printf("[GameModeQueue] Queue full; dropped update mode=%s event=%s triggered_at=%s\n",
                      msg.current_mode,
                      msg.active_event,
                      msg.event_triggered_at);
        return false;
    }

    Serial.printf("[GameModeQueue] Enqueued update mode=%s event=%s triggered_at=%s\n",
                  msg.current_mode,
                  msg.active_event,
                  msg.event_triggered_at);
    return true;
}
