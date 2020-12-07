/**
 * @file i2c_bus.h
 * @author Artem (artallo@mail.ru)
 * @brief Инициализация и работа с шиной I2C
 * @version 0.1
 * @date 2020-11-12
 * @copyright Copyright (c) 2020
 */

#ifndef I2C_BUS_H__
#define I2C_BUS_H__

#include "driver/i2c.h"

#define ACK_VAL 0x00
#define NACK_VAL 0x01

/**
 * @brief Struct for I2C bus
 */
typedef struct
{
    i2c_config_t conf;
    bool is_init; ///< True if i2c bus already initializied
} i2c_bus_t;

esp_err_t i2c_master_init(int scl, int sda, int freq, int port_num);

#endif /* I2C_BUS_H__ */
