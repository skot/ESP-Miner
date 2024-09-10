#include "esp_log.h"
#include "esp_netif.h"

#include "nvs_flash.h"
#include "nvs_config.h"
#include "nvs_device.h"

#include "display_task.h"
#include "http_server.h"
#include "network.h"

//static const char * TAG = "network";

EventBits_t Network_connect(GlobalState * GLOBAL_STATE) {

    char * wifi_ssid;
    char * wifi_pass;
    char * hostname;

    Display_init_state();

    // pull the wifi credentials and hostname out of NVS
    NVSDevice_get_wifi_creds(GLOBAL_STATE, &wifi_ssid, &wifi_pass, &hostname);

    // init and connect to wifi
    wifi_init(wifi_ssid, wifi_pass, hostname);
    start_rest_server((void *) GLOBAL_STATE);

    free(wifi_ssid);
    free(wifi_pass);
    free(hostname);

    //EventBits_t result_bits = wifi_connect(); //wait here forever for wifi to connect

    return wifi_connect(); //wait here forever for wifi to connect

    // if (result_bits & WIFI_CONNECTED_BIT) {
    //     ESP_LOGI(TAG, "Connected to SSID: %s", wifi_ssid);
    //     strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "Connected!", 20);
    // } else if (result_bits & WIFI_FAIL_BIT) {
    //     ESP_LOGE(TAG, "Failed to connect to SSID: %s", wifi_ssid);

    //     strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
    //     // User might be trying to configure with AP, just chill here
    //     ESP_LOGI(TAG, "Finished, waiting for user input.");
    //     while (1) {
    //         vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     }
    // } else {
    //     ESP_LOGE(TAG, "UNEXPECTED EVENT");
    //     strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "unexpected error", 20);
    //     // User might be trying to configure with AP, just chill here
    //     ESP_LOGI(TAG, "Finished, waiting for user input.");
    //     while (1) {
    //         vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     }
    // }
    //return ESP_OK;
}

void Network_AP_off(void) {
    wifi_softap_off();
}

void Network_set_wifi_status(GlobalState * GLOBAL_STATE, wifi_status_t status, uint16_t retry_count) {
    if (status == WIFI_RETRYING) {
        snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Retrying: %d", retry_count);
        return;
    } else if (status == WIFI_CONNECT_FAILED) {
        snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Connect Failed!");
        return;
    }
}