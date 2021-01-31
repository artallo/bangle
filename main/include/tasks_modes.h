/**
 * @file tasks_modes.h
 * @brief FreeRTOS and tasks stuff. Задачи, определяющие режимы работы браслета.
 * @version 0.1
 * @date 2021-01-30
 * @copyright Copyright (c) 2021
 */

#ifndef __TASKS_MODES_H__
#define __TASKS_MODES_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

///Type which describe all working modes
typedef enum {
    STARTING_MODE,
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

static void IRAM_ATTR button_isr_handler(void* arg);

//Tasks
void vTaskButtonPressed(void *pvParameters);
void vTaskModeSwitcher(void *pvParameters);
void vTaskPowerOnMode(void *pvParameters);
void vTaskBackgroundMode(void *pvParameters);
void vTaskInitializationMode(void *pvParameters);
void vTaskSensorcheckMode(void *pvParameters);
void vTaskDatacollectionMode(void *pvParameters);
void vTaskDatatransferMode(void *pvParameters);
void vTaskSleepMode(void *pvParameters);
void vTaskDeveloperMode(void *pvParameters);
void vTaskDisplay(void *pvParameters);

bool isEnoughSensors();
bool ChekingSensorsConnection();
bool isDataCheck_OK();
bool isChangingAccelData();
bool isDataExists();
bool AskingServer();
void ble_send_authInfo(uint16_t, uint16_t);
bool ble_get_confirmation();
void ble_init_beacon_mode();

bool tasks_init();

static const char* TAG_TASK = "vTasks";
static const char* TAG_BLE = "BLE";


#endif /* __TASKS_MODES_H__ */