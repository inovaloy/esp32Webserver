#include "esp_http_server.h"
#include "htmlData.h"
#include "Arduino.h"
#include "webServer.h"

httpd_handle_t webServerHttpd = NULL;


static esp_err_t indexHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)indexHtmlGz, indexHtmlGzLen);
}


void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t indexUri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = indexHandler,
        .user_ctx  = NULL
    };

    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        httpd_register_uri_handler(webServerHttpd, &indexUri);
    }
}
