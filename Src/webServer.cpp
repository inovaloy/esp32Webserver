#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"
#include "Arduino.h"
#include "webServer.h"
#include "webServerHelper.h"
#include <cJSON.h>
#include <WiFi.h>
#include <EEPROM.h>


void webHandlerHook(webServerMacro hook)
{
    switch (hook)
    {
    case INDEX_HTML:
        Serial.println("from INDEX_HTML");
        break;

    case HOME_HTML:
        Serial.println("from HOME_HTML");
        break;

    case DASHBOARD_HTML:
        Serial.println("from DASHBOARD_HTML");
        break;

    case LOGIN_HTML:
        Serial.println("from LOGIN_HTML");
        break;

    case LOGOUT_HTML:
        Serial.println("from LOGOUT_HTML");
        break;

    case WIFI_CONFIG_HTML:
        Serial.println("from WIFI_CONFIG_HTML");
        break;

    default:
        break;
    }
}


// Login handler hook
char* apiLoginHandlerHook(httpd_req_t *req) {
    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) {
        // Handle error: failed to get content
        Serial.println("Error: Failed to get content from request");
        return nullptr;
    }
    Serial.println("LOGIN HANDLER SUCCESS! Received data:");
    Serial.println(jsonData);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Login test successful!");
    cJSON_AddStringToObject(response, "received", jsonData);

    char *response_string = cJSON_Print(response);
    cJSON_Delete(response);
    return response_string;
}


// Registration handler hook - receives JSON data from browser
char* apiRegisterHandlerHook(httpd_req_t *req) {
    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) {
        // Handle error: failed to get content
        Serial.println("Error: Failed to get content from request");
        return nullptr;
    }
    Serial.println("Registration attempt received:");
    Serial.println(jsonData);

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON data");
    } else {
        cJSON *username = cJSON_GetObjectItem(json, "username");
        cJSON *password = cJSON_GetObjectItem(json, "password");
        cJSON *email = cJSON_GetObjectItem(json, "email");

        if (cJSON_IsString(username) && cJSON_IsString(password) && cJSON_IsString(email)) {
            String user = String(username->valuestring);
            String pass = String(password->valuestring);
            String userEmail = String(email->valuestring);

            Serial.printf("Registration attempt - Username: %s, Email: %s\n", user.c_str(), userEmail.c_str());

            // Simple registration logic (replace with your own)
            // Here you would typically save to EEPROM, SD card, or external storage
            if (user.length() >= 3 && pass.length() >= 6) {
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Registration successful! You can now login.");

                // In a real implementation, you would save the user data
                Serial.printf("New user registered: %s (%s)\n", user.c_str(), userEmail.c_str());
            } else {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Username must be at least 3 characters and password at least 6 characters");
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Username, password, and email are required");
        }

        cJSON_Delete(json);
    }

    char *response_string = cJSON_Print(response);
    cJSON_Delete(response);
    return response_string;
}

// WiFi status handler - returns current WiFi connection status
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

    char *response_string = cJSON_Print(response);
    cJSON_Delete(response);
    return response_string;
}

// WiFi scan handler - returns list of available networks
char* apiWifiScanHandlerHook(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();

    int networkCount = WiFi.scanNetworks();

    if (networkCount > 0) {
        for (int i = 0; i < networkCount; i++) {
            cJSON *network = cJSON_CreateObject();
            cJSON_AddStringToObject(network, "ssid", WiFi.SSID(i).c_str());
            cJSON_AddNumberToObject(network, "rssi", WiFi.RSSI(i));
            cJSON_AddBoolToObject(network, "encrypted", WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            cJSON_AddItemToArray(networks, network);
        }
    }

    cJSON_AddNumberToObject(response, "count", networkCount);
    cJSON_AddItemToObject(response, "networks", networks);

    char *response_string = cJSON_Print(response);
    cJSON_Delete(response);
    return response_string;
}

// WiFi connect handler - connects to specified network
char* apiWifiConnectHandlerHook(httpd_req_t *req) {
    char* jsonData = getContentFromReq(req);
    if (jsonData == nullptr) {
        Serial.println("Error: Failed to get content from request");
        return nullptr;
    }

    cJSON *json = cJSON_Parse(jsonData);
    cJSON *response = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Invalid JSON data");
    } else {
        cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
        cJSON *password_json = cJSON_GetObjectItem(json, "password");

        if (cJSON_IsString(ssid_json)) {
            String ssid = String(ssid_json->valuestring);
            String password = "";

            if (cJSON_IsString(password_json)) {
                password = String(password_json->valuestring);
            }

            Serial.printf("Attempting to connect to WiFi: %s\n", ssid.c_str());

            // Disconnect current connection
            WiFi.disconnect();
            delay(100);

            // Connect to new network
            WiFi.begin(ssid.c_str(), password.c_str());

            // Wait up to 10 seconds for connection
            unsigned long startTime = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
                delay(500);
            }

            if (WiFi.status() == WL_CONNECTED) {
                // Save credentials to EEPROM
                for (int i = 0; i < 32; i++) {
                    EEPROM.write(i, i < ssid.length() ? ssid[i] : 0);
                }
                for (int i = 0; i < 64; i++) {
                    EEPROM.write(100 + i, i < password.length() ? password[i] : 0);
                }
                EEPROM.commit();

                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Connected successfully");
                cJSON_AddStringToObject(response, "ip", WiFi.localIP().toString().c_str());

                Serial.printf("Successfully connected to %s\n", ssid.c_str());
            } else {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Failed to connect to network");

                Serial.printf("Failed to connect to %s\n", ssid.c_str());
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "SSID is required");
        }

        cJSON_Delete(json);
    }

    char *response_string = cJSON_Print(response);
    cJSON_Delete(response);
    return response_string;
}
