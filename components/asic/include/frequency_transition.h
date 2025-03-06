#ifndef FREQUENCY_TRANSITION_H
#define FREQUENCY_TRANSITION_H

#include <stdbool.h>

extern const char *FREQUENCY_TRANSITION_TAG;

/**
 * @brief Transition the ASIC frequency to a target value
 * 
 * This function gradually adjusts the ASIC frequency to reach the target value,
 * stepping up or down in increments to ensure stability.
 * 
 * @param target_frequency The target frequency in MHz
 * @param asic_type The type of ASIC chip (1366, 1368, 1370)
 * @return bool True if the transition was successful, false otherwise
 */
bool do_frequency_transition(float target_frequency, int asic_type);

#endif // FREQUENCY_TRANSITION_H
