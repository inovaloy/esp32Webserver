#include <WiFi.h>
#include "AutoGen/autoGenWebServer.h"
// #include "httpClient.h"
// #include "sd_card.h"
// #include "LiquidCrystal_I2C.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int counter = 0;

void setup()
{
    const char* ssid     = "Biswas family";
    const char* password = "biswas2022";

    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    // Serial.println("Read the SD card...!!!");
    // readSdCard();

    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();

    display.setTextSize(2);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);             // Start at top-left corner
    display.println(F("ESP32 WebServer"));


    display.display();
    delay(2000);

    Serial.println("Connecting...");
    display.setTextSize(1);
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Connecting to WiFi...");
    display.display();

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
    display.clearDisplay();
    display.println("WiFi connected");
    display.display();

    startWebServer();

    Serial.print("Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
    display.print("http://");
    display.print(WiFi.localIP());
    display.display();

    // getHttpClient(); // Removed - HTTPClient not used
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
