#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "global_state.h"
#include "display.h"
#include "screen.h"
#include "lwip/lwip_napt.h"

static const char * TAG = "screen";

extern const lv_img_dsc_t logo;

static lv_obj_t * screens[MAX_SCREENS];

static screen_t current_screen = -1;
static TickType_t current_screen_counter;

static GlobalState * GLOBAL_STATE;

static char ip_address_str[IP4ADDR_STRLEN_MAX];

static lv_obj_t *hashrate_label;
static lv_obj_t *efficiency_label;
static lv_obj_t *difficulty_label;
static lv_obj_t *chip_temp_label;

static lv_obj_t *self_test_labels[2];

static double current_hashrate;
static float current_power;
static uint64_t current_difficulty;
static float curreny_chip_temp;

#define SCREEN_UPDATE_MS 500
#define LOGO_DELAY_COUNT 5000/SCREEN_UPDATE_MS
#define CAROUSEL_DELAY_COUNT 10000 / SCREEN_UPDATE_MS

static lv_obj_t * create_scr_self_test(SystemModule * module) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "BITAXE SELF TEST");
    self_test_labels[0] = lv_label_create(scr);
    self_test_labels[1] = lv_label_create(scr);

    return scr;
}

static lv_obj_t * create_scr_overheat(SystemModule * module) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "DEVICE OVERHEAT!");
    lv_obj_t *label2 = lv_label_create(scr);
    lv_label_set_text(label2, "Power, frequency and fan configurations have been reset. Go to AxeOS to reconfigure device.");
    lv_label_set_long_mode(label2, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_t *label3 = lv_label_create(scr);
    lv_label_set_text(label3, "Device IP:");
    lv_obj_t *label4 = lv_label_create(scr);
    lv_label_set_text_static(label4, ip_address_str);

    return scr;
}

static lv_obj_t * create_scr_configure(SystemModule * module) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "Welcome to your new BitAxe! Connect to the configuration Wifi and connect the BitAxe to your network.");
    lv_label_set_long_mode(label1, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_t *label2 = lv_label_create(scr);
    lv_label_set_text(label2, "Configuration SSID:");
    lv_obj_t *label3 = lv_label_create(scr);
    lv_label_set_text(label3, module->ap_ssid);

    return scr;
}

static lv_obj_t * create_scr_connection(SystemModule * module) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "Connecting to SSID:");
    lv_obj_t *label2 = lv_label_create(scr);
    lv_label_set_text(label2, module->ssid);
    lv_obj_t *label3 = lv_label_create(scr);
    lv_label_set_text(label3, "Configuration SSID:");
    lv_obj_t *label4 = lv_label_create(scr);
    lv_label_set_text(label4, module->ap_ssid);

    return scr;
}

static lv_obj_t * create_scr_logo() {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &logo);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    return scr;
}

static lv_obj_t * create_scr_urls(SystemModule * module) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "Mining URL:");
    lv_obj_t *label2 = lv_label_create(scr);
    lv_label_set_text(label2, module->is_using_fallback ? module->fallback_pool_url : module->pool_url);
    lv_obj_t *label3 = lv_label_create(scr);
    lv_label_set_text(label3, "Bitaxe IP:");
    lv_obj_t *label4 = lv_label_create(scr);
    lv_label_set_text_static(label4, ip_address_str);

    return scr;
}

static lv_obj_t * create_scr_stats(SystemModule * module, PowerManagementModule * power_management) {
    lv_obj_t * scr = lv_obj_create(NULL);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    hashrate_label = lv_label_create(scr);
    lv_label_set_text(hashrate_label, "Gh/s: n/a");
    efficiency_label = lv_label_create(scr);
    lv_label_set_text(efficiency_label, "J/Th: n/a");
    difficulty_label = lv_label_create(scr);
    lv_label_set_text(difficulty_label, "Best: n/a");
    chip_temp_label = lv_label_create(scr);
    lv_label_set_text(chip_temp_label, "Temp: n/a");

    return scr;
}

static void screen_show(screen_t screen)
{
    if (current_screen != screen) {
        lv_obj_t * scr = screens[screen];

        if (scr && lvgl_port_lock(0)) {
            lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, LV_DEF_REFR_PERIOD * 128 / 8, 0, false);
            lvgl_port_unlock();
        }

        current_screen = screen;
        current_screen_counter = 0;
    }
}

static void screen_update_cb(lv_timer_t * timer) 
{
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
    PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

    if (module->overheat_mode == 1) {
        screen_show(SCR_OVERHEAT);
        return;
    }

    if (GLOBAL_STATE->SELF_TEST_MODULE.running) {
        screen_show(SCR_SELF_TEST);
        return;
    }

    if (current_screen == SCR_SELF_TEST) {

        return;
    }

    if (GLOBAL_STATE->ASIC_functions.init_fn == NULL) {
        // Invalid model
        // TODO Not sure what should be done here?
        return;
    }

    if (module->ssid[0] == '\0') {
        screen_show(SCR_CONFIGURE);
        return;
    }

    if (!module->startup_done) {
        screen_show(SCR_CONNECTION);
        return;
    }

    current_screen_counter++;

    // Logo

    if (current_screen < SCR_LOGO) {
        screen_show(SCR_LOGO);
        return;
    }

    if (current_screen == SCR_LOGO) {
        if (LOGO_DELAY_COUNT > current_screen_counter) {
            return;
        }
        screen_show(SCR_CAROUSEL_START);
        return;
    }

    if (module->FOUND_BLOCK) {
        // TODO make special screen for this
    }

    // Carousel

    if (current_hashrate != module->current_hashrate) {
        lv_label_set_text_fmt(hashrate_label, "Gh/s: %.2f", module->current_hashrate);
    }

    if (current_power != power_management->power || current_hashrate != module->current_hashrate) {
        lv_label_set_text_fmt(efficiency_label, "J/Th: %.2f", power_management->power / (module->current_hashrate / 1000.0));
    }

    if (current_difficulty != module->best_session_nonce_diff) {
        lv_label_set_text_fmt(difficulty_label, "Best: %s/%s", module->best_session_diff_string, module->best_diff_string);
    }

    if (curreny_chip_temp != power_management->chip_temp_avg) {
        lv_label_set_text_fmt(chip_temp_label, "Temp: %.1f C", power_management->chip_temp_avg);
    }

    current_hashrate = module->current_hashrate;
    current_power = power_management->power;
    current_difficulty = module->best_session_nonce_diff;
    curreny_chip_temp = power_management->chip_temp_avg;

    if (CAROUSEL_DELAY_COUNT > current_screen_counter) {
        return;
    }

    screen_next();
}

void screen_next() 
{
    if (current_screen >= SCR_CAROUSEL_START) {
        screen_show(current_screen == SCR_CAROUSEL_END ? SCR_CAROUSEL_START : current_screen + 1);
    }
}

static void ip_event_handler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t * event = (ip_event_got_ip_t *) event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_address_str, IP4ADDR_STRLEN_MAX);
    }
}

esp_err_t screen_start(void * pvParameters)
{
    GLOBAL_STATE = (GlobalState *) pvParameters;

    if (display_active()) {
        SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
        PowerManagementModule * power_management = &GLOBAL_STATE->POWER_MANAGEMENT_MODULE;

        screens[SCR_SELF_TEST] = create_scr_self_test(module);
        screens[SCR_OVERHEAT] = create_scr_overheat(module);
        screens[SCR_CONFIGURE] = create_scr_configure(module);
        screens[SCR_CONNECTION] = create_scr_connection(module);
        screens[SCR_LOGO] = create_scr_logo();
        screens[SCR_URLS] = create_scr_urls(module);
        screens[SCR_STATS] = create_scr_stats(module, power_management);

        lv_timer_create(screen_update_cb, SCREEN_UPDATE_MS, NULL);

        esp_event_handler_instance_t instance_got_ip;
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, &instance_got_ip), TAG, "Error registering IP event handler");
    }

    return ESP_OK;
}

// TODO: Transition code until self test code is revamped
void display_show_status(const char *messages[], size_t message_count)
{
    if (lvgl_port_lock(0)) {

        screen_show(SCR_SELF_TEST);
        // messages[0] is ignored, it's already on the screen
        lv_label_set_text(self_test_labels[0], messages[1]);
        lv_label_set_text(self_test_labels[1], messages[2]);
        lvgl_port_unlock();
    }
}

