/*
* This is a autogen file; Do not change manually
*/

#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "Arduino.h"
#include "webServer.h"
#include "AutoGen/autoGenWebServer.h"
#include "dynamicData.h"
#include <cJSON.h>

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
    return sendLargeResponse(req, (const char *)dashboard_dynamicHtmlGz, dashboard_dynamicHtmlGzLen);
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

// API Endpoints for dynamic data
static esp_err_t apiSystemHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *json_data = getSystemInfoJson();
    esp_err_t result = httpd_resp_sendstr(req, json_data);
    free(json_data);
    return result;
}

static esp_err_t apiSensorsHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *json_data = getSensorDataJson();
    esp_err_t result = httpd_resp_sendstr(req, json_data);
    free(json_data);
    return result;
}

static esp_err_t apiNetworkHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *json_data = getNetworkInfoJson();
    esp_err_t result = httpd_resp_sendstr(req, json_data);
    free(json_data);
    return result;
}

static esp_err_t apiControlHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (req->method == HTTP_GET) {
        // Return current control status
        char *json_data = getControlStatusJson();
        esp_err_t result = httpd_resp_sendstr(req, json_data);
        free(json_data);
        return result;
    } else if (req->method == HTTP_POST) {
        // Handle control commands
        char buf[100];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
            return ESP_FAIL;
        }
        buf[ret] = '\0';

        // Parse JSON command
        cJSON *json = cJSON_Parse(buf);
        if (json == NULL) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }

        cJSON *device = cJSON_GetObjectItem(json, "device");
        cJSON *action = cJSON_GetObjectItem(json, "action");

        if (device && action && cJSON_IsString(device) && cJSON_IsString(action)) {
            bool success = handleDeviceControl(device->valuestring, action->valuestring);

            cJSON *response = cJSON_CreateObject();
            cJSON_AddBoolToObject(response, "success", success);
            cJSON_AddStringToObject(response, "message", success ? "Command executed" : "Command failed");

            char *response_str = cJSON_Print(response);
            esp_err_t result = httpd_resp_sendstr(req, response_str);

            free(response_str);
            cJSON_Delete(response);
            cJSON_Delete(json);
            return result;
        }

        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device or action");
        return ESP_FAIL;
    }

    return ESP_FAIL;
}

// Server-Sent Events endpoint
static esp_err_t apiEventsHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    // Send initial connection event
    httpd_resp_sendstr_chunk(req, "event: connected\n");
    httpd_resp_sendstr_chunk(req, "data: Connection established\n\n");

    // Keep connection alive and send periodic updates
    for (int i = 0; i < 1200; i++) { // Run for ~10 minutes (1200 * 0.5s)
        // Send system data every 2 seconds (i % 4 == 0)
        if (i % 4 == 0) {
            char *system_json = getSystemInfoJson();
            httpd_resp_sendstr_chunk(req, "event: system\n");
            httpd_resp_sendstr_chunk(req, "data: ");
            httpd_resp_sendstr_chunk(req, system_json);
            httpd_resp_sendstr_chunk(req, "\n\n");
            free(system_json);
        }

        // Send sensor data every 1 second (i % 2 == 0)
        if (i % 2 == 0) {
            char *sensor_json = getSensorDataJson();
            httpd_resp_sendstr_chunk(req, "event: sensors\n");
            httpd_resp_sendstr_chunk(req, "data: ");
            httpd_resp_sendstr_chunk(req, sensor_json);
            httpd_resp_sendstr_chunk(req, "\n\n");
            free(sensor_json);
        }

        delay(500); // 500ms delay between iterations

        // Check if client is still connected
        if (httpd_req_async_handler_complete(req) != ESP_OK) {
            break;
        }
    }

    // End the stream
    return httpd_resp_send_chunk(req, NULL, 0);
}

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase limits for larger responses
    config.max_resp_headers = 16;     // Increase max response headers
    config.stack_size = 8192;         // Increase stack size (default is 4096)
    config.task_priority = 5;         // Set task priority
    config.core_id = tskNO_AFFINITY;  // Allow task to run on any core
    config.max_open_sockets = 7;      // Increase max open sockets if needed

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

    // API endpoints
    httpd_uri_t uri_api_system = {
        .uri       = "/api/system",
        .method    = HTTP_GET,
        .handler   = apiSystemHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_sensors = {
        .uri       = "/api/sensors",
        .method    = HTTP_GET,
        .handler   = apiSensorsHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_network = {
        .uri       = "/api/network",
        .method    = HTTP_GET,
        .handler   = apiNetworkHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_control_get = {
        .uri       = "/api/control",
        .method    = HTTP_GET,
        .handler   = apiControlHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_control_post = {
        .uri       = "/api/control",
        .method    = HTTP_POST,
        .handler   = apiControlHandler,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_events = {
        .uri       = "/api/events",
        .method    = HTTP_GET,
        .handler   = apiEventsHandler,
        .user_ctx  = NULL
    };

    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        // Register static page handlers
        httpd_register_uri_handler(webServerHttpd, &uri_dshbrd);
        httpd_register_uri_handler(webServerHttpd, &uri_home);
        httpd_register_uri_handler(webServerHttpd, &uri_);
        httpd_register_uri_handler(webServerHttpd, &uri_index);
        httpd_register_uri_handler(webServerHttpd, &uri_login);
        httpd_register_uri_handler(webServerHttpd, &uri_logout);

        // Register API handlers
        httpd_register_uri_handler(webServerHttpd, &uri_api_system);
        httpd_register_uri_handler(webServerHttpd, &uri_api_sensors);
        httpd_register_uri_handler(webServerHttpd, &uri_api_network);
        httpd_register_uri_handler(webServerHttpd, &uri_api_control_get);
        httpd_register_uri_handler(webServerHttpd, &uri_api_control_post);
        httpd_register_uri_handler(webServerHttpd, &uri_api_events);
    }
}
