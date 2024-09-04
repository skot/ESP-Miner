#include <stdbool.h>
#include "global_state.h"

bool NVSDevice_init(void);
bool NVSDevice_parse_config(GlobalState *);
bool NVSDevice_get_wifi_creds(GlobalState *, char **, char **, char **);
