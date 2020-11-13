/**
 * @file ssd1306_conf.h
 * @author Artem
 * @brief Configuration file for the SSD1306 library.
 * @version 0.1
 * @date 2020-11-13
 * @copyright Copyright (c) 2020
 */
#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// I2C Configuration
#define SSD1306_I2C_PORT 1
#define SSD1306_I2C_ADDR 0x3C

// Mirror the screen if needed
// #define SSD1306_MIRROR_VERT
// #define SSD1306_MIRROR_HORIZ

// Set inverse color if needed
// # define SSD1306_INVERSE_COLOR

// Include only needed fonts
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
//#define SSD1306_INCLUDE_FONT_11x18
//#define SSD1306_INCLUDE_FONT_16x26

#endif /* __SSD1306_CONF_H__ */