#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "system.h"
#include "system_task.h"
#include "connect.h"

static const char * TAG = "SystemTask";

void SYSTEM_task(void * pvParameters) {

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;



    ESP_LOGI(TAG, "SYSTEM_task started");

    while (1) {

        // Check for overheat mode
        if (module->overheat_mode == 1) {
            System_show_overheat_screen(GLOBAL_STATE);
            vTaskDelay(5000 / portTICK_PERIOD_MS);  // Update every 5 seconds
            SYSTEM_update_overheat_mode(GLOBAL_STATE);  // Check for changes
            continue;  // Skip the normal screen cycle
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);  // Update every 5 seconds

    }
}


