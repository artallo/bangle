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
    DEVELOPER_MODE
} menu_mode_t; 

#define GPIO_BUTTON 27 ///< Button pin

//LOGs
static const char* TAG_TASK = "vTasks";

volatile int64_t last_micros = 0;   ///< Delay measuring in usec 
volatile menu_mode_t MenuCurrentMode = POWER_ON_MODE; ///< Current working mode, starts from POWER_ON_MODE

//Handlers
static TaskHandle_t xTaskButtonPressedHandle = NULL; ///< vTaskButtonPressed task handler
static TaskHandle_t xTaskModeSwitcherHandle = NULL;  ///< vTaskModeSwitcher task handler
static TaskHandle_t xTaskPowerOnModeHandle = NULL;   ///< vTaskPowerOnMode task handler
static TaskHandle_t xTaskBackgroundModeHandle = NULL; ///< vTaskBackgroundMode task handler
static TaskHandle_t xTaskInitializationModeHandle = NULL; ///< vTaskInitializationMode task handler
static TaskHandle_t xTaskDeveloperModeHandle = NULL; ///< vTaskDeveloperMode task handler
static TaskHandle_t xTaskSensorcheckModeHandle = NULL; ///< vTaskSensorcheckMode task handler
static TaskHandle_t xTaskDatacollectionModeHandle = NULL; ///< vTaskDatacollectionMode task handler
static QueueHandle_t xQueueButtonHandle = NULL; ///< Queue for communication between ButtonPressed task and other mode tasks

//Tasks
void vTaskButtonPressed(void *pvParameters);
void vTaskModeSwitcher(void *pvParameters);
void vTaskPowerOnMode(void *pvParameters);
void vTaskBackgroundMode(void *pvParameters);
void vTaskInitializationMode(void *pvParameters);
void vTaskSensorcheckMode(void *pvParameters);
void vTaskDatacollectionMode(void *pvParameters);
void vTaskDeveloperMode(void *pvParameters);

bool isExternalPower();
bool isEnoughBatteryPower();
bool isEnoughSensors();
bool ChekingSensorsConnection();
bool isDataCheck_OK();
bool isChangingAccelData();
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
    //At First create and start Mode tasks
    xTaskCreate(vTaskPowerOnMode, "PowerOnMode", 2048, NULL, 1, &xTaskPowerOnModeHandle);
    xTaskCreate(vTaskBackgroundMode, "BackgroundMode", 2048, NULL, 1, &xTaskBackgroundModeHandle);
    xTaskCreate(vTaskInitializationMode, "InitMode", 2048, NULL, 1, &xTaskInitializationModeHandle);
    xTaskCreate(vTaskSensorcheckMode, "SensorcheckMode", 2048, NULL, 1, &xTaskSensorcheckModeHandle);
    xTaskCreate(vTaskDatacollectionMode, "DatacollectionMode", 2048, NULL, 1, &xTaskDatacollectionModeHandle);
    xTaskCreate(vTaskDeveloperMode, "DeveloperMode", 2048, NULL, 1, &xTaskDeveloperModeHandle);
    //Than second create ModeSwitcher task, or else we notify from ModeSwithcher task to nowhere
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
            ESP_LOGI(TAG_TASK, "Ext. power: YES");
            /*if (isEnoughBatteryPower()) {
                xTaskNotifyGive(xTaskDataTransferHandle);
            }*/
        } else {
            ESP_LOGI(TAG_TASK, "Ext. power: NO");
            if (isEnoughBatteryPower()) {
                ESP_LOGI(TAG_TASK, "Enough batt. power: YES");
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
        if (xQueueReceive(xQueueButtonHandle, &ButtonShortPress, 5000 / portTICK_PERIOD_MS) == pdPASS) {    //if it was press
            if (ButtonShortPress) {     //if short press
                ESP_LOGI(TAG_TASK, "Short pressing: feedback_blink, go to Initialization Mode");
                //TODO: feedback_blink, go to Initialization Mode
                //notify xTaskInitializationMode
                xTaskNotifyGive(xTaskInitializationModeHandle);
                //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {                    //if long press
                ESP_LOGI(TAG_TASK, "Long pressing: show wifi properties, erros, go to Developer Mode");
                //TODO: show wifi properties, erros, go to Developer Mode
                //notify xTaskDeveloperMode
                xTaskNotifyGive(xTaskDeveloperModeHandle);
                //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
        } else {                        //if no press
            ESP_LOGI(TAG_TASK, "Not pressed: return to Background Mode begin");
            //TODO: display off, return to Background Mode begin
        }

    }
}

/**
 * @brief Task for Initialization mode
 * 
 * @param pvParameters 
 */
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
            ESP_LOGI(TAG_TASK, "Enought sensors: YES");
            ESP_LOGI(TAG_TASK, "go to Sensor check task");
            xTaskNotifyGive(xTaskSensorcheckModeHandle);
            //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
            vTaskDelay(50 / portTICK_PERIOD_MS);
            //vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            //send auth info via BLE / WiFi
            ESP_LOGI(TAG_TASK, "Enought sensors: NO");
            BLE_SendAuthInfo();
        }

    }
    
}

/**
 * @brief Task for Sensor check mode
 * 
 * @param pvParameters 
 */
void vTaskSensorcheckMode(void *pvParameters) {
    while (1)     {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != SENSOR_CHECK_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = SENSOR_CHECK_MODE;
            ESP_LOGI(TAG_TASK, "SensorcheckMode activate");
        }
        if (ChekingSensorsConnection() == true) {
            //TODO: Concordance, Starting, Time synchro
            ESP_LOGI(TAG_TASK, "Cheking sensors connection OK: next do  Concordance, Starting, Time synchro.., go to Data collection mode");
            //notify xTaskDatacollectionMode
            xTaskNotifyGive(xTaskDatacollectionModeHandle);
            //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
            vTaskDelay(50 / portTICK_PERIOD_MS);
        } else {
            ESP_LOGI(TAG_TASK, "Cheking sensors connection NOT OK: feedback, one more try.. go to Initialization mode");
            //notify xTaskInitializationMode
            xTaskNotifyGive(xTaskInitializationModeHandle);
            //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief Task for Data collection mode
 * 
 * @param pvParameters 
 */
void vTaskDatacollectionMode(void *pvParameters) {
    bool ButtonShortPress = true;
    while(1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != DATA_COLLECTION_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);    
            MenuCurrentMode = DATA_COLLECTION_MODE;
            ESP_LOGI(TAG_TASK, "DatacollectionMode activate");
        }
        //TODO: start/resume (external?) task for data collection
        ESP_LOGI(TAG_TASK, "Start/continue external task for (wifi) data collection, saving, data check");
        //TODO: Data saving, data check - наверно это внутри задачи получения данных
        //if collected data is not OK
        if ( ! isDataCheck_OK()) {
            ESP_LOGI(TAG_TASK, "Data check NOT OK, go to Disconnection mode");
            // !!!! TODO: notificate Disconnection task
            continue; //this mean that we go to begin of while loop
        }
        //if accel data is not ganging
        if ( ! isChangingAccelData()) {
            ESP_LOGI(TAG_TASK, "Accel data not changing, check if it's long time then go to Sleep mode");
            //TODO: if it's long time not changing then notificate sleep mode task
            continue; //this mean that we go to begin of while loop
        }
        //if we get here then previous if not fired so wait 500ms for button click
        if (xQueueReceive(xQueueButtonHandle, &ButtonShortPress, pdMS_TO_TICKS(500)) == pdPASS) {
            //check it was first short click
            if (ButtonShortPress == true) {
                //wait next 5sec for second click
                ESP_LOGI(TAG_TASK, "Button was pressed 1st time, wait 5sec for second pressing");
                if (xQueueReceive(xQueueButtonHandle, &ButtonShortPress, pdMS_TO_TICKS(5000)) == pdPASS) {
                    if (ButtonShortPress == false) { //if long press
                        ESP_LOGI(TAG_TASK, "2nd pressing: button long pressed");
                        //TODO: stop data collection, go to data transfer
                        ESP_LOGI(TAG_TASK, "Stop data collection, go to Data transfer task");
                        //!!!!!notificate transfer task
                    } else { //if short press
                        ESP_LOGI(TAG_TASK, "2nd pressing: button short pressed");
                        //TODO: show info, delay
                        ESP_LOGI(TAG_TASK, "Show info, delay.. until button pressed");
                        xQueueReceive(xQueueButtonHandle, &ButtonShortPress, portMAX_DELAY);
                    }
                }
            }
        }
    }

}

/**
 * @brief Task for Developer mode
 * 
 * @param pvParameters 
 */
void vTaskDeveloperMode(void *pvParameters) {
    bool ButtonShortPress = true; //flag to read from queue short/long press of button
    while (1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != DEVELOPER_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = DEVELOPER_MODE;
            ESP_LOGI(TAG_TASK, "DeveloperMode activate");
            //TODO: do some developer staff
        }
        //TODO: full feedback
        ESP_LOGI(TAG_TASK, "full feedback");
        //wait for button long press
        if (xQueueReceive(xQueueButtonHandle, &ButtonShortPress, portMAX_DELAY) == pdPASS) {    //if it was press
            if (ButtonShortPress == false) {  //if long press
                //go to backrounf mode
                ESP_LOGI(TAG_TASK, "Long pressing: go to Background Mode");
                xTaskNotifyGive(xTaskBackgroundModeHandle);
                //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {    //if short press
                ESP_LOGI(TAG_TASK, "Short pressing: go to full feedback");
            }
        }

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

/**
 * @brief Check if all sensors connected
 * 
 * @return true 
 * @return false 
 */
bool isEnoughSensors() {
    static uint8_t  sensor_cnt = 0;
    if (sensor_cnt < 9) {
        sensor_cnt++;
        ESP_LOGI(TAG_TASK, "Now %d sensors connected", sensor_cnt);
        return false;
    }
    sensor_cnt = 5;
    return true;
}

/**
 * @brief Check and return true if sensors connection OK
 * 
 * @return true 
 * @return false 
 */
bool ChekingSensorsConnection() {
    static bool isContinue = false;
    if (isContinue) {
        return true;
    }
    isContinue = true;
    return false;
}

/**
 * @brief Chek if collected data is OK
 * 
 * @return true 
 * @return false 
 */
bool isDataCheck_OK() {
    return true;
}

/**
 * @brief Check if accel data is changing
 * 
 * @return true 
 * @return false 
 */
bool isChangingAccelData() {
    return true;
}

/**
 * @brief Send config message (iBeacon?) with connection info to sensors
 * 
 */
void BLE_SendAuthInfo() {
    ESP_LOGI(TAG_TASK, "send auth info via BLE");
    vTaskDelay(600 / portTICK_PERIOD_MS);
}