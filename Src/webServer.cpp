#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"
#include "Arduino.h"
#include "webServer.h"
#include "webServerHelper.h"
#include <cJSON.h>


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
