/**
 * @file bangle.c
 * @author Artem (artallo@mail.ru)
 * @brief 
 * @version 0.1
 * @date 2020-10-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define BUTTON_GPIO 27

void vTaskButtonCheck(void *pvParameters);

void app_main(void)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BUTTON_GPIO);
    /* Set the GPIO as input */
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    xTaskCreate(vTaskButtonCheck, "ButtonCheck", 32, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1) {
        printf("Checking button state: ");
        printf("%d\n", gpio_get_level(BUTTON_GPIO));
        for (int i = 2; i >= 0; i--) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    }
}


void vTaskButtonCheck(void *pvParameters) {
    uint8_t duration = 0;
    //see realization how measure key press duration in project SD reader/checker for Nikolay domofon
    while(1) {
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}