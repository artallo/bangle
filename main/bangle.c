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

void vTaskButtonCheck(void *pvParameters);

static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    if(esp_timer_get_time() - last_micros > 50 * 1000) {
        //xSemaphoreGiveFromISR()
        //direct task notification
        


    }
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void app_main(void)
{
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


    /*xTaskCreate(vTaskButtonCheck, "ButtonCheck", 32, NULL, 1, NULL);*/
    //vTaskStartScheduler();
    while(1) {
        printf("Checking button state: ");
        printf("%d\n", gpio_get_level(GPIO_BUTTON));
        for (int i = 2; i >= 0; i--) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    }
}


/*void vTaskButtonCheck(void *pvParameters) {
    uint8_t duration = 0;
    //see realization how measure key press duration in project SD reader/checker for Nikolay domofon
    while(1) {
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}*/