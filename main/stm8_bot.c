/**
 * @file stm8_bot.c
 * @author your name (you@domain.com)
 * @brief Bottom STM8L stuff (RTC, POWER, ..)
 * @version 0.1
 * @date 2021-01-21
 * @copyright Copyright (c) 2021
 */

#include <string.h>
#include "stm8_bot.h"

/**
 * @brief Read one register from bottom STM8
 * 
 * @param reg 
 * @return int 
 */
int stm8_bot_i2c_read_register(uint8_t reg) {
    uint8_t reg_val;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_READ, 1);
    i2c_master_read_byte(cmd, &reg_val, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        printf("PSU I2C register %d read OK. Value %d\n", reg, reg_val);
    } else {
        printf("PSU I2C register %d read error!\n", reg);
        //ESP_ERROR_CHECK(ret);
    }
    return reg_val;
}

/**
 * @brief Write one register to bottom STM8
 * 
 * @param reg 
 * @param val 
 * @return esp_err_t 
 */
esp_err_t stm8_bot_i2c_write_register(uint8_t reg, uint8_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);
    i2c_master_write_byte(cmd, val, 1);
    i2c_master_stop(cmd);    
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (err == ESP_OK) {
        printf("PSU I2C register %d write value %d OK!\n", reg, val);
    } else {
        printf("PSU I2C register %d write error!\n", reg);
    }

    return err;
}

/**
 * @brief Read n registers to buffer from bottom STM8
 * 
 * @param reg 
 * @param buf 
 * @param n_bytes 
 * @return esp_err_t 
 */
esp_err_t stm8_bot_i2c_read_data (uint8_t reg, uint8_t *buf, uint8_t n_bytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // making the command - begin 
    i2c_master_start(cmd);  // start condition
    // device address with write bit
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_WRITE, 1); 
    // send the register address
    i2c_master_write_byte(cmd, reg, 1);   

    i2c_master_start(cmd);  // start condition again
    // device address with read bit
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_READ, 1);
    // read n_bytes-1, issue ACK
    i2c_master_read(cmd, buf, n_bytes - 1, ACK_VAL);
    // read the last byte, issue NACK
    i2c_master_read_byte(cmd, buf + n_bytes - 1, NACK_VAL); 
    i2c_master_stop(cmd);  // stop condition
    // making the command - end 

    // Now execute the command
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

/**
 * @brief Write from buffer to n registers of bottom STM8
 * 
 * @param reg 
 * @param buf 
 * @param n_bytes 
 * @return esp_err_t 
 */
esp_err_t stm8_bot_i2c_write_data (uint8_t reg, uint8_t *buf, uint8_t n_bytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // making the command - begin 
    i2c_master_start(cmd);  // start condition
    // device address with write bit
    i2c_master_write_byte(cmd, (PSU_I2C_ADDR << 1) | I2C_MASTER_WRITE, 1); 
    // send the register address
    i2c_master_write_byte(cmd, reg, 1);   
    // write n_bytes, ACK
    i2c_master_write(cmd, buf, n_bytes, 1);
    // stop condition
    i2c_master_stop(cmd);
    // making the command - end 

    // Now execute the command
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

/**
 * @brief Get date/time from bottom STM8L
 * 
 * Get date/time and put data in stm8_time_t *t sructure
 * @param t 
 * @return esp_err_t 
 */
esp_err_t stm8_bot_getTime(stm8_time_t *t) {
    //variant with conversion
    /*i2c_read_data(PSU_I2C_REG_RTC_TR1, (uint8_t*) t, 6);
    for (int i = 0; i < 6; i++) {
        ((uint8_t*)t)[i] = BCD_TO_DEC(((uint8_t*) t)[i]);
    }*/

    //variant with additional buffer
    uint8_t buf[6];
    stm8_bot_i2c_read_data(PSU_I2C_REG_RTC_TR1, buf, 6);

    //convert raw BCD date/time data from STM to DEC representation
    buf[0] = ((buf[0] & 0b01110000) >> 4) * 10 + (buf[0] & 0b00001111); //sec
    buf[1] = ((buf[1] & 0b01110000) >> 4) * 10 + (buf[1] & 0b00001111); //min
    buf[2] = ((buf[2] & 0b01110000) >> 4) * 10 + (buf[2] & 0b00001111); //hr
    buf[3] = ((buf[3] & 0b00110000) >> 4) * 10 + (buf[3] & 0b00001111); //day
    buf[4] = ((buf[4] & 0b00010000) >> 4) * 10 + (buf[4] & 0b00001111); //month
    buf[5] = ((buf[5] & 0b11110000) >> 4) * 10 + (buf[5] & 0b00001111); //year
    
    //copy buf to date/time structure
    memcpy(t, buf, 6);

    return ESP_OK;
}

/**
 * @brief Set date/time on bottom STM8L
 * 
 * @param t 
 * @return esp_err_t 
 */
esp_err_t stm8_bot_setTime(stm8_time_t *t) {
    uint8_t buf[6];
    memcpy(buf, t, 6);
    
    //convert date/time from DEC to BCD format for STM
    buf[0] = ((buf[0] / 10) << 4) + (buf[0] % 10); //sec
    buf[1] = ((buf[1] / 10) << 4) + (buf[1] % 10); //min
    buf[2] = ((buf[2] / 10) << 4) + (buf[2] % 10); //hr
    buf[3] = ((buf[3] / 10) << 4) + (buf[3] % 10); //day
    buf[4] = ((buf[4] / 10) << 4) + (buf[4] % 10); //month
    buf[5] = ((buf[5] / 10) << 4) + (buf[5] % 10); //year
    
    //set INIT bit in PSU_I2C_REG_RTC_ISR1 register to goto time init mode
    stm8_bot_i2c_write_register(PSU_I2C_REG_RTC_ISR1, 0b10110001);

    //write date/time to STM RTC registers
    stm8_bot_i2c_write_data(PSU_I2C_REG_RTC_TR1, buf, 6);

    //clear INIT bit in PSU_I2C_REG_RTC_ISR1 register to goto time freerun mode
    stm8_bot_i2c_write_register(PSU_I2C_REG_RTC_ISR1, 0); //0b01010001   
    
    return ESP_OK;
}

/**
 * @brief Enable/disable 12V to power up display
 * 
 * @param s true - enable, false - disable
 * @return esp_err_t 
 */
esp_err_t stm8_bot_psu_en_display(bool s) {
    uint8_t val = stm8_bot_i2c_read_register(PSU_I2C_REG_PWR_CR);
    if (s) {    //true - set HV_EN bit to enable power
        val |= PSU_I2C_REG_PWR_CR_HV_EN;
    } else {    //false - clear HV_EN bit
        val &= ~PSU_I2C_REG_PWR_CR_HV_EN;
    }
    stm8_bot_i2c_write_register(PSU_I2C_REG_PWR_CR, val);
    return ESP_OK;
}

/**
 * @brief Check and return true if ext. power present
 * 
 * @return true
 * @return false
 */
bool stm8_bot_pcu_isExternalPower(bool flag) {
    
    return flag;
}

/**
 * @brief Check and return true if batt. charged
 * 
 * @return true
 * @return false
 */
bool stm8_bot_psu_isEnoughBatteryPower() {
    return true;
}
