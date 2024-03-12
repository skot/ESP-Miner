#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t start_rest_server(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif