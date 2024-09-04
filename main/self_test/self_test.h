#ifndef SELF_TEST_H_
#define SELF_TEST_H_

#include "global_state.h"

void self_test(void * pvParameters);
bool should_test(GlobalState *);

#endif