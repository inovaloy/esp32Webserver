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
extern Device       devices[];
extern uint8_t      deviceCount;
extern void         saveDevicesToEEPROM();
extern void         updateOledDeviceStatus();
extern char         adminPassword[];
extern void         saveAdminPassword(const char* newPassword, bool markConfigured);
extern bool         adminPasswordChangeRequired;
extern bool         checkAdminPassword(const char* attempt);
extern volatile bool eepromDirty;   // committed from main task loop()
extern char         controllerName[];
extern uint16_t     logoutMinutes;
extern void         saveControllerSettings(const char* name, uint16_t minutes);
extern uint8_t       oledBrightness;
extern bool          oledEnabled;
extern void          saveOledSettings(uint8_t brightness, bool enabled);
extern void          factoryResetSettings();
extern void          scheduleReboot();
extern uint8_t        storageRead(int address);
extern void           storageWrite(int address, uint8_t value);

// ── Session token (single slot, RAM only — cleared on reboot) ─────────────
#define SESSION_TOKEN_LEN 32
#define SESSION_INACTIVITY_MS ((unsigned long)logoutMinutes * 60UL * 1000UL)
static char sessionToken[SESSION_TOKEN_LEN + 1] = {0};
static unsigned long sessionLastActivity = 0;

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
    if (diff != 0) return false;
    if (millis() - sessionLastActivity >= SESSION_INACTIVITY_MS) {
        memset(sessionToken, 0, sizeof(sessionToken));
        sessionLastActivity = 0;
        return false;
    }
    sessionLastActivity = millis();
    return true;
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
    cJSON *json = (jsonData != nullptr) ? cJSON_Parse(jsonData) : NULL;
    free(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else {
        cJSON *pass_j = cJSON_GetObjectItem(json, "password");
        if (!cJSON_IsString(pass_j)) {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "password field required");
        } else {
            char attempt[ADMIN_PASS_LEN + 1] = {0};
            strncpy(attempt, pass_j->valuestring, ADMIN_PASS_LEN);
            if (checkAdminPassword(attempt)) {
                generateToken();
                sessionLastActivity = millis();
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "token", sessionToken);
                cJSON_AddBoolToObject(response, "mustChangePassword", adminPasswordChangeRequired);
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
    sessionLastActivity = 0;
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
    free(jsonData);
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
                saveAdminPassword(new_j->valuestring, true);
                memset(sessionToken, 0, sizeof(sessionToken));
                sessionLastActivity = 0;
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

char* apiAuthSetInitialPasswordHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    cJSON *json = jsonData ? cJSON_Parse(jsonData) : NULL;
    free(jsonData);
    cJSON *response = cJSON_CreateObject();
    cJSON *password = json ? cJSON_GetObjectItem(json, "password") : NULL;

    if (!adminPasswordChangeRequired) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Initial password is already configured");
    } else if (!cJSON_IsString(password) || strlen(password->valuestring) < 6 ||
               strlen(password->valuestring) > ADMIN_PASS_LEN) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Password must be 6 to 32 characters");
    } else {
        saveAdminPassword(password->valuestring, true);
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Admin password configured");
    }
    if (json) cJSON_Delete(json);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// GET /api/settings
char* apiSettingsHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "controllerName", controllerName);
    cJSON_AddNumberToObject(response, "logoutMinutes", logoutMinutes);
    cJSON_AddNumberToObject(response, "oledBrightness", oledBrightness);
    cJSON_AddBoolToObject(response, "oledEnabled", oledEnabled);
    cJSON_AddBoolToObject(response, "mustChangePassword", adminPasswordChangeRequired);
    cJSON_AddStringToObject(response, "firmwareVersion", "1.0.1");
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/settings/save { "controllerName": "Home Controller", "logoutMinutes": 15, "oledBrightness": 100, "oledEnabled": true }
char* apiSettingsSaveHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    char *jsonData = getContentFromReq(req);
    cJSON *json = jsonData ? cJSON_Parse(jsonData) : NULL;
    free(jsonData);
    cJSON *response = cJSON_CreateObject();
    cJSON *name_j = json ? cJSON_GetObjectItem(json, "controllerName") : NULL;
    cJSON *minutes_j = json ? cJSON_GetObjectItem(json, "logoutMinutes") : NULL;
    cJSON *brightness_j = json ? cJSON_GetObjectItem(json, "oledBrightness") : NULL;
    cJSON *enabled_j = json ? cJSON_GetObjectItem(json, "oledEnabled") : NULL;
    int minutes = minutes_j && cJSON_IsNumber(minutes_j) ? minutes_j->valueint : 0;
    int brightness = brightness_j && cJSON_IsNumber(brightness_j) ? brightness_j->valueint : -1;
    if (!json || !cJSON_IsString(name_j) || strlen(name_j->valuestring) == 0 ||
        strlen(name_j->valuestring) >= CONTROLLER_NAME_LEN || minutes < 1 || minutes > 1440 ||
        brightness < 1 || brightness > 255 || !cJSON_IsBool(enabled_j)) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Enter a controller name and logout time from 1 to 1440 minutes");
    } else {
        saveControllerSettings(name_j->valuestring, (uint16_t)minutes);
        saveOledSettings((uint8_t)brightness, cJSON_IsTrue(enabled_j));
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Settings saved");
    }
    if (json) cJSON_Delete(json);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

char* apiSettingsBackupHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    cJSON *response = cJSON_CreateObject();
    cJSON *devicesJson = cJSON_CreateArray();
    cJSON_AddStringToObject(response, "controllerName", controllerName);
    cJSON_AddNumberToObject(response, "logoutMinutes", logoutMinutes);
    cJSON_AddNumberToObject(response, "oledBrightness", oledBrightness);
    cJSON_AddBoolToObject(response, "oledEnabled", oledEnabled);
    for (uint8_t i = 0; i < deviceCount; i++) {
        cJSON *device = cJSON_CreateObject();
        cJSON_AddStringToObject(device, "name", devices[i].name);
        cJSON_AddNumberToObject(device, "pin", devices[i].pin);
        cJSON_AddBoolToObject(device, "state", devices[i].state != 0);
        cJSON_AddItemToArray(devicesJson, device);
    }
    cJSON_AddItemToObject(response, "devices", devicesJson);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

char* apiSettingsRestoreHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    char *jsonData = getContentFromReq(req);
    cJSON *json = jsonData ? cJSON_Parse(jsonData) : NULL;
    free(jsonData);
    cJSON *response = cJSON_CreateObject();
    cJSON *devicesJson = json ? cJSON_GetObjectItem(json, "devices") : NULL;
    bool invalidDevice = false;
    if (cJSON_IsArray(devicesJson)) {
        cJSON *item;
        cJSON_ArrayForEach(item, devicesJson) {
            cJSON *pin = cJSON_GetObjectItem(item, "pin");
            bool allowed = false;
            if (cJSON_IsNumber(pin)) {
                for (int i = 0; i < CFG_HIGH_VOLTAGE_PIN_COUNT; i++)
                    allowed |= CFG_HIGH_VOLTAGE_PINS[i] == pin->valueint;
                for (int i = 0; i < CFG_LOW_VOLTAGE_PIN_COUNT; i++)
                    allowed |= CFG_LOW_VOLTAGE_PINS[i] == pin->valueint;
            }
            if (!allowed) { invalidDevice = true; break; }
        }
    }
    if (!json || !cJSON_IsArray(devicesJson) || cJSON_GetArraySize(devicesJson) > MAX_DEVICES || invalidDevice) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid backup file");
    } else {
        cJSON *name = cJSON_GetObjectItem(json, "controllerName");
        cJSON *minutes = cJSON_GetObjectItem(json, "logoutMinutes");
        cJSON *brightness = cJSON_GetObjectItem(json, "oledBrightness");
        cJSON *enabled = cJSON_GetObjectItem(json, "oledEnabled");
        if (!cJSON_IsString(name) || !cJSON_IsNumber(minutes) || !cJSON_IsNumber(brightness) ||
            !cJSON_IsBool(enabled) || strlen(name->valuestring) == 0 ||
            strlen(name->valuestring) >= CONTROLLER_NAME_LEN || minutes->valueint < 1 || minutes->valueint > 1440 ||
            brightness->valueint < 1 || brightness->valueint > 255) {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Invalid backup settings");
        } else {
            saveControllerSettings(name->valuestring, (uint16_t)minutes->valueint);
            saveOledSettings((uint8_t)brightness->valueint, cJSON_IsTrue(enabled));
            deviceCount = 0;
            cJSON *item;
            cJSON_ArrayForEach(item, devicesJson) {
                cJSON *itemName = cJSON_GetObjectItem(item, "name");
                cJSON *pin = cJSON_GetObjectItem(item, "pin");
                cJSON *state = cJSON_GetObjectItem(item, "state");
                if (!cJSON_IsString(itemName) || !cJSON_IsNumber(pin) || !cJSON_IsBool(state)) continue;
                strncpy(devices[deviceCount].name, itemName->valuestring, DEVICE_NAME_LEN - 1);
                devices[deviceCount].name[DEVICE_NAME_LEN - 1] = '\0';
                devices[deviceCount].pin = (uint8_t)pin->valueint;
                devices[deviceCount].state = cJSON_IsTrue(state) ? 1 : 0;
                pinMode(devices[deviceCount].pin, OUTPUT);
                digitalWrite(devices[deviceCount].pin, devices[deviceCount].state ? HIGH : LOW);
                deviceCount++;
            }
            saveDevicesToEEPROM();
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Backup restored; rebooting");
            scheduleReboot();
        }
    }
    if (json) cJSON_Delete(json);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

char* apiSettingsFactoryResetHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    char* jsonData = getContentFromReq(req);
    cJSON *json = jsonData ? cJSON_Parse(jsonData) : NULL;
    free(jsonData);
    cJSON *response = cJSON_CreateObject();
    cJSON *password = json ? cJSON_GetObjectItem(json, "password") : NULL;
    char attempt[ADMIN_PASS_LEN + 1] = {0};

    if (!cJSON_IsString(password)) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Admin password required");
    } else {
        strncpy(attempt, password->valuestring, ADMIN_PASS_LEN);
        if (!checkAdminPassword(attempt)) {
            delay(500);
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Incorrect admin password");
        } else {
            factoryResetSettings();
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Factory reset; rebooting");
            scheduleReboot();
        }
    }
    if (json) cJSON_Delete(json);
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
    free(jsonData);
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
                    storageWrite(i, i < (int)ssid.length() ? ssid[i] : 0);
                for (int i = 0; i < 64; i++)
                    storageWrite(100 + i, i < (int)pass.length() ? pass[i] : 0);
                storageWrite(200, 0xA5);
                eepromDirty = true;  // committed from loop() in the main task
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

// GET /api/device-config — returns available pins grouped by voltage
char* apiDeviceConfigHandlerHook(httpd_req_t *req) {
    bool inUse[256] = {false};
    for (uint8_t i = 0; i < deviceCount; i++)
        inUse[devices[i].pin] = true;

    cJSON *response  = cJSON_CreateObject();
    cJSON *highVoltagePins = cJSON_CreateArray();
    cJSON *lowVoltagePins = cJSON_CreateArray();
    cJSON *availableHigh = cJSON_CreateArray();
    cJSON *availableLow = cJSON_CreateArray();

    for (int i = 0; i < CFG_HIGH_VOLTAGE_PIN_COUNT; i++) {
        uint8_t pin = CFG_HIGH_VOLTAGE_PINS[i];
        cJSON_AddItemToArray(highVoltagePins, cJSON_CreateNumber(pin));
        if (!inUse[pin])
            cJSON_AddItemToArray(availableHigh, cJSON_CreateNumber(pin));
    }
    for (int i = 0; i < CFG_LOW_VOLTAGE_PIN_COUNT; i++) {
        uint8_t pin = CFG_LOW_VOLTAGE_PINS[i];
        cJSON_AddItemToArray(lowVoltagePins, cJSON_CreateNumber(pin));
        if (!inUse[pin])
            cJSON_AddItemToArray(availableLow, cJSON_CreateNumber(pin));
    }

    cJSON_AddNumberToObject(response, "maxDevices",     CFG_MAX_DEVICES);
    cJSON_AddNumberToObject(response, "currentDevices", deviceCount);
    cJSON_AddItemToObject(response,   "highVoltagePins", highVoltagePins);
    cJSON_AddItemToObject(response,   "lowVoltagePins",  lowVoltagePins);
    cJSON_AddItemToObject(response,   "availableHighVoltagePins", availableHigh);
    cJSON_AddItemToObject(response,   "availableLowVoltagePins",  availableLow);

    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}

// POST /api/reboot — send response then reboot after a short delay
char* apiRebootHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Rebooting...");
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    // Schedule reboot after 500 ms so the HTTP response can be sent first
    // Using a one-shot timer via delay — httpd task will send the response
    // before ESP.restart() is called because the handler returns first.
    extern void scheduleReboot();
    scheduleReboot();
    return out;
}

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

// POST /api/devices/add  { "name": "Lamp", "pin": 26, "voltage": "high" }
char* apiDevicesAddHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }

    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) return nullptr;

    cJSON *json = cJSON_Parse(jsonData);
    free(jsonData);
    cJSON *response = cJSON_CreateObject();
    bool inUse[256] = {false};
    for (uint8_t i = 0; i < deviceCount; i++)
        inUse[devices[i].pin] = true;

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON");
    } else if (deviceCount >= MAX_DEVICES) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Maximum device limit reached");
        cJSON_Delete(json);
    } else {
        cJSON *name_j = cJSON_GetObjectItem(json, "name");
        cJSON *pin_j  = cJSON_GetObjectItem(json, "pin");
        cJSON *voltage_j = cJSON_GetObjectItem(json, "voltage");
        bool highVoltage = cJSON_IsString(voltage_j) && strcmp(voltage_j->valuestring, "high") == 0;
        bool lowVoltage = cJSON_IsString(voltage_j) && strcmp(voltage_j->valuestring, "low") == 0;
        bool allowedPin = false;
        if (cJSON_IsNumber(pin_j)) {
            const uint8_t pin = (uint8_t)pin_j->valuedouble;
            const uint8_t *allowedPins = highVoltage ? CFG_HIGH_VOLTAGE_PINS : CFG_LOW_VOLTAGE_PINS;
            const uint8_t allowedPinCount = highVoltage ? CFG_HIGH_VOLTAGE_PIN_COUNT : CFG_LOW_VOLTAGE_PIN_COUNT;
            for (uint8_t i = 0; i < allowedPinCount; i++) {
                if (allowedPins[i] == pin && !inUse[pin]) { allowedPin = true; break; }
            }
        }
        if (cJSON_IsString(name_j) && cJSON_IsNumber(pin_j) && (highVoltage || lowVoltage) && allowedPin) {
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
            cJSON_AddStringToObject(response, "message", "name, voltage (high/low), and an available GPIO pin are required");
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
    free(jsonData);
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
    free(jsonData);
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
                Serial.printf("Device[%d] '%s' -> %s\r\n", idx, devices[idx].name, state ? "ON" : "OFF");
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
