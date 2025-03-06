#include "frequency_transition.h"
#include "bm1366.h"
#include "bm1368.h"
#include "bm1370.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

const char *FREQUENCY_TRANSITION_TAG = "frequency_transition";

// Track current frequency for each ASIC type
static float current_frequency_1366 = 56.25;
static float current_frequency_1368 = 56.25;
static float current_frequency_1370 = 56.25;

bool do_frequency_transition(float target_frequency, int asic_type) {
    float step = 6.25;
    float current;
    float target = target_frequency;

    // Select the appropriate current frequency based on ASIC type
    switch (asic_type) {
        case 1366:
            current = current_frequency_1366;
            break;
        case 1368:
            current = current_frequency_1368;
            break;
        case 1370:
            current = current_frequency_1370;
            break;
        default:
            ESP_LOGE(FREQUENCY_TRANSITION_TAG, "Unknown ASIC type: %d", asic_type);
            return false;
    }

    float direction = (target > current) ? step : -step;

    // If current frequency is not a multiple of step, adjust to the nearest multiple
    if (fmod(current, step) != 0) {
        float next_dividable;
        if (direction > 0) {
            next_dividable = ceil(current / step) * step;
        } else {
            next_dividable = floor(current / step) * step;
        }
        current = next_dividable;
        
        // Call the appropriate send_hash_frequency function based on ASIC type
        switch (asic_type) {
            case 1366:
                BM1366_send_hash_frequency(current);
                break;
            case 1368:
                BM1368_send_hash_frequency(current);
                break;
            case 1370:
                BM1370_send_hash_frequency(-1, current, 0.001);
                break;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Gradually adjust frequency in steps until target is reached
    while ((direction > 0 && current < target) || (direction < 0 && current > target)) {
        float next_step = fmin(fabs(direction), fabs(target - current));
        current += direction > 0 ? next_step : -next_step;
        
        // Call the appropriate send_hash_frequency function based on ASIC type
        switch (asic_type) {
            case 1366:
                BM1366_send_hash_frequency(current);
                break;
            case 1368:
                BM1368_send_hash_frequency(current);
                break;
            case 1370:
                BM1370_send_hash_frequency(-1, current, 0.001);
                break;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // Set the final target frequency
    switch (asic_type) {
        case 1366:
            BM1366_send_hash_frequency(target);
            current_frequency_1366 = target;
            break;
        case 1368:
            BM1368_send_hash_frequency(target);
            current_frequency_1368 = target;
            break;
        case 1370:
            BM1370_send_hash_frequency(-1, target, 0.001);
            current_frequency_1370 = target;
            break;
    }
    
    ESP_LOGI(FREQUENCY_TRANSITION_TAG, "Successfully transitioned ASIC type %d to %.2f MHz", asic_type, target);
    return true;
}
