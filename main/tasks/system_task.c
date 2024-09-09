#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "freertos/event_groups.h"

#include "system.h"
#include "system_task.h"
#include "connect.h"

QueueHandle_t user_input_queue;

// Declare a variable to hold the created event group.
EventGroupHandle_t systemEventGroup;

static const char * TAG = "SystemTask";
static void IRAM_ATTR gpio_isr_handler(void* arg);
static void init_gpio(void);

static uint8_t current_screen;

void SYSTEM_task(void * pvParameters) {

    /* Attempt to create the event group. */
    systemEventGroup = xEventGroupCreate();
    EventBits_t eventBits;

    if (systemEventGroup == NULL) {
        ESP_LOGE(TAG, "Event group creation failed");
    }

    init_gpio(); // Initialize GPIO for button input

    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    System_init_system(GLOBAL_STATE);
    user_input_queue = xQueueCreate(10, sizeof(char[10])); // Create a queue to handle user input events

    System_clear_display(GLOBAL_STATE);
    System_init_connection(GLOBAL_STATE);

    char input_event[10];
    ESP_LOGI(TAG, "SYSTEM_task started");

    while (GLOBAL_STATE->ASIC_functions.init_fn == NULL) {
        System_show_ap_information("ASIC MODEL INVALID", GLOBAL_STATE);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    current_screen = 0;

    while (1) {

        /* Wait a maximum of 100ms for either bit 0 or bit 4 to be set within
          the event group. Clear the bits before exiting. */
        eventBits = xEventGroupWaitBits(
                systemEventGroup,   /* The event group being tested. */
                eBIT_0 | eBIT_4, /* The bits within the event group to wait for. */
                pdTRUE,        /* BIT_0 & BIT_4 should be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                (10000 / portTICK_PERIOD_MS) ); /* Wait a maximum of 10s for either bit to be set. */

        // Check for overheat mode
        if (module->overheat_mode == 1) {
            System_show_overheat_screen(GLOBAL_STATE);
            vTaskDelay(5000 / portTICK_PERIOD_MS);  // Update every 5 seconds
            SYSTEM_update_overheat_mode(GLOBAL_STATE);  // Check for changes
            continue;  // Skip the normal screen cycle
        }

        if (eventBits & eBIT_0) {
            // Handle button press
            ESP_LOGI(TAG, "Button pressed, switching to next screen");
            // Handle button press logic here
        } else {
            ESP_LOGI(TAG, "No button press detected, cycling through screens");
        }

        // Automatically cycle through screens
        System_clear_display(GLOBAL_STATE);

        switch (current_screen) {
            case 0:
                System_update_system_performance(GLOBAL_STATE);
                break;
            case 1:
                System_update_system_info(GLOBAL_STATE);
                break;
            case 2:
                System_update_esp32_info(GLOBAL_STATE);
                break;
        }

        eventBits = 0; // Reset uxBits for the next iteration
        current_screen = (current_screen + 1) % 3;
    }
}

//setup the GPIO for the button with interrupt
static void init_gpio(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Interrupt on any edge
    io_conf.mode = GPIO_MODE_INPUT; // Set as input mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Enable pull-up resistor
    io_conf.pin_bit_mask = (1ULL << BUTTON_BOOT); // Set GPIO 0
    gpio_config(&io_conf);

    // Install ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_BOOT, gpio_isr_handler, (void*) BUTTON_BOOT);
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    // uint32_t gpio_num = (uint32_t) arg;
    // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);

    BaseType_t xHigherPriorityTaskWoken, xResult;

    // xHigherPriorityTaskWoken must be initialised to pdFALSE.
    xHigherPriorityTaskWoken = pdFALSE;

    // Set bit 0 and bit 4 in xEventGroup.
    xResult = xEventGroupSetBitsFromISR(
                                systemEventGroup,   // The event group being updated.
                                eBIT_0,             // The bits being set.
                                &xHigherPriorityTaskWoken);

    // Was the message posted successfully?
    if (xResult != pdFAIL) {
        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
           switch should be requested. The macro used is port specific and will
           be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
           the documentation page for the port being used. */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
