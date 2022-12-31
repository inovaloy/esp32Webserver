/*
* This is a autogen file; Do not change manually
*/

#include "esp_http_server.h"
#include "AutoGenHtmlData.h"
#include "Arduino.h"
#include "webServer.h"
#include "AutoGenWebServer.h"

httpd_handle_t webServerHttpd = NULL;

static esp_err_t dashboardHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook(DASHBOARD_HTML);
    return httpd_resp_send(req, (const char *)dashboardHtmlGz, dashboardHtmlGzLen);
}

static esp_err_t homeHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook(HOME_HTML);
    return httpd_resp_send(req, (const char *)homeHtmlGz, homeHtmlGzLen);
}

static esp_err_t indexHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook(INDEX_HTML);
    return httpd_resp_send(req, (const char *)indexHtmlGz, indexHtmlGzLen);
}

static esp_err_t loginHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook(LOGIN_HTML);
    return httpd_resp_send(req, (const char *)loginHtmlGz, loginHtmlGzLen);
}

static esp_err_t logoutHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook(LOGOUT_HTML);
    return httpd_resp_send(req, (const char *)logoutHtmlGz, logoutHtmlGzLen);
}

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        httpd_uri_t uri_dshbrd = {
        .uri       = "/dshbrd",
        .method    = HTTP_GET,
        .handler   = dashboardHandler,
        .user_ctx  = NULL
    };

        httpd_uri_t uri_home = {
        .uri       = "/home",
        .method    = HTTP_GET,
        .handler   = homeHandler,
        .user_ctx  = NULL
    };

        httpd_uri_t uri_ = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = indexHandler,
        .user_ctx  = NULL
    };

        httpd_uri_t uri_index = {
        .uri       = "/index",
        .method    = HTTP_GET,
        .handler   = indexHandler,
        .user_ctx  = NULL
    };

        httpd_uri_t uri_login = {
        .uri       = "/login",
        .method    = HTTP_GET,
        .handler   = loginHandler,
        .user_ctx  = NULL
    };

        httpd_uri_t uri_logout = {
        .uri       = "/logout",
        .method    = HTTP_GET,
        .handler   = logoutHandler,
        .user_ctx  = NULL
    };

    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        httpd_register_uri_handler(webServerHttpd, &uri_dshbrd);
        httpd_register_uri_handler(webServerHttpd, &uri_home);
        httpd_register_uri_handler(webServerHttpd, &uri_);
        httpd_register_uri_handler(webServerHttpd, &uri_index);
        httpd_register_uri_handler(webServerHttpd, &uri_login);
        httpd_register_uri_handler(webServerHttpd, &uri_logout);
    }
}
