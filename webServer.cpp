#include "esp_http_server.h"
#include "AutoGenHtmlData.h"
#include "Arduino.h"
#include "AutoGenWebServer.h"
#include "webServer.h"


void webHandelerHook(webServerMacro hook)
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
