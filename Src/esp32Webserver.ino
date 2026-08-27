#include <WiFi.h>
#include <DNSServer.h>
#include <EEPROM.h>
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

// WiFi Configuration
#define WIFI_TIMEOUT 20000  // 20 seconds timeout for WiFi connection
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 100
#define MAX_SSID_LENGTH 32
#define MAX_PASS_LENGTH 64

// AP Configuration
const char* ap_ssid = "ESP32-Setup";
const char* ap_password = "12345678";
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMsk(255, 255, 255, 0);

// DNS Server for captive portal
DNSServer dnsServer;
bool isAPMode = false;
int counter = 0;

// Function declarations
bool connectToWiFi();
void displayWiFiInfo();
void startAPMode();
void setupConfigServer();
String getConfigPage();
String readStringFromEEPROM(int addr, int maxLength);
void writeStringToEEPROM(int addr, String data, int maxLength);

void setup()
{
    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // Initialize pins
    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();

    display.setTextSize(2);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);             // Start at top-left corner
    display.println(F("WebServer"));
    display.display();
    delay(2000);

    // Try to connect to saved WiFi credentials
    if (connectToWiFi()) {
        Serial.println("Connected to WiFi successfully!");
        displayWiFiInfo();
    } else {
        Serial.println("Failed to connect to WiFi. Starting AP mode...");
        startAPMode();
    }
    startWebServer();
}

void loop()
{
    if (isAPMode) {
        dnsServer.processNextRequest();
    }
}

// Function to read WiFi credentials from EEPROM
String readStringFromEEPROM(int addr, int maxLength) {
    String data = "";
    char c;
    for (int i = 0; i < maxLength; i++) {
        c = EEPROM.read(addr + i);
        if (c == '\0') break;
        data += c;
    }
    return data;
}

// Function to write WiFi credentials to EEPROM
void writeStringToEEPROM(int addr, String data, int maxLength) {
    for (int i = 0; i < maxLength; i++) {
        if (i < data.length()) {
            EEPROM.write(addr + i, data[i]);
        } else {
            EEPROM.write(addr + i, '\0');
            break;
        }
    }
    EEPROM.commit();
}

// Function to attempt WiFi connection
bool connectToWiFi() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Checking saved WiFi...");
    display.display();

    // Read saved credentials
    String ssid = readStringFromEEPROM(SSID_ADDR, MAX_SSID_LENGTH);
    String password = readStringFromEEPROM(PASS_ADDR, MAX_PASS_LENGTH);

    // If no saved credentials, try default ones first
    if (ssid.length() != 0) {
        Serial.printf("Trying saved WiFi: %s\n", ssid.c_str());
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("Connecting to:\n%s", ssid.c_str());
    display.display();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();
    bool ledStatus = false;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
        delay(500);
        Serial.print(".");
        ledStatus = !ledStatus;
        digitalWrite(33, ledStatus);
    }

    if (WiFi.status() == WL_CONNECTED) {
        // If connection successful and using default credentials, save them
        if (readStringFromEEPROM(SSID_ADDR, MAX_SSID_LENGTH).length() == 0) {
            writeStringToEEPROM(SSID_ADDR, ssid, MAX_SSID_LENGTH);
            writeStringToEEPROM(PASS_ADDR, password, MAX_PASS_LENGTH);
            Serial.println("Default credentials saved to EEPROM");
        }
        digitalWrite(33, HIGH); // Turn on LED to indicate connection
        return true;
    } else {
        digitalWrite(33, LOW); // Turn off LED
        return false;
    }
}

// Function to display WiFi connection info
void displayWiFiInfo() {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    display.printf("SSID: %s\n", WiFi.SSID().c_str());
    display.display();
}

// Function to start AP mode with captive portal
void startAPMode() {
    isAPMode = true;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("AP Mode Started");
    display.printf("SSID: %s\n", ap_ssid);
    display.printf("Password: %s\n", ap_password);
    display.printf("IP: %s\n", apIP.toString().c_str());
    display.display();

    Serial.printf("Starting AP Mode - SSID: %s, Password: %s\n", ap_ssid, ap_password);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ap_ssid, ap_password);

    // Start DNS server for captive portal
    dnsServer.start(53, "*", apIP);

    Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("Connect to the AP and navigate to 192.168.4.1 to configure WiFi");
}
