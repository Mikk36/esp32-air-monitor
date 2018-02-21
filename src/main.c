/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "co2.h"
#include "pressure.h"
#include "si7021.h"
#include "main.h"

void blink_task(void *pvParameter)
{
    int led_pin = *((int *)pvParameter);

    gpio_pad_select_gpio(led_pin);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);
    while (1)
    {
        /* Blink on (output high) */
        gpio_set_level(led_pin, 1);
        vTaskDelay(25 / portTICK_PERIOD_MS);
        /* Blink off (output low) */
        gpio_set_level(led_pin, 0);
        vTaskDelay(4975 / portTICK_PERIOD_MS);
    }
}

// I2C
static void i2c_init()
{
    int i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 33;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = 32;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

// void task_i2cscanner(void *pvParameter)
// {
//     int i;
//     esp_err_t espRc;

//     while (1)
//     {
//         printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
//         printf("00:         ");
//         for (i = 40; i < 0xa0; i++)
//         {
//             i2c_cmd_handle_t cmd = i2c_cmd_link_create();
//             i2c_master_start(cmd);
//             i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
//             i2c_master_stop(cmd);

//             espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 50 / portTICK_PERIOD_MS);
//             if (i % 16 == 0)
//             {
//                 printf("\n%.2x:", i);
//             }
//             if (espRc == 0)
//             {
//                 printf(" %.2x", i);
//             }
//             else
//             {
//                 printf(" --");
//             }
//             //ESP_LOGD(tag, "i=%d, rc=%d (0x%x)", i, espRc, espRc);
//             i2c_cmd_link_delete(cmd);
//         }
//         printf("\n");
//         vTaskDelay(4500 / portTICK_PERIOD_MS);
//     }
// }

void task_reporter(void *pvParameter)
{
    struct climateData *data = pvParameter;
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    while (1)
    {
        printf("Temperature: %.2f C\n", data->temperature);
        printf("Humidity: %.2f %%\n", data->humidity);
        printf("Pressure: %.2f hPa\n", data->pressure);
        printf("CO2: %d PPM\n", data->co2);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    struct climateData data;

    printf("Hello World!\n");
    int led_pin = 2;
    xTaskCreate(&blink_task, "blink_task", 1000, &led_pin, 5, NULL);

    i2c_init();

    co2_init();
    xTaskCreate(&co2_rx_task, "co2_rx_task", 1500, &data, configMAX_PRIORITIES, NULL);
    xTaskCreate(&co2_tx_task, "co2_tx_task", 1000, NULL, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(&pressure_task, "pressure_task", 1500, &data, configMAX_PRIORITIES, NULL);
    xTaskCreate(&temperature_humidity_task, "temperature_humidity_task", 1500, &data, configMAX_PRIORITIES, NULL);

    xTaskCreate(&task_reporter, "task_reporter", 1500, &data, configMAX_PRIORITIES - 2, NULL);
}
