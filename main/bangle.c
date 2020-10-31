/**
 * @file bangle.c
 * @author Artem (artallo@mail.ru)
 * @brief Прошивка браслета
 * @version 0.1
 * @date 2020-10-28
 * 
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

typedef enum {  ///< Menu modes
    POWER_ON_MODE,
    BACKGROUND_MODE
} menu_mode_t;

#define GPIO_BUTTON 27 ///< Button

volatile int64_t last_micros = 0;   ///< Delay measuring in usec 
static TaskHandle_t xTaskButtonPressedHandle = NULL;    ///< vTaskButtonPressed task handler
static TaskHandle_t xTaskPowerOnModeHandle = NULL;  ///< vTaskMenuModePowerOnMode task handler
static TaskHandle_t xTaskBackgroundHandle = NULL;
menu_mode_t MenuCurrentMode = POWER_ON_MODE;

void vTaskButtonPressed(void *pvParameters);
void vTaskMenu(void *pvParameters);

/**
 * @brief Button pressed ISR
 * 
 * This ISR direct notify task vTaskButtonPressed in which the duration of the pressing is determined
 * @param arg - GPIO number that trigger interrupt
 */
static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    if (esp_timer_get_time() - last_micros > 80 * 1000) {    //80ms debounce delay
        last_micros = esp_timer_get_time(); //update last_micros
        //Give direct to task notification (it's faster, instead of using binary semaphore)
        vTaskNotifyGiveFromISR(xTaskButtonPressedHandle, NULL);
    }
}

void app_main(void)
{
    //Create task to handle button pressing
    xTaskCreate(vTaskButtonPressed, "ButtonPressed", 2048, NULL, 1, &xTaskButtonPressedHandle);
    //Configure the IOMUX register for pad BLINK_GPIO */
    gpio_pad_select_gpio(GPIO_BUTTON);
    //Set the GPIO as input
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_BUTTON, GPIO_INTR_NEGEDGE);
    //install gpio isr service 0 - default mode
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BUTTON, button_isr_handler, (void*) GPIO_BUTTON);
    
    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void vTaskMenu(void *pvParameters) {
    while (1) {
        if (MenuCurrentMode == POWER_ON_MODE) {
            xTaskNotifyGive(xTaskPowerOnModeHandle);
        } else if (MenuCurrentMode == BACKGROUND_MODE) {
            xTaskNotifyGive(xTaskBackgroundHandle);
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    
}

/**
 * @brief Task for determining the duration of a button press
 * 
 * Task in which the duration of the button pressing is determined
 * @param pvParameters 
 */
void vTaskButtonPressed(void *pvParameters) {
    uint8_t duration = 0;
    while (1) {
        //Block indefinitely to wait for a notification, pdTRUE means task notification is being used as a binary semaphore, so the notification value is cleared
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        //Check button is pressed
        if (gpio_get_level(GPIO_BUTTON) == 0) {
            int64_t last_micros = esp_timer_get_time(); //get current time for pressing delay measuring
            printf("Button pressed for.. ");
            //while button is pressed and delay < 2s
            while ((gpio_get_level(GPIO_BUTTON) == 0) && (esp_timer_get_time() - last_micros <= 2000 * 1000)) {
                duration++; //update duration counter
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }   //exit when button released or pressing delay > 2s
            printf("%dms\n", duration * 50);
            duration = 0;
        }
    }
}