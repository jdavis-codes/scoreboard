#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum ScoreTarget {
    SCORE_TARGET_HOME,
    SCORE_TARGET_AWAY,
    SCORE_TARGET_BOTH
};

enum ScoreAction {
    SCORE_ACTION_INCREMENT,
    SCORE_ACTION_DECREMENT,
    SCORE_ACTION_SET,
    SCORE_ACTION_RESET
};

struct ScoreMessage {
    ScoreTarget target;
    ScoreAction action;
    int16_t value; // For SCORE_ACTION_SET or custom increment step
    bool isWeb = false;
};

struct ScoreUpdate {
    int16_t home;
    int16_t away;
};

// inline prevents duplicate symbols
inline QueueHandle_t scoreQueue = NULL;
inline QueueHandle_t supabaseQueue = NULL;

inline void initScoreQueue() {
    if (scoreQueue == NULL) {
        scoreQueue = xQueueCreate(10, sizeof(ScoreMessage));
    }
}

inline void initSupabaseQueue() {
    if (supabaseQueue == NULL) {
        supabaseQueue = xQueueCreate(10, sizeof(ScoreUpdate));
    }
}

inline bool postScoreEvent(ScoreTarget target, ScoreAction action, int16_t value = 0) {
    initScoreQueue();
    if (scoreQueue == NULL) {
        return false;
    }
    ScoreMessage msg = { target, action, value, false };
    return xQueueSend(scoreQueue, &msg, 0) == pdTRUE;
}

inline bool postScoreEventFromWeb(ScoreTarget target, ScoreAction action, int16_t value = 0) {
    initScoreQueue();
    if (scoreQueue == NULL) {
        return false;
    }
    ScoreMessage msg = { target, action, value, true };
    return xQueueSend(scoreQueue, &msg, 0) == pdTRUE;
}
