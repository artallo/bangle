/**
 * @file stm8_bot.h
 * @author your name (you@domain.com)
 * @brief Bottom STM8L stuff (RTC, POWER, ..)
 * @version 0.1
 * @date 2021-01-21
 * @copyright Copyright (c) 2021
 */

#ifndef __STM_BOT_H__
#define __STM_BOT_H__

#include "i2c_bus.h"

//BCD to DEC
#define BCD_TO_DEC(x) (((((x) & 0xF0) >> 4) * 10) + ((x) & 0x0F))

//PSU - Power Supply Unit
#define PSU_I2C_ADDR 0x58

#define PSU_I2C_REG_PWR_CR 0x01
#define PSU_I2C_REG_PWR_CR_HV_EN 0x03

//RTC registers
#define PSU_I2C_REG_RTC_TR1 0x06
#define PSU_I2C_REG_RTC_TR2 0x07
#define PSU_I2C_REG_RTC_TR3 0x08
#define PSU_I2C_REG_RTC_DR1 0x09
#define PSU_I2C_REG_RTC_DR2 0x0A
#define PSU_I2C_REG_RTC_DR3 0x0B
#define PSU_I2C_REG_RTC_ISR1 0x0E

/**
 * @brief Struct date/time for STM8
 * 
 */
typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hr;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} stm8_time_t;

int stm8_bot_i2c_read_register(uint8_t reg);
esp_err_t stm8_bot_i2c_write_register(uint8_t reg, uint8_t val);
esp_err_t stm8_bot_i2c_read_data (uint8_t reg, uint8_t *buf, uint8_t n_bytes);
esp_err_t stm8_bot_i2c_write_data (uint8_t reg, uint8_t *buf, uint8_t n_bytes);
esp_err_t stm8_bot_getTime(stm8_time_t *t);
esp_err_t stm8_bot_setTime(stm8_time_t *t);

//utility

#endif /* __STM_BOT_H__ */