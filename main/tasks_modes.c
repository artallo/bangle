/**
 * @file tasks_modes.c
 * @brief FreeRTOS and tasks stuff. Задачи, определяющие режимы работы браслета.
 * @version 0.1
 * @date 2021-01-30
 * @copyright Copyright (c) 2021
 */

#include "driver/gpio.h"
#include "esp_ble.h"
#include "esp_log.h"

#include "esp_config.h"
#include "ssd1306.h"
#include "stm8_bot.h"
#include "tasks_modes.h"

volatile menu_mode_t MenuCurrentMode = STARTING_MODE; ///< Current working mode, starts from STARTING_MODE
volatile int64_t last_micros = 0;   ///< Delay measuring in usec

uint8_t ble_remote_sensor_count = 1; ///< Transmit/confirm connecting sensor number
uint8_t ble_remote_sensor_wifi = 1;  ///< Transmit/confirm connecting sensor wifi network number

//Handlers
static TaskHandle_t xTaskButtonPressedHandle = NULL; ///< vTaskButtonPressed task handler
static TaskHandle_t xTaskModeSwitcherHandle = NULL;  ///< vTaskModeSwitcher task handler
static TaskHandle_t xTaskPowerOnModeHandle = NULL;   ///< vTaskPowerOnMode task handler
static TaskHandle_t xTaskBackgroundModeHandle = NULL; ///< vTaskBackgroundMode task handler
static TaskHandle_t xTaskInitializationModeHandle = NULL; ///< vTaskInitializationMode task handler
static TaskHandle_t xTaskDeveloperModeHandle = NULL; ///< vTaskDeveloperMode task handler
static TaskHandle_t xTaskSensorcheckModeHandle = NULL; ///< vTaskSensorcheckMode task handler
static TaskHandle_t xTaskDatacollectionModeHandle = NULL; ///< vTaskDatacollectionMode task handler
static TaskHandle_t xTaskDatatransferModeHandle = NULL; ///< vTaskDatatransferMode task handler
static TaskHandle_t xTaskSleepModeHandle = NULL; ///< vTaskSleepMode task handler
static TaskHandle_t xTaskDisplayHandle = NULL; ///< vTaskDisplay task handler
static QueueHandle_t xQueueButtonHandle = NULL; ///< Queue for communication between ButtonPressed task and other mode tasks

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

bool tasks_init() {
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
    MenuCurrentMode = STARTING_MODE;
    //Create Dispaly task
    xTaskCreate(vTaskDisplay, "Display", 2048, NULL, 1, &xTaskDisplayHandle);
    //At First create and start Mode tasks
    xTaskCreate(vTaskPowerOnMode, "PowerOnMode", 2048, NULL, 1, &xTaskPowerOnModeHandle);
    xTaskCreate(vTaskBackgroundMode, "BackgroundMode", 2048, NULL, 1, &xTaskBackgroundModeHandle);
    xTaskCreate(vTaskInitializationMode, "InitMode", 2048, NULL, 1, &xTaskInitializationModeHandle);
    xTaskCreate(vTaskSensorcheckMode, "SensorcheckMode", 2048, NULL, 1, &xTaskSensorcheckModeHandle);
    xTaskCreate(vTaskDatacollectionMode, "DatacollectionMode", 2048, NULL, 1, &xTaskDatacollectionModeHandle);
    xTaskCreate(vTaskDatatransferMode, "DatatransferMode", 2048, NULL, 1, &xTaskDatatransferModeHandle);
    xTaskCreate(vTaskSleepMode, "SleepMode", 2048, NULL, 1, &xTaskSleepModeHandle);
    xTaskCreate(vTaskDeveloperMode, "DeveloperMode", 2048, NULL, 1, &xTaskDeveloperModeHandle);
    //Then at last create ModeSwitcher task, or else we would notify from ModeSwithcher task to nowhere
    xTaskCreate(vTaskModeSwitcher, "ModeSwitcher", 2048, NULL, 1, &xTaskModeSwitcherHandle);

    return true;
}

// !!!может эта задача не нужна, а сразу запускать PowerOn mode без начального ожидания notification и блокировки !!!
/**
 * @brief Task that switch working modes
 * 
 * @param pvParameters 
 */
void vTaskModeSwitcher(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG_TASK, "ModeSwitcher activate");
        //notificate and unblock coresponding task
        if (MenuCurrentMode == STARTING_MODE) {
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
        if (MenuCurrentMode != POWER_ON_MODE) {
            //wait indefinitly until task was unblocked with notification
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = POWER_ON_MODE;
            ESP_LOGI(TAG_TASK, "PowerOnMode activate");
        }
        //check ext. power and batterys and notify corespondent task
        if (stm8_bot_psu_isExternalPower(true)) {
            ESP_LOGI(TAG_TASK, "Ext. power: YES");
            if (stm8_bot_psu_isEnoughBatteryPower(true)) {
                ESP_LOGI(TAG_TASK, "Enough batt. power: YES");
                ESP_LOGI(TAG_TASK, "Goto Data transfer mode..");
                //delay for display
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                xTaskNotifyGive(xTaskDatatransferModeHandle);
                //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {    //battery charge LOW
                ESP_LOGI(TAG_TASK, "Enough batt. power: NO");
                ESP_LOGI(TAG_TASK, ".. delay and goto block begin");
                vTaskDelay(2500 / portTICK_PERIOD_MS);
            }
        } else {        //external power NO
            ESP_LOGI(TAG_TASK, "Ext. power: NO");
            if (stm8_bot_psu_isEnoughBatteryPower(true)) {
                ESP_LOGI(TAG_TASK, "Enough batt. power: YES");
                ESP_LOGI(TAG_TASK, "Notifing vTaskBackgroundMode");
                //delay for display
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                xTaskNotifyGive(xTaskBackgroundModeHandle);
                //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {    //battery charge LOW
                ESP_LOGI(TAG_TASK, "Enough batt. power: NO");
                ESP_LOGI(TAG_TASK, ".. delay and goto block begin");
                vTaskDelay(2500 / portTICK_PERIOD_MS);
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
                ble_remote_sensor_count = 1;
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
    //uint8_t sensor_count = 1; //number of sensor for connection
    //uint8_t wifi_id = 1; //wifi network table number
    while (1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != INITIALIZATION_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            MenuCurrentMode = INITIALIZATION_MODE;
            ESP_LOGI(TAG_TASK, "InitializationMode activate");
            //однократно инициализация режима маяка
            ble_init_beacon_mode();
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
            //send auth info via BLE
            ESP_LOGI(TAG_TASK, "Enought sensors: NO");
            //bool is_sensor_confirmed = false;
            while (1) {
                ble_send_authInfo(ble_remote_sensor_wifi, ble_remote_sensor_count); //transmiting 3seconds wifi and sensor number via BLE
                ble_scan_confirmation(ble_remote_sensor_wifi, ble_remote_sensor_count); //goto scan mode for 1sec to rceive confirmation from sensors via BLE
                if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1100)) != 0) { //if confirmation received within next 1.1sec then break the loop
                    break;
                }
            }
            // here sensor connection confirmed
            ++ble_remote_sensor_count;
            
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
            xTaskNotifyGive(xTaskSleepModeHandle);
            vTaskDelay(pdMS_TO_TICKS(50));
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
                        xTaskNotifyGive(xTaskDatatransferModeHandle);
                        //delay that new task activate and change MenuCurrentMode variable, so this task go to begin and wait notification in blocked state
                        vTaskDelay(50 / portTICK_PERIOD_MS);
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
 * @brief Task for data transfer mode
 * 
 * @param pvParameters 
 */
void vTaskDatatransferMode(void *pvParameters) {
    while (1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != DATA_TRANSFER_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);    
            MenuCurrentMode = DATA_TRANSFER_MODE;
            ESP_LOGI(TAG_TASK, "DatatransferMode activate");
        }
        if (stm8_bot_psu_isExternalPower(true) == true) {    //if ext. power present
            if (isDataExists()) {   //check if data exists
                if (AskingServer() ==  true) {  //check if server ready
                    ESP_LOGI(TAG_TASK, "Ext. power present, data exists and server ready");
                    //do data transfering
                    ESP_LOGI(TAG_TASK, "Data transfering..");
                    //delay for display info
                    vTaskDelay(2500 / portTICK_PERIOD_MS);
                }
            } else {    //no data to transfer
                ESP_LOGI(TAG_TASK, "No data to transfer..");
                //TODO: feedback blink or display 
            }
            //here data was transfered or no data to transfer so then go to Background mode
            ESP_LOGI(TAG_TASK, ".. go to background mode");
            xTaskNotifyGive(xTaskBackgroundModeHandle);
            vTaskDelay(pdMS_TO_TICKS(50));            
        } else {    //if no ext. power
            //TODO: show info, delay
            ESP_LOGI(TAG_TASK, "No ext. power, show info, delay 1500ms.. go to task begin");
            vTaskDelay(pdMS_TO_TICKS(1500));
        }

    }
}

/**
 * @brief Task for Sleep mode
 * 
 * @param pvParameters 
 */
void vTaskSleepMode(void *pvParameters) {
    while (1) {
        //first time wait indefinitley for notification to activate task
        if (MenuCurrentMode != SLEEP_MODE) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);    
            MenuCurrentMode = SLEEP_MODE;
            ESP_LOGI(TAG_TASK, "SleepMode activate");
        }
        //TODO: BNO to sleep with interrupt from BNO
        //TODO: ESP sleep and wake by timer or by interrupt from BNO
        //IF ESP wake by timer then check time < 90min else goto Backgroundmode
        //IF ESP wake by interrupt from BNO then go to Datacollection mode
        ESP_LOGI(TAG_TASK, "Sleep long time.. and go to Background mode");
        xTaskNotifyGive(xTaskBackgroundModeHandle);
        vTaskDelay(pdMS_TO_TICKS(50));        
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
 * @brief Task for displaying info on ssd1309 display
 * 
 * @param pvParameters 
 */
void vTaskDisplay(void *pvParameters) {
    while (1)
    {
        // Clear screen
        ssd1306_Fill(Black);
        // Flush buffer to screen
        //ssd1306_UpdateScreen();
        //select current mode
        if (MenuCurrentMode == POWER_ON_MODE) {
            ssd1306_SetCursor(18, 10);
            ssd1306_WriteString("Power on mode", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == BACKGROUND_MODE) {
            ssd1306_SetCursor(11, 10);
            ssd1306_WriteString("Background mode", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == INITIALIZATION_MODE) {
            ssd1306_SetCursor(14, 10);
            ssd1306_WriteString("Initialization", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == SENSOR_CHECK_MODE) {
            ssd1306_SetCursor(15, 10);
            ssd1306_WriteString("Sensor check", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == DATA_COLLECTION_MODE) {
            ssd1306_SetCursor(13, 10);
            ssd1306_WriteString("Data collection", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == DATA_TRANSFER_MODE) {
            ssd1306_SetCursor(13, 10);
            ssd1306_WriteString("Data transfer", Font_7x10, White);
            ssd1306_UpdateScreen();
        } else if (MenuCurrentMode == DEVELOPER_MODE) {
            ssd1306_SetCursor(14, 10);
            ssd1306_WriteString("Developer mode", Font_7x10, White);
            ssd1306_UpdateScreen();
        }
        // TODO: возможно ли переделать с ожидания delay на ожидание уведомления taskNotify от задач режимов, чтобы не крутить в холостую обновляя статичную информацию
        vTaskDelay(100 / portTICK_PERIOD_MS);
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
 * @brief Check if all sensors connected
 * 
 * @return true 
 * @return false 
 */
bool isEnoughSensors() {
    //static uint8_t  sensor_cnt = 0;
    if (ble_remote_sensor_count < 5) {
        //++ble_remote_sensor_count;
        ESP_LOGI(TAG_TASK, "Now %d sensors connected", ble_remote_sensor_count - 1);
        return false;
    }
    ble_remote_sensor_count = 5;
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
 * @brief Check if data exists
 * 
 * @return true 
 * @return false 
 */
bool isDataExists() {
    return true;
}

/**
 * @brief Ask server that it's ready receive data 
 * 
 * @return true 
 * @return false 
 */
bool AskingServer() {
    return true;
}

/**
 * @brief Send config message (iBeacon) with connection info to sensors (3 seconds)
 * 
 * UUID 0 and 1 bytes content 'B' and 'A', minor field content wifi network, major field content sensor number
 */
void ble_send_authInfo(uint16_t wifi_num, uint16_t sensor_cnt) {
    ESP_LOGI(TAG_TASK, "send auth info via BLE");
    
    extern esp_ble_ibeacon_vendor_t vendor_config;
    vendor_config.proximity_uuid[0] = 'B';
    vendor_config.proximity_uuid[1] = 'A';
    vendor_config.major = ((sensor_cnt&0xFF00)>>8) + ((sensor_cnt&0xFF)<<8);
    vendor_config.minor = ((wifi_num&0xFF00)>>8) + ((wifi_num&0xFF)<<8);
    esp_ble_ibeacon_t ibeacon_adv_data;
    esp_err_t status = esp_ble_config_ibeacon_data (&vendor_config, &ibeacon_adv_data);
    if (status == ESP_OK){
        esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    }

    esp_ble_adv_params_t ble_adv_params = {
        .adv_int_min        = 0x20,
        .adv_int_max        = 0x40,
        .adv_type           = ADV_TYPE_NONCONN_IND,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    esp_ble_gap_start_advertising(&ble_adv_params);
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    esp_ble_gap_stop_advertising();
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    esp_err_t err;

    switch(event)
    {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            uint32_t duration = 1; //seconds
            esp_ble_gap_start_scanning(duration);
            break;
        }
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
            if((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG_BLE,"Scan start failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI(TAG_BLE,"Start scanning...");
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
            switch(scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT: {
                    //ESP_LOGI(TAG_BLE, "iBeacon Found");
                    if (esp_ble_is_ibeacon_packet(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)) {
                        if (ble_remote_sensor_confirmed(scan_result->scan_rst.ble_adv)) {
                            esp_ble_gap_stop_scanning();
                            xTaskNotifyGive(xTaskInitializationModeHandle);
                        }
                        /*esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                        ESP_LOGI(TAG_BLE, "iBeacon Found");
                        esp_log_buffer_hex("IBEACON_DEMO: Device address:", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN );
                        esp_log_buffer_hex("IBEACON_DEMO: Proximity UUID:", ibeacon_data->ibeacon_vendor.proximity_uuid, ESP_UUID_LEN_128);

                        uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
                        uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
                        ESP_LOGI(TAG_BLE, "Major: 0x%04x (%d)", major, major);
                        ESP_LOGI(TAG_BLE, "Minor: 0x%04x (%d)", minor, minor);
                        ESP_LOGI(TAG_BLE, "Measured power (RSSI at a 1m distance):%d dbm", ibeacon_data->ibeacon_vendor.measured_power);
                        ESP_LOGI(TAG_BLE, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);*/
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
            if((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG_BLE,"Scan stop failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI(TAG_BLE,"Stop scan successfully");
            }
            break;
        }
        default:
            break;
    }
}

/**
 * @brief Init beacon mode and register callback for BLE scan events.
 * 
 */
void ble_init_beacon_mode() {
    esp_err_t status;
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG_BLE, "gap callback register error: %s", esp_err_to_name(status));
        return;
    }

    /*esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };*/

    //extern esp_ble_scan_params_t ble_scan_params;
    //esp_ble_gap_set_scan_params(&ble_scan_params);
}

/**
 * @brief Start BLE scan for confirmation from sensors (1 second)
 * 
 * @return true 
 * @return false 
 */
void ble_scan_confirmation() {

    /*esp_err_t status;
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG_BLE, "gap callback register error: %s", esp_err_to_name(status));
        return false;
    }*/

    extern esp_ble_scan_params_t ble_scan_params;
    esp_ble_gap_set_scan_params(&ble_scan_params);
}

/**
 * @brief Check confirmaton from sensor received via BLE
 * 
 * Major field content sensor number. Minor field content wifi network number. UUID bytes 0 and 1 content values'S' and 'E'
 * @return true 
 * @return false 
 */
bool ble_remote_sensor_confirmed(uint8_t *adv_data) {
    if ((adv_data[6] == 'S') && 
        (adv_data[7] == 'E') && 
        (adv_data[23] == ble_remote_sensor_count) &&
        (adv_data[25] == ble_remote_sensor_wifi)) {
        return true;
    }
    return false;
}