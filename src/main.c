#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "co2.h"
#include "pressure.h"
#include "si7021.h"
#include "main.h"
#include "wifi.h"
#include "apps/sntp/sntp.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "settings.h"

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static const char *TAG = "example";

void nvs_init()
{
    printf("nvs_flash_init\n");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        printf("No free pages, erasing NVS\n");
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    printf("NVS successfully initialized\n");
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

void postWeatherServer(
    struct climateData *data,
    char *request,
    char *requestData)
{
    sprintf(requestData,
            "{"
            "\"sensorId\":1,"
            "\"temperature\":%f,"
            "\"humidity\":%f,"
            "\"pressure\":%f,"
            "\"co2\":%d"
            "}\r\n",
            data->temperature, data->humidity, data->pressure, data->co2);
    sprintf(request,
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            WEB_URL, WEB_SERVER, strlen(requestData), requestData);

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    // struct in_addr *addr;
    int s;

    int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

    if (err != 0 || res == NULL)
    {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    /* Code to print the resolved IP.
           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
    // addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    // ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0)
    {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
        return;
    }
    // ESP_LOGI(TAG, "... allocated socket");

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
    {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        return;
    }

    // ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    if (write(s, request, strlen(request)) < 0)
    {
        ESP_LOGE(TAG, "... socket send failed");
        close(s);
        return;
    }
    // ESP_LOGI(TAG, "... socket send success");

    close(s);
}

void reporter_task(void *pvParameter)
{
    struct climateData *data = pvParameter;
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    char requestData[300];
    char request[512];

    while (1)
    {
        printf(
            "Temperature: %.2f C Humidity: %.2f %% Pressure: %7.2f hPa CO2: %4d PPM, Free heap: %.1f kB\n",
            data->temperature,
            data->humidity,
            data->pressure,
            data->co2,
            (float)esp_get_free_heap_size() / 1024);

        postWeatherServer(data, request, requestData);

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    struct climateData data;

    nvs_init();
    i2c_init();
    wifi_event_group = xEventGroupCreate();
    initialise_wifi();

    co2_init();

    start_wifi();

    uint8_t counter = 0;
    while (counter < 100)
    {
        counter++;
        read_pressure(&data);
        read_temp_rh(&data.temperature, &data.humidity);
        data.co2 = co2_read();

        // printf("Waking wifi\n");
        // start_wifi();
        // wake_wifi();

        char requestData[300];
        char request[512];
        postWeatherServer(&data, request, requestData);

        printf(
            "Temperature: %.2f C Humidity: %.2f %% Pressure: %7.2f hPa CO2: %4d PPM, Free heap: %.1f kB\n",
            data.temperature,
            data.humidity,
            data.pressure,
            data.co2,
            (float)esp_get_free_heap_size() / 1024);

        // printf("Sleeping wifi\n");
        // stop_wifi();
        sleep_wifi();

        // printf("Entering light sleep\n");
        // const int wakeup_time_sec = 15;
        // esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
        // // esp_deep_sleep_start();
        // esp_light_sleep_start();

        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
    fflush(stdout);
    esp_restart();
}
