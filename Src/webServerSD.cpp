/*
* Web Server with SD Card support
* This version can serve HTML files from SD card or fall back to compiled versions
*/

#include "esp_http_server.h"
#include "Arduino.h"
#include "webServer.h"
#include "webServerHelper.h"
#include "sdCardHandler.h"
#include "webServerSD.h"

// Import compiled HTML data as fallback
#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"

httpd_handle_t webServerHttpdSD = NULL;
bool useSDCardForHTML = true;

// SD card paths for HTML files
#define SD_HTML_PATH "/html/"

// Generic HTML handler that tries SD card first, then falls back to compiled
static esp_err_t genericHtmlHandler(httpd_req_t *req, const char* filename,
                                     const uint8_t* fallbackData, size_t fallbackLen,
                                     webServerMacro hookMacro) {
    httpd_resp_set_type(req, "text/html");

    // Call the hook
    webHandlerHook(hookMacro);

    if (useSDCardForHTML) {
        // Build SD card path
        char sdPath[128];
        snprintf(sdPath, sizeof(sdPath), "%s%s", SD_HTML_PATH, filename);

        // Try to read from SD card
        size_t fileSize = 0;
        char* fileContent = readFileFromSD(sdPath, &fileSize);

        if (fileContent) {
            Serial.printf("Serving %s from SD card\n", filename);
            esp_err_t result = sendLargeResponse(req, fileContent, fileSize);
            free(fileContent);
            return result;
        } else {
            Serial.printf("Failed to read %s from SD card, using fallback\n", filename);
        }
    }

    // Fallback to compiled HTML (gzipped)
    Serial.printf("Serving %s from compiled data\n", filename);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return sendLargeResponse(req, (const char *)fallbackData, fallbackLen);
}

// Individual handlers for each HTML page
static esp_err_t dashboardHandlerSD(httpd_req_t *req) {
    return genericHtmlHandler(req, "dashboard.html",
                            dashboardHtmlGz, dashboardHtmlGzLen,
                            DASHBOARD_HTML);
}

static esp_err_t homeHandlerSD(httpd_req_t *req) {
    return genericHtmlHandler(req, "home.html",
                            homeHtmlGz, homeHtmlGzLen,
                            HOME_HTML);
}

static esp_err_t indexHandlerSD(httpd_req_t *req) {
    return genericHtmlHandler(req, "index.html",
                            indexHtmlGz, indexHtmlGzLen,
                            INDEX_HTML);
}

static esp_err_t loginHandlerSD(httpd_req_t *req) {
    return genericHtmlHandler(req, "login.html",
                            loginHtmlGz, loginHtmlGzLen,
                            LOGIN_HTML);
}

static esp_err_t logoutHandlerSD(httpd_req_t *req) {
    return genericHtmlHandler(req, "logout.html",
                            logoutHtmlGz, logoutHtmlGzLen,
                            LOGOUT_HTML);
}

// API handlers remain the same
static esp_err_t apiLoginHandlerSD(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *jsonData = apiLoginHandlerHook(req);
    esp_err_t result = httpd_resp_sendstr(req, jsonData);
    free(jsonData);
    return result;
}

static esp_err_t apiRegisterHandlerSD(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *jsonData = apiRegisterHandlerHook(req);
    esp_err_t result = httpd_resp_sendstr(req, jsonData);
    free(jsonData);
    return result;
}

void startWebServerWithSD(bool enableSD, int sdCardPin) {
    useSDCardForHTML = enableSD;

    // Initialize SD card if enabled
    if (enableSD) {
        if (initSDCard(sdCardPin)) {
            Serial.println("SD Card initialized successfully");
            Serial.println("Listing HTML files on SD card:");
            listSDDir(SD_HTML_PATH, 0);

            // Verify HTML files exist
            const char* requiredFiles[] = {
                "index.html", "home.html", "dashboard.html",
                "login.html", "logout.html"
            };

            bool allFilesFound = true;
            for (int i = 0; i < 5; i++) {
                char path[128];
                snprintf(path, sizeof(path), "%s%s", SD_HTML_PATH, requiredFiles[i]);
                if (!fileExistsOnSD(path)) {
                    Serial.printf("Warning: %s not found on SD card\n", requiredFiles[i]);
                    allFilesFound = false;
                }
            }

            if (!allFilesFound) {
                Serial.println("Warning: Some HTML files missing, will use compiled fallback");
            }
        } else {
            Serial.println("SD Card initialization failed, using compiled HTML only");
            useSDCardForHTML = false;
        }
    } else {
        Serial.println("SD Card disabled, using compiled HTML");
    }

    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_resp_headers = 16;
    config.stack_size       = 8192;
    config.task_priority    = 5;
    config.core_id          = tskNO_AFFINITY;
    config.max_open_sockets = 7;

    // HTML File Handlers and URIs
    httpd_uri_t uri_dshbrd = {
        .uri       = "/dshbrd",
        .method    = HTTP_GET,
        .handler   = dashboardHandlerSD,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_home = {
        .uri       = "/home",
        .method    = HTTP_GET,
        .handler   = homeHandlerSD,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_ = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = indexHandlerSD,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_login = {
        .uri       = "/login",
        .method    = HTTP_GET,
        .handler   = loginHandlerSD,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_logout = {
        .uri       = "/logout",
        .method    = HTTP_GET,
        .handler   = logoutHandlerSD,
        .user_ctx  = NULL
    };

    // API handlers
    httpd_uri_t uri_api_login = {
        .uri       = "/api/login",
        .method    = HTTP_POST,
        .handler   = apiLoginHandlerSD,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_api_register = {
        .uri       = "/api/register",
        .method    = HTTP_POST,
        .handler   = apiRegisterHandlerSD,
        .user_ctx  = NULL
    };

    // Start server
    if (httpd_start(&webServerHttpdSD, &config) == ESP_OK) {
        httpd_register_uri_handler(webServerHttpdSD, &uri_dshbrd);
        httpd_register_uri_handler(webServerHttpdSD, &uri_home);
        httpd_register_uri_handler(webServerHttpdSD, &uri_);
        httpd_register_uri_handler(webServerHttpdSD, &uri_login);
        httpd_register_uri_handler(webServerHttpdSD, &uri_logout);
        httpd_register_uri_handler(webServerHttpdSD, &uri_api_login);
        httpd_register_uri_handler(webServerHttpdSD, &uri_api_register);

        Serial.println("Web server started successfully");
        if (useSDCardForHTML) {
            Serial.println("Mode: SD Card with compiled fallback");
        } else {
            Serial.println("Mode: Compiled HTML only");
        }
    } else {
        Serial.println("Error starting web server!");
    }
}
