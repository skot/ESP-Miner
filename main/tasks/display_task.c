#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"

#include "oled.h"
#include "system.h"
#include "display_task.h"

// Enum for display states
typedef enum {
    DISPLAY_STATE_SPLASH,
    DISPLAY_STATE_INIT,
    DISPLAY_STATE_MINING,
    DISPLAY_STATE_ERROR,
} DisplayState;

// Struct for display state machine
typedef struct {
    DisplayState state;
} DisplayStateMachine;

static DisplayStateMachine displayStateMachine;

// Declare a variable to hold the created event group.
EventGroupHandle_t displayEventGroup;

//static variables
static const char * TAG = "DisplayTask";

//static function prototypes
static void IRAM_ATTR gpio_isr_handler(void* arg);
static void init_gpio(void);
static void splash_screen(GlobalState *);

void DISPLAY_task(void * pvParameters) {
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    /* Attempt to create the event group. */
    displayEventGroup = xEventGroupCreate();
    EventBits_t eventBits;
    if (displayEventGroup == NULL) {
        ESP_LOGE(TAG, "Display Event group creation failed");
    }

    // Initialize the display state machine
    displayStateMachine.state = DISPLAY_STATE_INIT;

    if (Display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Display");
        return;
    }

    init_gpio(); // Initialize GPIO for button input

    module->screen_page = 0;

    ESP_LOGI(TAG, "DISPLAY_task started");

    // Main task loop
    while (1) {
        /* Wait a maximum of 10s for either bit 0 or bit 4 to be set within
          the event group. Clear the bits before exiting. */
        eventBits = xEventGroupWaitBits(
                displayEventGroup,   /* The event group being tested. */
                BUTTON_BIT | OVERHEAT_BIT, /* The bits within the event group to wait for. */
                pdTRUE,        /* BIT_0 & BIT_4 should be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                (10000 / portTICK_PERIOD_MS) ); /* Wait a maximum of 10s for either bit to be set. */

        //debounce button
        if (eventBits & BUTTON_BIT) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //catch overheat event
        if (eventBits & OVERHEAT_BIT) {
            displayStateMachine.state = DISPLAY_STATE_ERROR;
        }

        //clear display
        System_clear_display(GLOBAL_STATE);

        switch (displayStateMachine.state) {
            case DISPLAY_STATE_SPLASH:
                splash_screen(GLOBAL_STATE);
                displayStateMachine.state = DISPLAY_STATE_INIT;
                break;

            case DISPLAY_STATE_INIT:
                System_init_connection(GLOBAL_STATE);
                break;

            case DISPLAY_STATE_MINING:

                if (eventBits & BUTTON_BIT) {
                // Handle button press
                ESP_LOGI(TAG, "Button pressed, next screen: %d", module->screen_page);
                // Handle button press logic here
                } else {
                    ESP_LOGI(TAG, "No button press detected, cycling through screens");
                }

                switch (module->screen_page) {
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
                module->screen_page = (module->screen_page + 1) % 3;

                break;
            case DISPLAY_STATE_ERROR:
                System_show_overheat_screen(GLOBAL_STATE);
                SYSTEM_update_overheat_mode(GLOBAL_STATE);  // Check for changes
                break;
            default:
                // Handle unknown state
                break;
        }

        eventBits = 0; // Reset uxBits for the next iteration
    }

}

void Display_init_state(void) {
    displayStateMachine.state = DISPLAY_STATE_INIT;

    //set event bits to trigger display update
    xEventGroupSetBits(displayEventGroup, eBIT_4);

}

void Display_mining_state(void) {
    displayStateMachine.state = DISPLAY_STATE_MINING;

    //set event bits to trigger display update
    xEventGroupSetBits(displayEventGroup, eBIT_4);

}

esp_err_t Display_init(void) {

    // oled
    if (!OLED_init()) {
        ESP_LOGI(TAG, "OLED init failed!");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "OLED init success!");
        // clear the oled screen
        OLED_fill(0);
        return ESP_OK;
    }
}

void Display_bad_NVS(void) {
    if (!OLED_init()) {
        OLED_clear();
        OLED_writeString(0, 0, "NVS load failed!");
    }
    return;
}

static void splash_screen(GlobalState * GLOBAL_STATE) {
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    //create buffer for display data
    char display_data[20];

    snprintf(display_data, 20, "bitaxe%s %d", GLOBAL_STATE->device_model_str, GLOBAL_STATE->board_version);
    ESP_LOGI(TAG, "Displaying splash screen: %s", display_data);

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:

            if (OLED_status()) {
                OLED_clear();
                OLED_writeString(0, 0, display_data);
                OLED_writeString(0, 2, esp_app_get_description()->version);
            }
            break;
        default:
    }
}


void System_init_connection(GlobalState * GLOBAL_STATE)
{
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            if (OLED_status()) {
                OLED_clear();
                OLED_writeString(0, 0, "Connecting to WiFi:");
                OLED_writeString(0, 1, module->ssid);
            }
            break;
        default:
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
                                displayEventGroup,   // The event group being updated.
                                BUTTON_BIT,             // The bits being set.
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