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
};

// 'inline' prevents duplicate symbol errors across multiple .cpp includes
inline QueueHandle_t scoreQueue = NULL;

inline void initScoreQueue() {
    if (scoreQueue == NULL) {
        scoreQueue = xQueueCreate(10, sizeof(ScoreMessage));
    }
}

inline bool postScoreEvent(ScoreTarget target, ScoreAction action, int16_t value = 0) {
    initScoreQueue();
    if (scoreQueue == NULL) {
        return false;
    }
    ScoreMessage msg = { target, action, value };
    return xQueueSend(scoreQueue, &msg, 0) == pdTRUE;
}
