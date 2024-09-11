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

// Struct for display state machine
typedef struct {
    display_state_t state;
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
static void screen_pool_connect(GlobalState *);

static void normal_mode(GlobalState *);
static void main_screen(GlobalState *, uint8_t);

void DISPLAY_task(void * pvParameters) {
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    //SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    /* Attempt to create the event group. */
    displayEventGroup = xEventGroupCreate();
    EventBits_t eventBits = 0;
    if (displayEventGroup == NULL) {
        ESP_LOGE(TAG, "Display Event group creation failed");
    }

    // Initialize the display state machine
    displayStateMachine.state = DISPLAY_STATE_SPLASH;

    if (Display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Display");
        return;
    }

    init_gpio(); // Initialize GPIO for button input

    ESP_LOGI(TAG, "DISPLAY_task started");

    // Main task loop
    while (1) {
        ESP_LOGI(TAG, "Display state: %d", displayStateMachine.state);

        //catch overheat event
        if (eventBits & OVERHEAT_BIT) {
            displayStateMachine.state = DISPLAY_STATE_ERROR;
        }

        //clear display
        System_clear_display(GLOBAL_STATE);

        switch (displayStateMachine.state) {
            case DISPLAY_STATE_SPLASH:
                splash_screen(GLOBAL_STATE);
                break;

            case DISPLAY_STATE_NET_CONNECT:
                System_init_connection(GLOBAL_STATE);
                break;

            case DISPLAY_STATE_POOL_CONNECT:
                screen_pool_connect(GLOBAL_STATE);
                break;

            case DISPLAY_STATE_MINING_INIT:
                normal_mode(GLOBAL_STATE);
                break;

            case DISPLAY_STATE_ERROR:
                System_show_overheat_screen(GLOBAL_STATE);
                SYSTEM_update_overheat_mode(GLOBAL_STATE);  // Check for changes
                break;
            default:
                // Handle unknown state
                break;
        }

        eventBits = 0; // Reset eventBits for the next iteration

        //wait here for an event or timeout
        eventBits = xEventGroupWaitBits(displayEventGroup,
            OVERHEAT_BIT | UPDATE, //events to wait for
            pdTRUE, pdFALSE,
            portMAX_DELAY); //timeout length - forever
    }

}

// normal mode display loop
//
static void normal_mode(GlobalState * GLOBAL_STATE) {
    //SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
    EventBits_t eventBits = 0;

    while (1) {
        if (eventBits & EXIT) {
            displayStateMachine.state = DISPLAY_STATE_ERROR;
            return;
        }
        if (eventBits & BUTTON_BIT) {
            // Handle button press
            //ESP_LOGI(TAG, "Button pressed, next screen: %d", page);
            //debounce button
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        if (eventBits == 0) {
            // No events, update display
            main_screen(GLOBAL_STATE, (UPDATE_HASHRATE | UPDATE_SHARES | UPDATE_BD));
        } else {
            main_screen(GLOBAL_STATE, eventBits);
        }

        //wait here for an event or timeout
        eventBits = 0; // Reset uxBits for the next iteration
        eventBits = xEventGroupWaitBits(displayEventGroup, BUTTON_BIT | UPDATE_HASHRATE | UPDATE_SHARES | UPDATE_BD | EXIT, pdTRUE, pdFALSE, (10000 / portTICK_PERIOD_MS));
    }
}

void Display_normal_update(uint8_t update_type) {
    xEventGroupSetBits(displayEventGroup, update_type);
}

void Display_net_connect_state(void) {
    displayStateMachine.state = DISPLAY_STATE_NET_CONNECT;

    //set event bits to trigger display update
    xEventGroupSetBits(displayEventGroup, UPDATE);

}

void Display_pool_connect_state(void) {
    displayStateMachine.state = DISPLAY_STATE_POOL_CONNECT;

    //set event bits to trigger display update
    xEventGroupSetBits(displayEventGroup, UPDATE);

}

void Display_mining_init_state(void) {
    displayStateMachine.state = DISPLAY_STATE_MINING_INIT;

    //set event bits to trigger display update
    xEventGroupSetBits(displayEventGroup, UPDATE);

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

static void main_screen(GlobalState * GLOBAL_STATE, uint8_t type) {
    SystemModule * system = &GLOBAL_STATE->SYSTEM_MODULE;
    PowerManagementModule * pm = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    char oled_buf[20];

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:

            OLED_clearLine(2);

            //hashrate
            if (type | UPDATE_HASHRATE) {
                float efficiency = pm->power / (system->current_hashrate / 1000.0);
                memset(oled_buf, ' ', 20);
                snprintf(oled_buf, 20, "%.0f GH/s - %.0f J/TH  ", system->current_hashrate, efficiency);
                OLED_writeString(0, 0, oled_buf);
            }

            //BD
            if (type | UPDATE_BD) {
                memset(oled_buf, ' ', 20);
                snprintf(oled_buf, 20, system->FOUND_BLOCK ? "!!! BLOCK FOUND !!!" : "BEST: %s", system->best_diff_string);
                OLED_writeString(0, 1, oled_buf);
            }

            //shares
            if (type | UPDATE_SHARES) {
                memset(oled_buf, ' ', 20);
                snprintf(oled_buf, 20, "SHARES: %llu/%llu", system->shares_accepted, system->shares_rejected);
                OLED_writeString(0, 3, oled_buf);
            }

            break;
        default:
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
    //SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

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

static void screen_pool_connect(GlobalState * GLOBAL_STATE)
{
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    switch (GLOBAL_STATE->device_model) {
        case DEVICE_MAX:
        case DEVICE_ULTRA:
        case DEVICE_SUPRA:
        case DEVICE_GAMMA:
            if (OLED_status()) {
                OLED_clear();
                OLED_writeString(0, 0, "Connecting to Pool:");
                OLED_writeString(0, 1, module->pool_url);
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

static void gpio_isr_handler(void* arg) {
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