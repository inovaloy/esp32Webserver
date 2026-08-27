#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"
#include "Arduino.h"
#include "webServer.h"
#include "webServerHelper.h"
#include "deviceConfig.h"
#include <cJSON.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <string.h>

// Device state and admin auth owned by the .ino
extern Device  devices[];
extern uint8_t deviceCount;
extern void    saveDevicesToEEPROM();
extern void    updateOledDeviceStatus();
extern char    adminPassword[];
extern void    saveAdminPassword(const char* newPassword);
extern bool    checkAdminPassword(const char* attempt);

// ── Session token (single slot, RAM only — cleared on reboot) ─────────────
#define SESSION_TOKEN_LEN 32
static char sessionToken[SESSION_TOKEN_LEN + 1] = {0};

static void generateToken() {
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < SESSION_TOKEN_LEN; i++)
        sessionToken[i] = hex[esp_random() % 16];
    sessionToken[SESSION_TOKEN_LEN] = '\0';
}

static bool isAuthorised(httpd_req_t *req) {
    if (sessionToken[0] == '\0') return false;
    char buf[SESSION_TOKEN_LEN + 1] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Auth-Token", buf, sizeof(buf)) != ESP_OK)
        return false;
    uint8_t diff = 0;
    for (int i = 0; i < SESSION_TOKEN_LEN; i++)
        diff |= (uint8_t)sessionToken[i] ^ (uint8_t)buf[i];
    return diff == 0;
}

static esp_err_t sendUnauthorised(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Unauthorized\"}");
    return ESP_FAIL;
}


// ── Page hook ─────────────────────────────────────────────────────────────
void webHandlerHook(webServerMacro hook)
{
    switch (hook)
    {
    case BASE_HTML:      Serial.println("Shell requested");            break;
    case LOGIN_HTML:     Serial.println("Login page requested");       break;
    case DASHBOARD_HTML: Serial.println("Dashboard requested");        break;
    case WIFI_CONFIG_HTML: Serial.println("WiFi config requested");    break;
    default: break;
    }
}


// ── Auth API ──────────────────────────────────────────────────────────────

// POST /api/login  { "password": "..." }
char* apiLoginHandlerHook(httpd_req_t *req) {
    char* jsonData = getContentFromReq(req);

    Serial.printf("[LOGIN] content_len=%d body=%s\n",
                  req->content_len, jsonData ? jsonData : "(null)");

    cJSON *json = (jsonData != nullptr) ? cJSON_Parse(jsonData) : NULL;
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *pass_j = cJSON_GetObjectItem(json, "password");
        if (!cJSON_IsString(pass_j)) {
            Serial.printf("[LOGIN] keys in json: ");
            cJSON *item = json->child;
            while (item) { Serial.printf("%s ", item->string); item = item->next; }
            Serial.println();
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "password field required");
        } else {
            char attempt[ADMIN_PASS_LEN + 1] = {0};
            strncpy(attempt, pass_j->valuestring, ADMIN_PASS_LEN);
            if (checkAdminPassword(attempt)) {
                generateToken();
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "token", sessionToken);
            } else {
                delay(500);
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Incorrect password");
            }
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/logout
char* apiLogoutHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    memset(sessionToken, 0, sizeof(sessionToken));
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Logged out");
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/auth/change-password  { "current": "...", "new": "..." }
char* apiAuthChangePasswordHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *cur_j = cJSON_GetObjectItem(json, "current");
        cJSON *new_j = cJSON_GetObjectItem(json, "new");
        if (!cJSON_IsString(cur_j) || !cJSON_IsString(new_j)) {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "current and new fields required");
        } else {
            char attempt[ADMIN_PASS_LEN + 1] = {0};
            strncpy(attempt, cur_j->valuestring, ADMIN_PASS_LEN);
            if (!checkAdminPassword(attempt)) {
                delay(500);
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Current password incorrect");
            } else if (strlen(new_j->valuestring) < 6) {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "New password must be at least 6 characters");
            } else {
                saveAdminPassword(new_j->valuestring);
                memset(sessionToken, 0, sizeof(sessionToken));
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Password updated. Please log in again.");
            }
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}


// ── WiFi API ──────────────────────────────────────────────────────────────

char* apiWifiStatusHandlerHook(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    if (WiFi.status() == WL_CONNECTED) {
        cJSON_AddBoolToObject(response, "connected", true);
        cJSON_AddStringToObject(response, "ssid", WiFi.SSID().c_str());
        cJSON_AddStringToObject(response, "ip", WiFi.localIP().toString().c_str());
        cJSON_AddNumberToObject(response, "rssi", WiFi.RSSI());
    } else {
        cJSON_AddBoolToObject(response, "connected", false);
        cJSON_AddStringToObject(response, "status", "Disconnected");
    }
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

char* apiWifiScanHandlerHook(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        cJSON *net = cJSON_CreateObject();
        cJSON_AddStringToObject(net, "ssid", WiFi.SSID(i).c_str());
        cJSON_AddNumberToObject(net, "rssi", WiFi.RSSI(i));
        cJSON_AddBoolToObject(net, "encrypted", WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(networks, net);
    }
    cJSON_AddNumberToObject(response, "count", n);
    cJSON_AddItemToObject(response, "networks", networks);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

char* apiWifiConnectHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *ssid_j = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass_j = cJSON_GetObjectItem(json, "password");
        if (cJSON_IsString(ssid_j)) {
            String ssid = String(ssid_j->valuestring);
            String pass = cJSON_IsString(pass_j) ? String(pass_j->valuestring) : "";
            WiFi.disconnect();
            delay(100);
            WiFi.begin(ssid.c_str(), pass.c_str());
            unsigned long t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < 10000)
                delay(500);
            if (WiFi.status() == WL_CONNECTED) {
                for (int i = 0; i < 32; i++)
                    EEPROM.write(i, i < (int)ssid.length() ? ssid[i] : 0);
                for (int i = 0; i < 64; i++)
                    EEPROM.write(100 + i, i < (int)pass.length() ? pass[i] : 0);
                EEPROM.write(200, 0xA5);
                EEPROM.commit();
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Connected");
                cJSON_AddStringToObject(response, "ip", WiFi.localIP().toString().c_str());
                updateOledDeviceStatus();
            } else {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Failed to connect");
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "ssid required");
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}


// ── Device API ────────────────────────────────────────────────────────────

// GET /api/devices
char* apiDevicesHandlerHook(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    for (uint8_t i = 0; i < deviceCount; i++) {
        cJSON *d = cJSON_CreateObject();
        cJSON_AddNumberToObject(d, "index", i);
        cJSON_AddStringToObject(d, "name",  devices[i].name);
        cJSON_AddNumberToObject(d, "pin",   devices[i].pin);
        cJSON_AddBoolToObject(d,   "state", devices[i].state == 1);
        cJSON_AddItemToArray(list, d);
    }
    cJSON_AddNumberToObject(response, "count", deviceCount);
    cJSON_AddItemToObject(response, "devices", list);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/devices/add  { "name": "Lamp", "pin": 26 }
char* apiDevicesAddHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else if (deviceCount >= MAX_DEVICES) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Maximum 16 devices reached");
        cJSON_Delete(json);
    } else {
        cJSON *name_j = cJSON_GetObjectItem(json, "name");
        cJSON *pin_j  = cJSON_GetObjectItem(json, "pin");
        if (cJSON_IsString(name_j) && cJSON_IsNumber(pin_j)) {
            uint8_t idx = deviceCount;
            strncpy(devices[idx].name, name_j->valuestring, DEVICE_NAME_LEN - 1);
            devices[idx].name[DEVICE_NAME_LEN - 1] = '\0';
            devices[idx].pin   = (uint8_t)pin_j->valuedouble;
            devices[idx].state = 0;
            deviceCount++;
            pinMode(devices[idx].pin, OUTPUT);
            digitalWrite(devices[idx].pin, LOW);
            saveDevicesToEEPROM();
            updateOledDeviceStatus();
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Device added");
            cJSON_AddNumberToObject(response, "index", idx);
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "name (string) and pin (number) required");
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/devices/remove  { "index": 2 }
char* apiDevicesRemoveHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *idx_j = cJSON_GetObjectItem(json, "index");
        if (cJSON_IsNumber(idx_j)) {
            int idx = (int)idx_j->valuedouble;
            if (idx < 0 || idx >= (int)deviceCount) {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Index out of range");
            } else {
                digitalWrite(devices[idx].pin, LOW);
                for (int i = idx; i < (int)deviceCount - 1; i++)
                    devices[i] = devices[i + 1];
                deviceCount--;
                saveDevicesToEEPROM();
                updateOledDeviceStatus();
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Device removed");
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "index (number) required");
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/devices/toggle  { "index": 0, "state": true }
char* apiDevicesToggleHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *idx_j   = cJSON_GetObjectItem(json, "index");
        cJSON *state_j = cJSON_GetObjectItem(json, "state");
        if (cJSON_IsNumber(idx_j) && (cJSON_IsBool(state_j) || cJSON_IsNumber(state_j))) {
            int idx   = (int)idx_j->valuedouble;
            int state = cJSON_IsTrue(state_j) ? 1 : 0;
            if (idx < 0 || idx >= (int)deviceCount) {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Index out of range");
            } else {
                devices[idx].state = (uint8_t)state;
                digitalWrite(devices[idx].pin, state ? HIGH : LOW);
                saveDevicesToEEPROM();
                updateOledDeviceStatus();
                Serial.printf("Device[%d] '%s' -> %s\n", idx, devices[idx].name, state ? "ON" : "OFF");
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddNumberToObject(response, "index", idx);
                cJSON_AddBoolToObject(response, "state", state == 1);
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "index (number) and state (bool) required");
        }
        cJSON_Delete(json);
    }

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}
