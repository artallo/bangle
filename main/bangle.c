/**
 * @file bangle.c
 * @author Artem (artallo@mail.ru)
 * @brief Прошивка браслета
 * @version 0.1
 * @date 2020-10-28
 * 
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

///Type which describe all working modes
typedef enum {  
    POWER_ON_MODE,
    BACKGROUND_MODE,
    INITIALIZATION_MODE,
    SENSOR_CHECK_MODE,
    DISCONNECTION_MODE,
    DATA_TRANSFER_MODE,
    DATA_COLLECTION_MODE,
    SLEEP_MODE,
    DEVELOPER_MODE_MODE
} menu_mode_t; 

#define GPIO_BUTTON 27 ///< Button pin

//LOGs
static const char* TAG_TASK = "vTasks";

volatile int64_t last_micros = 0;   ///< Delay measuring in usec 
menu_mode_t MenuCurrentMode = POWER_ON_MODE; ///< Current working mode, starts from POWER_ON_MODE

//Handlers
static TaskHandle_t xTaskButtonPressedHandle = NULL; ///< vTaskButtonPressed task handler
static TaskHandle_t xTaskModeSwitcherHandle = NULL;  ///< vTaskModeSwitcher task handler
static TaskHandle_t xTaskPowerOnModeHandle = NULL;   ///< vTaskPowerOnMode task handler
static TaskHandle_t xTaskBackgroundModeHandle = NULL; ///< vTaskBackgroundMode task handler
static TaskHandle_t xTaskInitializationModeHandle = NULL; ///< vTaskInitializationMode task handler
static TaskHandle_t xTaskDeveloperModeHandle = NULL; ///< vTaskDeveloperMode task handler
static QueueHandle_t xQueueButtonHandle = NULL; ///< Queue for communication between ButtonPressed task and other mode tasks

//Tasks
void vTaskButtonPressed(void *pvParameters);
void vTaskModeSwitcher(void *pvParameters);
void vTaskPowerOnMode(void *pvParameters);
void vTaskBackgroundMode(void *pvParameters);
void vTaskInitializationMode(void *pvParameters);
void vTaskDeveloperMode(void *pvParameters);

bool isExternalPower();
bool isEnoughBatteryPower();
bool isEnoughSensors();
void BLE_SendAuthInfo();

/**
 * @brief Button pressed ISR
 * 
 * This ISR direct notify task vTaskButtonPressed in which the duration of the pressing is determined
 * @param arg - GPIO number that trigger interrupt
 */
static void IRAM_ATTR button_isr_handler(void* arg)
{
    //uint32_t gpio_num = (uint32_t) arg;
    if (esp_timer_get_time() - last_micros > 80 * 1000) {    //80ms debounce delay
        last_micros = esp_timer_get_time(); //update last_micros
        //Give direct to task notification (it's faster, instead of using binary semaphore)
        vTaskNotifyGiveFromISR(xTaskButtonPressedHandle, NULL);
    }
}

void app_main(void)
{
    //Create queue to store button pressing
    xQueueButtonHandle = xQueueCreate(1, sizeof(bool));
    //Create task to handle button pressing
    xTaskCreate(vTaskButtonPressed, "ButtonPressed", 2048, NULL, 1, &xTaskButtonPressedHandle);
    //Configure the IOMUX register for pad GPIO_BUTTON
    gpio_pad_select_gpio(GPIO_BUTTON);
    //Set the GPIO as input
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_BUTTON, GPIO_INTR_NEGEDGE);
    //install gpio isr service 0 - default mode
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BUTTON, button_isr_handler, (void*) GPIO_BUTTON);
    
    //Set starting mode to POWER_ON_MODE
    MenuCurrentMode = POWER_ON_MODE;
    //First create and start Mode tasks
    xTaskCreate(vTaskPowerOnMode, "PowerOnMode", 2048, NULL, 1, &xTaskPowerOnModeHandle);
    xTaskCreate(vTaskBackgroundMode, "BackgroundMode", 2048, NULL, 1, &xTaskBackgroundModeHandle);
    xTaskCreate(vTaskInitializationMode, "InitMode", 2048, NULL, 1, &xTaskInitializationModeHandle);
    xTaskCreate(vTaskDeveloperMode, "DeveloperMode", 2048, NULL, 1, &xTaskDeveloperModeHandle);
    //Second start ModeSwitcher task, or else we notify from ModeSwithcher task to nowhere
    xTaskCreate(vTaskModeSwitcher, "ModeSwitcher", 2048, NULL, 1, &xTaskModeSwitcherHandle);

    //Main loop
    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Task that switch working modes
 * 
 * @param pvParameters 
 */
void vTaskModeSwitcher(void *pvParameters) {    //может эта задача ненужна, а сразу запускать PowerOn mode без начального ожидания notification и блокировки
    while (1) {
        ESP_LOGI(TAG_TASK, "ModeSwitcher activate");
        //notificate and unblock coresponding task
        if (MenuCurrentMode == POWER_ON_MODE) {
            ESP_LOGI(TAG_TASK, "Notifing vTaskPowerOnMode");
            xTaskNotifyGive(xTaskPowerOnModeHandle);
        } else if (MenuCurrentMode == BACKGROUND_MODE) {
            ESP_LOGI(TAG_TASK, "Notifing vTaskBackgroundMode");
            xTaskNotifyGive(xTaskBackgroundModeHandle);
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

/**
 * @brief Task for Power On mode
 * 
 * @param pvParameters 
 */
void vTaskPowerOnMode(void *pvParameters) {
    while (1) {
        //wait indefinitly until task was unblocked with notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG_TASK, "PowerOnMode activate");
        //check ext. power and batterys and notify corespondent task
        if (isExternalPower()) {
            ESP_LOGI(TAG_TASK, "Ext. power YES");
            /*if (isEnoughBatteryPower()) {
                xTaskNotifyGive(xTaskDataTransferHandle);
            }*/
        } else {
            ESP_LOGI(TAG_TASK, "Ext. power NO");
            if (isEnoughBatteryPower()) {
                ESP_LOGI(TAG_TASK, "Enough batt. power");
                ESP_LOGI(TAG_TASK, "Notifing vTaskBackgroundMode");
                xTaskNotifyGive(xTaskBackgroundModeHandle);
            }
        }
    }
}

/**
 * @brief Task for Background mode
 * 
 * @param pvParameters 
 */
void vTaskBackgroundMode(void *pvParameters) {
    bool ButtonShortPress = true; //flag to read from queue short/long press of button
    while (1) {
        //check current mode to find out that we first time here, and block task and wait indefinitly
        //or else mode already changed and task is executed
        if (MenuCurrentMode != BACKGROUND_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = BACKGROUND_MODE;
            ESP_LOGI(TAG_TASK, "BackgroundMode activate");
        }
        //check queue for button pressed and block task and wait indefinitly
        //i.e. wait for button pressed first time
        xQueueReceive(xQueueButtonHandle, &ButtonShortPress, portMAX_DELAY);
        ESP_LOGI(TAG_TASK, "Button pressed first time, show charge properties 5sec");
        //TODO: show charge properties 
        //wait 5sec for button pressed
        if (xQueueReceive(xQueueButtonHandle, &ButtonShortPress, 5000 / portTICK_PERIOD_MS) == pdPASS) {
            if (ButtonShortPress) {     //if short press
                ESP_LOGI(TAG_TASK, "Short pressing: feedback_blink, go to Initialization Mode");
                //TODO: feedback_blink, go to Initialization Mode
                //notify xTaskInitializationMode
                xTaskNotifyGive(xTaskInitializationModeHandle);
            } else {                    //if long press
                ESP_LOGI(TAG_TASK, "Long pressing: show wifi properties, erros, go to Developer Mode");
                //TODO: show wifi properties, erros, go to Developer Mode
                //notify xTaskDeveloperMode
                xTaskNotifyGive(xTaskDeveloperModeHandle);
            }
        } else {                        //if no press
            ESP_LOGI(TAG_TASK, "Not pressed: return to Background Mode begin");
            //TODO: display off, return to Background Mode begin
        }

    }
}

void vTaskInitializationMode(void *pvParameters) {
    while (1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != INITIALIZATION_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = INITIALIZATION_MODE;
            ESP_LOGI(TAG_TASK, "InitializationMode activate");
            //TODO: Инициализация сервера, проверка наличия таблицы соответствия
        }
        if (isEnoughSensors()) {
            //go to SensorsCheck task

        } else {
            //send auth info via BLE / WiFi
            BLE_SendAuthInfo();
        }

    }
    
}

void vTaskDeveloperMode(void *pvParameters) {
    while (1) {
        /* code */
    }
    
}

/**
 * @brief Task for determining the duration of a button press
 * 
 * @param pvParameters 
 */
void vTaskButtonPressed(void *pvParameters) {
    uint8_t duration = 0;
    #define poll_delay_ms 50
    #define press_delay_ms 1200 //if press <= delay then it's short press, else it's long press
    while (1) {
        //Block indefinitely to wait for a notification, pdTRUE means task notification is being used as a binary semaphore,
        //i.e. wait for interrupt from button
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        //Check button is pressed down
        if (gpio_get_level(GPIO_BUTTON) == 0) {
            int64_t last_micros = esp_timer_get_time(); //get current time for pressing delay measuring
            printf("Button pressed for.. ");
            //while button is pressed and delay < 2s
            while ((gpio_get_level(GPIO_BUTTON) == 0) && (esp_timer_get_time() - last_micros <= press_delay_ms * 1000)) {
                duration++; //update duration counter
                vTaskDelay(poll_delay_ms / portTICK_PERIOD_MS);
            }   //exit when button released or pressing delay > 2s
            printf("%dms\n", duration * poll_delay_ms); //print duration in msec
            duration = 0;
            //flag that set it was short or long press
            bool ButtonShortPress = true;
            //check if it was long press
            if (esp_timer_get_time() - last_micros > press_delay_ms * 1000) {
                ButtonShortPress = false;
            }
            //send press flag to the queue of button pressing
            xQueueSendToBack(xQueueButtonHandle, (void*) &ButtonShortPress, 10 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief Check and return true if ext. power present
 * 
 * @return true
 * @return false
 */
bool isExternalPower() {
    return false;
}

/**
 * @brief Check and return true if batt. charged
 * 
 * @return true
 * @return false
 */
bool isEnoughBatteryPower() {
    return true;
}

bool isEnoughSensors() {
    static uint8_t  sensor_cnt = 0;
    if (sensor_cnt < 9) {
        sensor_cnt++;
        ESP_LOGI(TAG_TASK, "Now %d sensors connected", sensor_cnt);
        return false;
    }
    return true;
}

void BLE_SendAuthInfo() {
    ESP_LOGI(TAG_TASK, "send auth info via BLE");
    vTaskDelay(500 / portTICK_PERIOD_MS);
}