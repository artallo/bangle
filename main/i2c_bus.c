/**
 * @file i2c_bus.c
 * @author Artem (artallo@mail.ru)
 * @brief Инициализация и работа с шиной I2C
 * @version 0.1
 * @date 2020-11-12
 * @copyright Copyright (c) 2020
 */
#include "i2c_bus.h"
#include "esp_log.h"

/**
 * @brief I2C bus object
 */
static i2c_bus_t i2c_bus = {
    .is_init = false
};

/**
 * @brief Init I2C bus
 * 
 * @param scl 
 * @param sda 
 * @param freq 
 * @param port_num 
 * @return esp_err_t 
 */
esp_err_t i2c_master_init(int scl, int sda, int freq, int port_num) {
    if ( ! i2c_bus.is_init ) {
        i2c_bus.conf.mode = I2C_MODE_MASTER;
        i2c_bus.conf.scl_io_num = scl;
        i2c_bus.conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_bus.conf.sda_io_num = sda;
        i2c_bus.conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_bus.conf.master.clk_speed = freq;
        i2c_param_config(port_num, &i2c_bus.conf);
        esp_err_t res = i2c_driver_install(port_num, i2c_bus.conf.mode, 0, 0, 0);
        if (res == ESP_OK) {
            i2c_bus.is_init = true;
            ESP_LOGI("I2C", "INIT OKk");
        }
        return res;
    }
    ESP_LOGI("I2C", "ALREADY INITED");
    return ESP_OK;
}