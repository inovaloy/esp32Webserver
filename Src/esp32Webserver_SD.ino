/*
 * ESP32 Web Server with SD Card Support - Example
 *
 * This example shows how to use the web server with HTML files from SD card
 * instead of compiled HTML embedded in firmware.
 *
 * To use this:
 * 1. Run: python3 Scripts/prepareSDCard.py
 * 2. Copy sd_card_files/html/* to your SD card's /html/ folder
 * 3. Insert SD card into ESP32
 * 4. Upload this sketch
 */

#include <WiFi.h>
#include "webServerSD.h"  // Use SD card version instead of AutoGen/autoGenWebServer.h
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SD Card Configuration
#define SD_CS_PIN 5  // Change this if your SD card CS pin is different

void setup() {
    const char* ssid     = "Biswas family";
    const char* password = "biswas2022";

    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();

    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println(F("ESP32 WebServer"));
    display.display();
    delay(2000);

    // Connect to WiFi
    Serial.println("Connecting...");
    display.setTextSize(1);
    display.setTextColor(WHITE);
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

    // Start Web Server with SD Card Support
    //
    // Option 1: Enable SD card with fallback to compiled HTML
    startWebServerWithSD(true, SD_CS_PIN);
    //
    // Option 2: Use only compiled HTML (no SD card)
    // startWebServerWithSD(false);
    //
    // Note: If SD card fails or files are missing, automatically falls back to compiled HTML

    Serial.print("Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");

    display.clearDisplay();
    display.println("Server Ready!");
    display.print("http://");
    display.print(WiFi.localIP());
    display.display();
}

void loop() {
    bool ledStatus = HIGH;
    digitalWrite(33, ledStatus);
    delay(100);
    ledStatus = LOW;
    digitalWrite(33, ledStatus);
    delay(100);
}
