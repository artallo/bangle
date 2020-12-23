/**
 * @file esp_ble.h
 * @author Artem (artallo@mail.ru)
 * @brief Initialization and other BLE stuff
 * @version 0.1
 * @date 2020-12-23
 * @copyright Copyright (c) 2020
 */
#ifndef _ESP_BLE_H__
#define _ESP_BLE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_bt.h"
//#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"

bool BLE_Init();

bool BLE_Deinit();

#endif /* _ESP_BLE_H__ */