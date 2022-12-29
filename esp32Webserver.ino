#include <WiFi.h>
#include "webServer.h"

void setup()
{
    const char* ssid     = "*************";
    const char* password = "*************";

    Serial.begin(115200);
    Serial.println();

    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    startWebServer();

    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
}

void loop()
{
    delay(10000);
}
