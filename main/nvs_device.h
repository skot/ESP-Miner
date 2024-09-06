#include <stdbool.h>
#include "esp_err.h"
#include "global_state.h"

esp_err_t NVSDevice_init(void);
esp_err_t NVSDevice_parse_config(GlobalState *);
esp_err_t NVSDevice_get_wifi_creds(GlobalState *, char **, char **, char **);
