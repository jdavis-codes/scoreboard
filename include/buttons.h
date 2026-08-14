#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum ButtonId
{
    BUTTON_HOME,
    BUTTON_AWAY,
    BUTTON_RESET
};

enum ButtonEvent
{
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_SINGLE_TAP,
    BUTTON_DOUBLE_TAP,
    BUTTON_LONG_PRESSED
};

// Initialize hardware and launch FreeRTOS task
void setupButtons();
void buttonTask(void *pvParameters);

// Color helper function (RGB 0-255)
void setButtonColor(ButtonId btn, uint8_t r, uint8_t g, uint8_t b);