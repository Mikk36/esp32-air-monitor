#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
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

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "settings.h"

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

#define WEB_SERVER "firestore.googleapis.com"
#define WEB_PORT "443"

static const char *TAG = "example";
static char *cert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEXDCCA0SgAwIBAgINAeOpMBz8cgY4P5pTHTANBgkqhkiG9w0BAQsFADBMMSAw\n"
    "HgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFs\n"
    "U2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjAeFw0xNzA2MTUwMDAwNDJaFw0yMTEy\n"
    "MTUwMDAwNDJaMFQxCzAJBgNVBAYTAlVTMR4wHAYDVQQKExVHb29nbGUgVHJ1c3Qg\n"
    "U2VydmljZXMxJTAjBgNVBAMTHEdvb2dsZSBJbnRlcm5ldCBBdXRob3JpdHkgRzMw\n"
    "ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDKUkvqHv/OJGuo2nIYaNVW\n"
    "XQ5IWi01CXZaz6TIHLGp/lOJ+600/4hbn7vn6AAB3DVzdQOts7G5pH0rJnnOFUAK\n"
    "71G4nzKMfHCGUksW/mona+Y2emJQ2N+aicwJKetPKRSIgAuPOB6Aahh8Hb2XO3h9\n"
    "RUk2T0HNouB2VzxoMXlkyW7XUR5mw6JkLHnA52XDVoRTWkNty5oCINLvGmnRsJ1z\n"
    "ouAqYGVQMc/7sy+/EYhALrVJEA8KbtyX+r8snwU5C1hUrwaW6MWOARa8qBpNQcWT\n"
    "kaIeoYvy/sGIJEmjR0vFEwHdp1cSaWIr6/4g72n7OqXwfinu7ZYW97EfoOSQJeAz\n"
    "AgMBAAGjggEzMIIBLzAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUH\n"
    "AwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYDVR0OBBYEFHfCuFCa\n"
    "Z3Z2sS3ChtCDoH6mfrpLMB8GA1UdIwQYMBaAFJviB1dnHB7AagbeWbSaLd/cGYYu\n"
    "MDUGCCsGAQUFBwEBBCkwJzAlBggrBgEFBQcwAYYZaHR0cDovL29jc3AucGtpLmdv\n"
    "b2cvZ3NyMjAyBgNVHR8EKzApMCegJaAjhiFodHRwOi8vY3JsLnBraS5nb29nL2dz\n"
    "cjIvZ3NyMi5jcmwwPwYDVR0gBDgwNjA0BgZngQwBAgIwKjAoBggrBgEFBQcCARYc\n"
    "aHR0cHM6Ly9wa2kuZ29vZy9yZXBvc2l0b3J5LzANBgkqhkiG9w0BAQsFAAOCAQEA\n"
    "HLeJluRT7bvs26gyAZ8so81trUISd7O45skDUmAge1cnxhG1P2cNmSxbWsoiCt2e\n"
    "ux9LSD+PAj2LIYRFHW31/6xoic1k4tbWXkDCjir37xTTNqRAMPUyFRWSdvt+nlPq\n"
    "wnb8Oa2I/maSJukcxDjNSfpDh/Bd1lZNgdd/8cLdsE3+wypufJ9uXO1iQpnh9zbu\n"
    "FIwsIONGl1p3A8CgxkqI/UAih3JaGOqcpcdaCIzkBaR9uYQ1X4k2Vg5APRLouzVy\n"
    "7a8IVk6wuy6pm+T7HT4LY8ibS5FEZlfAFLSW8NwsVz9SBK2Vqn1N0PIMn5xA6NZV\n"
    "c7o835DLAFshEWfC7TIe3g==\n"
    "-----END CERTIFICATE-----\n";

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

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

void postFirebase(
    struct climateData *data,
    mbedtls_net_context *server_fd,
    int *ret,
    int *flags,
    mbedtls_ssl_context *ssl,
    char *request,
    char *requestData,
    char *time,
    char *timestamp)
{
    sprintf(requestData,
            "{\"fields\":{"
            "\"sensorId\":{\"integerValue\":1},"
            "\"temperature\":{\"doubleValue\":%f},"
            "\"humidity\":{\"doubleValue\":%f},"
            "\"pressure\":{\"doubleValue\":%f},"
            "\"co2\":{\"integerValue\":%d},"
            "\"time\":{\"timestampValue\":\"%s\"}"
            "}}\r\n",
            data->temperature, data->humidity, data->pressure, data->co2, time);
    sprintf(request,
            "POST %s?documentId=1_%s&key=%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            WEB_URL, timestamp, WEB_KEY, WEB_SERVER, strlen(requestData), requestData);

    /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    // ESP_LOGI(TAG, "Connected to AP");

    mbedtls_net_init(server_fd);

    // ESP_LOGI(TAG, "Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

    if ((*ret = mbedtls_net_connect(server_fd, WEB_SERVER,
                                    WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -*ret);
        goto exit;
    }

    // ESP_LOGI(TAG, "Connected.");

    mbedtls_ssl_set_bio(ssl, server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    // ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

    while ((*ret = mbedtls_ssl_handshake(ssl)) != 0)
    {
        if (*ret != MBEDTLS_ERR_SSL_WANT_READ && *ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -*ret);
            goto exit;
        }
    }

    // ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

    if ((*flags = mbedtls_ssl_get_verify_result(ssl)) != 0)
    {
        /* In real life, we probably want to close connection if ret != 0 */
        // ESP_LOGW(TAG, "Failed to verify peer certificate!");
        // bzero(*buf, sizeof(*buf));
        // mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
        // ESP_LOGW(TAG, "verification info: %s", buf);
    }
    else
    {
        // ESP_LOGI(TAG, "Certificate verified.");
    }

    // ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));

    // ESP_LOGI(TAG, "Writing HTTP request...");
    // ESP_LOGI(TAG, "Request: %s", request);

    size_t written_bytes = 0;
    do
    {
        *ret = mbedtls_ssl_write(ssl,
                                 (const unsigned char *)request + written_bytes,
                                 strlen(request) - written_bytes);
        if (*ret >= 0)
        {
            // ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += *ret;
        }
        else if (*ret != MBEDTLS_ERR_SSL_WANT_WRITE && *ret != MBEDTLS_ERR_SSL_WANT_READ)
        {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -*ret);
            goto exit;
        }
    } while (written_bytes < strlen(request));

    // ESP_LOGI(TAG, "Reading HTTP response...");

    // do
    // {
    //     len = sizeof(buf) - 1;
    //     bzero(buf, sizeof(buf));
    //     ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);

    //     if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    //         continue;

    //     if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
    //     {
    //         ret = 0;
    //         break;
    //     }

    //     if (ret < 0)
    //     {
    //         ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
    //         break;
    //     }

    //     if (ret == 0)
    //     {
    //         ESP_LOGI(TAG, "connection closed");
    //         break;
    //     }

    //     len = ret;
    //     ESP_LOGD(TAG, "%d bytes read", len);
    //     /* Print response directly to stdout as it is read */
    //     for (int i = 0; i < len; i++)
    //     {
    //         putchar(buf[i]);
    //     }
    // } while (1);

    mbedtls_ssl_close_notify(ssl);

exit:
    mbedtls_ssl_session_reset(ssl);
    mbedtls_net_free(server_fd);

    // if (ret != 0)
    // {
    //     mbedtls_strerror(ret, buf, 100);
    //     ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
    // }

    // putchar('\n'); // JSON output doesn't have a newline at end
}

void reporter_task(void *pvParameter)
{
    struct climateData *data = pvParameter;
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // char buf[512];
    int ret, flags;
    char requestData[300];
    char request[512];

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    // ESP_LOGI(TAG, "Seeding the random number generator");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     NULL, 0)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        abort();
    }

    // ESP_LOGI(TAG, "Loading the CA root certificate...");

    ret = mbedtls_x509_crt_parse(&cacert, (unsigned char *)cert, strlen(cert) + 1);

    if (ret < 0)
    {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        abort();
    }

    // ESP_LOGI(TAG, "Setting hostname for TLS session...");

    /* Hostname set here should match CN in server certificate */
    if ((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        abort();
    }

    // ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        // goto exit;
    }

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.
       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        // goto exit;
    }

    time_t now;
    struct tm timeinfo;
    char time_buf[64];
    char timestamp_buf[16];

    while (1)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(time_buf, sizeof(time_buf), "%FT%TZ", &timeinfo);
        strftime(timestamp_buf, sizeof(timestamp_buf), "%Y%m%d%H%M%S", &timeinfo);

        printf(
            "Temperature: %.2f C Humidity: %.2f %% Pressure: %7.2f hPa CO2: %4d PPM, Free heap: %.1f kB\n",
            data->temperature,
            data->humidity,
            data->pressure,
            data->co2,
            (float)esp_get_free_heap_size() / 1024);

        postFirebase(data, &server_fd, &ret, &flags, &ssl, request, requestData, time_buf, timestamp_buf);

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    struct climateData data;

    printf("Hello World!\n");

    nvs_init();
    i2c_init();

    wifi_event_group = xEventGroupCreate();

    int led_pin = 2;
    xTaskCreate(&blink_task, "blink_task", 1000, &led_pin, 5, NULL);
    xTaskCreate(&pressure_task, "pressure_task", 2500, &data, 10, NULL);
    xTaskCreate(&temperature_humidity_task, "temperature_humidity_task", 2500, &data, 10, NULL);
    xTaskCreate(&co2_task, "co2_task", 2500, &data, 10, NULL);
    xTaskCreate(&reporter_task, "reporter_task", 20000, &data, 8, NULL);
    xTaskCreate(&wifi_task, "wifi_task", 2500, NULL, 5, NULL);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];

    // Set timezone to UTC and print time
    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%FT%TZ", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
}
