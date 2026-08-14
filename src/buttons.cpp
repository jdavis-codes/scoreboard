#include "buttons.h"
#include "score_event.h"
#include <Arduino.h>

// Define Pins (Adjust GPIOs according to your board layout)

#define BTN1_R_PIN D11
#define BTN1_G_PIN D10
#define BTN1_B_PIN D9
#define BTN1_PIN D8

// #define BTN2_PIN       16
// #define BTN2_R_PIN     17
// #define BTN2_G_PIN     18
// #define BTN2_B_PIN     19

#define DEBOUNCE_TIME_MS 30
#define LONG_PRESS_TIME_MS 5000
#define DOUBLE_TAP_WINDOW_MS 300  // Max time between 1st release and 2nd press


// LEDC PWM Channels (0 to 5 for 6 LED pins)
#define LEDC_TIMER_BIT 8  // 8-bit resolution (0-255)
#define LEDC_FREQ_HZ 5000 // 5kHz PWM frequency

enum ButtonFsmState {
    STATE_IDLE,
    STATE_PRESSED_1,
    STATE_WAIT_DOUBLE_TAP,
    STATE_PRESSED_2
};

struct ButtonState
{
    ButtonId id;
    uint8_t pin;
    ScoreTarget target;
    ButtonFsmState state = STATE_IDLE;
    uint32_t pressStartMs = 0;
    uint32_t releaseTimeMs = 0;
    bool longPressFired = false;
};

static ButtonState buttons[1] = {
    {BUTTON_HOME, BTN1_PIN, SCORE_TARGET_HOME}
};

static const size_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// Translate button gestures to global score commands
static void dispatchButtonScoreEvent(ScoreTarget target, ButtonEvent event) {
    switch (event) {
    case BUTTON_SINGLE_TAP:
        postScoreEvent(target, SCORE_ACTION_INCREMENT);
        break;
    case BUTTON_DOUBLE_TAP:
        postScoreEvent(target, SCORE_ACTION_DECREMENT);
        break;
    case BUTTON_LONG_PRESSED:
        postScoreEvent(target, SCORE_ACTION_RESET);
        break;
    default:
        break;
    }
}

void setButtonColor(ButtonId btn, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t baseChannel = (btn == BUTTON_HOME) ? 0 : 3;

    // For Common Cathode: write duty directly.
    // For Common Anode: invert with (255 - r).
    ledcWriteChannel(baseChannel + 0, 255 - r);
    ledcWriteChannel(baseChannel + 1, 255 - g);
    ledcWriteChannel(baseChannel + 2, 255 - b);
}

void setupButtons()
{
    // Configure input pins
    pinMode(BTN1_PIN, INPUT_PULLUP);
    // pinMode(BTN2_PIN, INPUT_PULLUP);

    // Configure PWM output channels for RGB LEDs using Arduino ESP32 LEDC API
    ledcAttachChannel(BTN1_R_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 0);
    ledcAttachChannel(BTN1_G_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 1);
    ledcAttachChannel(BTN1_B_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 2);

    // ledcAttachChannel(BTN2_R_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 3);
    // ledcAttachChannel(BTN2_G_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 4);
    // ledcAttachChannel(BTN2_B_PIN, LEDC_FREQ_HZ, LEDC_TIMER_BIT, 5);

    // Default LED states (e.g., dim blue standby)
    setButtonColor(BUTTON_HOME, 0, 0, 30);
    // setButtonColor(BUTTON_2, 0, 0, 30);

    // Ensure score queue exists and spawn task
    initScoreQueue();
}

void buttonTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Run every 10ms

    while (1)
    {
        uint32_t now = millis();

        for (size_t i = 0; i < NUM_BUTTONS; i++)
        {
            ButtonState &btn = buttons[i];
            bool isDown = (digitalRead(btn.pin) == LOW); // Assuming Active LOW

            switch (btn.state)
            {

            case STATE_IDLE:
                if (isDown)
                {
                    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
                    if (digitalRead(btn.pin) == LOW)
                    {
                        btn.state = STATE_PRESSED_1;
                        btn.pressStartMs = now;
                        btn.longPressFired = false;

                        // Visual feedback: Light up button
                        setButtonColor(btn.id, 0, 255, 0); // Green
                    }
                }
                break;

            case STATE_PRESSED_1:
                if (!isDown)
                {
                    // Released first time
                    btn.releaseTimeMs = now;
                    btn.state = STATE_WAIT_DOUBLE_TAP;
                    setButtonColor(btn.id, 0, 0, 30); // Dim standby
                }
                else if (!btn.longPressFired && (now - btn.pressStartMs >= LONG_PRESS_TIME_MS))
                {
                    // Long press detected
                    btn.longPressFired = true;
                    dispatchButtonScoreEvent(btn.target, BUTTON_LONG_PRESSED);
                    setButtonColor(btn.id, 255, 255, 255); // white feedback
                }
                break;

            case STATE_WAIT_DOUBLE_TAP:
                if (isDown)
                {
                    // Pressed a second time within double-tap window!
                    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
                    if (digitalRead(btn.pin) == LOW)
                    {
                        btn.state = STATE_PRESSED_2;
                        btn.pressStartMs = now;

                        // Send double tap event
                        dispatchButtonScoreEvent(btn.target, BUTTON_DOUBLE_TAP);

                        // Visual feedback: Flash Magenta for double tap
                        setButtonColor(btn.id, 255, 0, 255);
                    }
                }
                else if (now - btn.releaseTimeMs > DOUBLE_TAP_WINDOW_MS)
                {
                    // Timeout expired without 2nd press -> It's a Single Tap
                    if (!btn.longPressFired)
                    {
                        dispatchButtonScoreEvent(btn.target, BUTTON_SINGLE_TAP);
                    }
                    btn.state = STATE_IDLE;
                }
                break;

            case STATE_PRESSED_2:
                if (!isDown)
                {
                    // Released second press -> back to IDLE
                    btn.state = STATE_IDLE;
                    setButtonColor(btn.id, 0, 0, 255);
                }
                break;
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
