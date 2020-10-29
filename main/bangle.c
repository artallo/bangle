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

#define GPIO_BUTTON 27 ///< button

volatile int64_t last_micros = 0;
volatile bool in_isr_begin = false;
volatile bool in_isr_mid = false;
static TaskHandle_t xTaskButtonPressedHandle = NULL;

void vTaskButtonPressed(void *pvParameters);

static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    in_isr_begin = true;
    if(esp_timer_get_time() - last_micros > 50 * 1000) {
        //Direct to task notification
        in_isr_mid = true;
        vTaskNotifyGiveFromISR(xTaskButtonPressedHandle, NULL);
        last_micros = esp_timer_get_time();
    }
    //xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void app_main(void)
{
    xTaskCreate(vTaskButtonPressed, "ButtonPressed", 2048, NULL, 1, &xTaskButtonPressedHandle);
    /* Configure the IOMUX register for pad BLINK_GPIO */
    gpio_pad_select_gpio(GPIO_BUTTON);
    /* Set the GPIO as input */
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_BUTTON, GPIO_INTR_NEGEDGE);
    //install gpio isr service 0 - default mode
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BUTTON, button_isr_handler, (void*) GPIO_BUTTON);

    
    //vTaskStartScheduler();
    
    while(1) {
        /*printf("Checking button state: ");
        printf("%d\n", gpio_get_level(GPIO_BUTTON));
        for (int i = 2; i >= 0; i--) {
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }*/
        printf("in isr begin: %d", in_isr_begin);
        printf(", in isr mid: %d\n", in_isr_mid);
        in_isr_begin = false;
        in_isr_mid = false;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}


void vTaskButtonPressed(void *pvParameters) {
    uint8_t duration = 0;
    //see realization how measure key press duration in project SD reader/checker for Nikolay domofon
    while(1) {
        printf("in task while\n");
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        printf("Button pressed\n");
        /*if (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }*/
    }
}