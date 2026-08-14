#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <Arduino.h>

#include "ota_manager.h"
#include "led_strip.h"
#include "buttons.h"

#define BLINK_GPIO ((gpio_num_t) 46)

void blinkTask(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // avoid task stalls when USB serial host is not connected
    xTaskCreate(&blinkTask, "blinkTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);

    setupStrip();
    setupButtons();

    startBootCylonTask(); // spins until stopped by ota_manager_setup() on WiFi connect

    pinMode(LED_BUILTIN, OUTPUT);
    ota_manager_setup();

    stopBootCylonTask(); // safety net in case WiFi never connected

    xTaskCreate(&buttonTask, "buttonTask", 3072, NULL, 5, NULL);

    xTaskCreate(&stripTask, "stripTask", 4096, NULL, 5, NULL);
    
}
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10));
}