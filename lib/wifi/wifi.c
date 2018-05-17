#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
extern EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
extern const int CONNECTED_BIT;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static char *getAuthModeName(wifi_auth_mode_t auth_mode)
{

    char *names[] = {"OPEN", "WEP", "WPA PSK", "WPA2 PSK", "WPA WPA2 PSK", "MAX"};
    return names[auth_mode];
}

void initialise_wifi()
{
    esp_log_level_set("wifi", ESP_LOG_NONE);
    // printf("initialise_wifi step 1\n");
    tcpip_adapter_init();
    // printf("initialise_wifi step 2\n");
    // wifi_event_group = xEventGroupCreate();
    // printf("initialise_wifi step 3\n");
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    // printf("initialise_wifi step 4\n");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // printf("initialise_wifi step 5\n");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // printf("initialise_wifi step 6\n");
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI("wifi", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    // printf("initialise_wifi step 7\n");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // printf("initialise_wifi step 8\n");
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    // printf("initialise_wifi step 9\n");
    ESP_ERROR_CHECK(esp_wifi_start());
}

void scan_networks()
{
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true};

    printf("Start scanning...");
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    printf(" completed!\n");
    printf("\n");

    // get the list of APs found in the last scan
    uint16_t ap_num = MAX_APs;
    wifi_ap_record_t ap_records[MAX_APs];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    // print the list
    printf("Found %d access points:\n", ap_num);
    printf("\n");
    printf("               SSID              | Channel | RSSI |   Auth Mode \n");
    printf("----------------------------------------------------------------\n");
    for (int i = 0; i < ap_num; i++)
        printf("%32s | %7d | %4d | %12s\n", (char *)ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi, getAuthModeName(ap_records[i].authmode));
    printf("----------------------------------------------------------------\n");
}

void wifi_task(void *pvParameter)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    initialise_wifi();
    printf("Waiting for WiFi connection\n");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    printf("Connected to WiFi\n");

    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
    printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
    printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));

    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}