#include <WiFi.h>
#include "webServer.h"
#include "httpClinet.h"
#include "LiquidCrystal_I2C.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
int counter = 0;

void setup()
{
    const char* ssid     = "*************";
    const char* password = "*************";

    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);
    lcd.begin(14, 15); // sda, scl

	// Turn on the blacklight and print a message.
	lcd.backlight();
	lcd.print("Connecting...");

    WiFi.begin(ssid, password);
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        ledStatus = !ledStatus;
        digitalWrite(33, ledStatus);
    }
    Serial.println("");
    Serial.println("WiFi connected");
    lcd.clear();
    lcd.print("WiFi Connected");
    startWebServer();

    Serial.print("Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
    lcd.setCursor(0, 1);
    lcd.print("IP:");
    lcd.setCursor(3, 1);
    lcd.print(WiFi.localIP());

    getHttpClient();
}

void loop()
{
    bool ledStatus = HIGH;
    digitalWrite(33, ledStatus);
    delay(100);
    ledStatus = LOW;
    digitalWrite(33, ledStatus);
    delay(100);
}
