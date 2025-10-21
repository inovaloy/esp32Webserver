#ifndef WEB_SERVER_HELPER_H
#define WEB_SERVER_HELPER_H

#include <stddef.h>
#include "esp_http_server.h"

esp_err_t sendLargeResponse(httpd_req_t *req, const char* data, size_t data_len);
char* getContentFromReq(httpd_req_t *req);

// New function to serve files from SD card
esp_err_t serveFileFromSD(httpd_req_t *req, const char* filepath, const char* contentType);

#endif // WEB_SERVER_HELPER_H
