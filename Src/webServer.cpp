#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"
#include "Arduino.h"
#include "webServer.h"
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

// Hooks for API handlers
char* apiTestHandlerHook(){
    cJSON *json = cJSON_CreateObject();
    cJSON *controls = cJSON_CreateObject();

    cJSON_AddBoolToObject(controls, "ledState", 1);
    cJSON_AddBoolToObject(controls, "status", 0);
    cJSON_AddItemToObject(json, "controls", controls);

    char *json_string = cJSON_Print(json);
    Serial.println(json_string);
    cJSON_Delete(json);
    return json_string;
}


char* apiServerHandlerHook(){
    Serial.println("from API Server Handler Hook");
    return nullptr;
}
