/*
* This is a autogen file; Do not change manually
*/

#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "Arduino.h"
#include "webServer.h"
#include "AutoGen/autoGenWebServer.h"

httpd_handle_t webServerHttpd = NULL;

// Helper function to send large content in chunks
static esp_err_t sendLargeResponse(httpd_req_t *req, const char* data, size_t data_len) {
    const size_t chunk_size = 1024; // Send in 1KB chunks
    size_t remaining = data_len;

    while (remaining > 0) {
        size_t to_send = (remaining > chunk_size) ? chunk_size : remaining;
        if (httpd_resp_send_chunk(req, data, to_send) != ESP_OK) {
            return ESP_FAIL;
        }
        data += to_send;
        remaining -= to_send;
    }

    // Send empty chunk to signal end
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t dashboardHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook(DASHBOARD_HTML);
    return sendLargeResponse(req, (const char *)dashboardHtmlGz, dashboardHtmlGzLen);
}

static esp_err_t homeHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook(HOME_HTML);
    return sendLargeResponse(req, (const char *)homeHtmlGz, homeHtmlGzLen);
}

static esp_err_t indexHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook(INDEX_HTML);
    return sendLargeResponse(req, (const char *)indexHtmlGz, indexHtmlGzLen);
}

static esp_err_t loginHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook(LOGIN_HTML);
    return sendLargeResponse(req, (const char *)loginHtmlGz, loginHtmlGzLen);
}

static esp_err_t logoutHandler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook(LOGOUT_HTML);
    return sendLargeResponse(req, (const char *)logoutHtmlGz, logoutHtmlGzLen);
}

static esp_err_t apiLoginHandler(httpd_req_t *req) {
    Serial.println("from apiLoginHandler");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // // Get content length
    // size_t content_length = req->content_len;
    // if (content_length <= 0) {
    //     httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
    //     return ESP_FAIL;
    // }

    // // Allocate buffer for the content
    // char *content = (char*)malloc(content_length + 1);
    // if (!content) {
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    //     return ESP_FAIL;
    // }

    // // Read the content
    // size_t received = 0;
    // int ret = httpd_req_recv(req, content, content_length);
    // if (ret <= 0) {
    //     free(content);
    //     if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
    //         httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
    //     } else {
    //         httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request");
    //     }
    //     return ESP_FAIL;
    // }

    content[ret] = '\0';  // Null terminate the string

    // Call the hook with the received data
    char *json_data = apiLoginHandlerHook(req);
    esp_err_t result = httpd_resp_sendstr(req, json_data);

    free(content);
    free(json_data);
    return result;
}

static esp_err_t apiRegisterHandler(httpd_req_t *req) {
    Serial.println("from apiRegisterHandler");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Get content length
    size_t content_length = req->content_len;
    if (content_length <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
        return ESP_FAIL;
    }

    // Allocate buffer for the content
    char *content = (char*)malloc(content_length + 1);
    if (!content) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Read the content
    size_t received = 0;
    int ret = httpd_req_recv(req, content, content_length);
    if (ret <= 0) {
        free(content);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request");
        }
        return ESP_FAIL;
    }

    content[ret] = '\0';  // Null terminate the string

    // Call the hook with the received data
    char *json_data = apiRegisterHandlerHook(content);
    esp_err_t result = httpd_resp_sendstr(req, json_data);

    free(content);
    free(json_data);
    return result;
}

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase limits for larger responses
    config.max_resp_headers = 16;              // Increase max response headers
    config.stack_size       = 8192;            // Increase stack size (default is 4096)
    config.task_priority    = 5;               // Set task priority
    config.core_id          = tskNO_AFFINITY;  // Allow task to run on any core
    config.max_open_sockets = 7;               // Increase max open sockets if needed

    // HTML File Handlers and URIs
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

    // API Handlers and URIs
    httpd_uri_t uri_api_login = {
        .uri       = "/api/login",
        .method    = HTTP_POST,
        .handler   = apiLoginHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_register = {
        .uri       = "/api/register",
        .method    = HTTP_POST,
        .handler   = apiRegisterHandler,
        .user_ctx  = NULL
    };

    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        Serial.println("Web server started successfully!");

        // Register static page handlers
        httpd_register_uri_handler(webServerHttpd, &uri_dshbrd);
        httpd_register_uri_handler(webServerHttpd, &uri_home);
        httpd_register_uri_handler(webServerHttpd, &uri_);
        httpd_register_uri_handler(webServerHttpd, &uri_index);
        httpd_register_uri_handler(webServerHttpd, &uri_login);
        httpd_register_uri_handler(webServerHttpd, &uri_logout);

        // Register API handlers
        httpd_register_uri_handler(webServerHttpd, &uri_api_login);
        httpd_register_uri_handler(webServerHttpd, &uri_api_register);
    } else {
        Serial.println("Failed to start web server!");
    }
}
