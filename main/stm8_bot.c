/**
 * @file stm8_bot.c
 * @author your name (you@domain.com)
 * @brief Bottom STM8L stuff (RTC, POWER, ..)
 * @version 0.1
 * @date 2021-01-21
 * @copyright Copyright (c) 2021
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

#include "stm8_bot.h"

int i2c_read_register(uint8_t reg) {
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
    //printf("register: %d\n", rtc_tr1_val);
    return reg_val;
}

int i2c_read_data (uint8_t reg, uint8_t *buf, uint8_t n_bytes) {

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
    printf("sec: %d, min: %d, hr: %d month: %d, year: %d\n", BCD_DEC(buf[0]), BCD_DEC(buf[1]), BCD_DEC(buf[2]), buf[4], buf[5]);

    return 0;
}


stm_time_t stm8_bot_getTime() {
    stm_time_t t;
    for (int r = 0x06; r <= 0x0B; r++) {

    }
}