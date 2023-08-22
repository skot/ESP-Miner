#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_
#include <esp_http_server.h>


httpd_handle_t start_webserver(void);

esp_err_t start_rest_server(const char *base_path);

#endif