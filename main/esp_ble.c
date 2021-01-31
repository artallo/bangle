#include "esp_ble.h"

/**
 * @brief Init BLE, allocate memory
 * 
 * @return true 
 * @return false 
 */
bool BLE_Init() {
    esp_err_t err;
    
    //err = nvs_flash_init();
    err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    return true;
}

/**
 * @brief Deinit BLE, disable BT and free allocated memory
 * 
 * @return true 
 * @return false 
 */
bool BLE_Deinit() {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    printf("Deinit iBeacon free heap %d\n", xPortGetFreeHeapSize());

    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    printf("Deinit BT free heap %d\n", xPortGetFreeHeapSize());

    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    printf("Mem release BT_BTDM free heap %d\n", xPortGetFreeHeapSize());

    return true;
}