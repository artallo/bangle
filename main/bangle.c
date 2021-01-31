/**
 * @file bangle.c
 * @author Artem (artallo@mail.ru)
 * @brief Прошивка браслета
 * @version 0.1
 * @date 2020-10-28
 * 
 */
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "bno055.h"
#include "esp_ble.h"
#include "esp_config.h"
#include "i2c_bus.h"
#include "stm8_bot.h"
#include "ssd1306.h"
#include "tasks_modes.h"



//const char* msgHello = "Hello";

//LOGs




//bool BLE_Init();
//bool BLE_Deint();

bool bno055Init();

void app_main(void)
{   
    ESP_LOGI(TAG_TASK, "%s", "app_main() started");
    printf("min stack size: %d\n", configMINIMAL_STACK_SIZE);
    printf("max priorities: %d\n", configMAX_PRIORITIES);

    //NVS init
    nvs_flash_init();
    
    //i2c init
    ESP_ERROR_CHECK(i2c_master_init(GPIO_I2C_SCL, GPIO_I2C_SDA, I2C_FREQ, I2C_PORT_NUM));

    //set date/time
    stm8_bot_setTime(&(stm8_time_t){0, 0 , 18, 31, 1, 21});

    //PSU enable display power
    stm8_bot_psu_en_display(true);

    //SSD1309 display init
    ssd1306_Init(GPIO_DISPLAY_RESET);

    //bno0555 init;
    //bno055Init();

    //BT init
    BLE_Init();

    //tasks init
    tasks_init();
    
    //Main loop
    //char ch = '0';
    //char* pcWrBuf = (char*) calloc(10, 50);
    while (1) {
        /*ssd1306_SetCursor(60, 27);
        ssd1306_WriteChar(ch, Font_7x10, White);
        if (ch < 'z') {
            ch++;
        } else {
            ch = '0';
        }
        ssd1306_UpdateScreen();*/
        //vTaskList(pcWrBuf);
        //vTaskGetRunTimeStats(pcWrBuf);
        //printf("%s", pcWrBuf);
        
        //output date/time to console
        stm8_time_t t;
        memset(&t, 0, 6);
        stm8_bot_getTime(&t);
        printf("Time: %d:%d:%d Date: %d.%d.%d\n", t.hr, t.min, t.sec, t.day, t.month, t.year);*/
        
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}






/**
 * @brief Init BNO055. Set initial params, switch operation mode to NDOF.
 * 
 * @return true 
 * @return false 
 */
bool bno055Init() {
    printf("Init BNO055..\n");
    bno055_config_t bno_conf;
    esp_err_t err;
    err = bno055_set_default_conf(&bno_conf);
    err = bno055_open(&bno_conf);
    printf("bno055_open() returned 0x%02X \n", err);

    if( err != ESP_OK ) {
        printf("bno055_open error!\n");
        err = bno055_close();
        return false;
    }

    /*err = bno055_set_ext_crystal_use(1);
    if( err != ESP_OK ) {
        printf("Couldn't set external crystal use\n");
        err = bno055_close();
        return false;
    }
    vTaskDelay(1000 / portTICK_RATE_MS);*/

    err = bno055_set_opmode(OPERATION_MODE_NDOF);
    printf("bno055_set_opmode(OPERATION_MODE_NDOF) returned 0x%02x \n", err);
    if( err != ESP_OK ) {
        printf("Couldn't set NDOF mode\n");
        err = bno055_close();
        return false;
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
    return true;
}