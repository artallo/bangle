/**
 * @file esp_config.h
 * @author Artem (artallo@mail.ru)
 * @brief Файл конфигурации 
 * 
 * Задается конфигурация шины I2C. Определяются пины ESP32 на которые подключенны внешние датчики и устройста.
 * @version 0.1
 * @date 2020-11-12
 * @copyright Copyright (c) 2020
 */

#ifndef ESP_CONFIG_H__
#define ESP_CONFIG_H__

#define GPIO_BUTTON 27 ///< Button pin

#define GPIO_DISPLAY_RESET 21 ///< Display reset pin

#define GPIO_I2C_SCL 25 ///< I2C SCL pin
#define GPIO_I2C_SDA 26 ///< I2C SDA pin
#define I2C_FREQ 100000 ///< I2C frequency
#define I2C_PORT_NUM I2C_NUM_1 ///< I2C port number. There are 2 i2c port in esp32: I2C_NUM_0 and I2C_NUM_1

#define WIFI_UUID {'W', 'I', 'F', 'I', '-', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd', 0x64, 0x78, 0x25} ///< WiFi AP name/psw (SSID/PSW)

#endif /* ESP_CONFIG_H__ */
