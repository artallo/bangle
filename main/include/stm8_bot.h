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

#include <stdint.h>
/*#include <sys/unistd.h>
#include <types.h>*/

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hr;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} stm_time_t;

stm_time_t stm8_bot_getTime();

#endif /* __STM_BOT_H__ */