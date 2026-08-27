#include <WiFi.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "AutoGen/autoGenWebServer.h"
#include "deviceConfig.h"
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
#define EEPROM_MAGIC_ADDR 200   // address that holds the validity marker
#define EEPROM_MAGIC_BYTE 0xA5  // arbitrary non-0xFF sentinel value

// Device storage — up to 16 controllable GPIO devices
// (struct and defines are in deviceConfig.h)

// Runtime device list (loaded from EEPROM at boot)
Device  devices[MAX_DEVICES];
uint8_t deviceCount = 0;

// Admin password buffer (loaded from EEPROM at boot)
char adminPassword[ADMIN_PASS_LEN + 1];  // +1 for null terminator

// Device EEPROM helpers
void loadDevicesFromEEPROM();
void saveDevicesToEEPROM();
void updateOledDeviceStatus();

// Admin password helpers
void loadAdminPasswordFromEEPROM();
void saveAdminPassword(const char* newPassword);

// AP Configuration — SSID and password are derived from the device MAC at runtime
// so each device is unique and the password is not the same for every unit.
// These are populated in setup() before startAPMode() is called.
char ap_ssid[32];
char ap_password[16];
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMsk(255, 255, 255, 0);

// DNS Server for captive portal
DNSServer dnsServer;

bool isAPMode = false;
int counter = 0;

// Function declarations
bool eepromIsValid();
bool connectToWiFi();
void displayWiFiInfo();
void startAPMode();
String readStringFromEEPROM(int addr, int maxLength);
void writeStringToEEPROM(int addr, String data, int maxLength);
bool checkAdminPassword(const char* attempt);

void setup()
{
    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    // Derive AP credentials from the device MAC address.
    // SSID:     "ESP32-<last 4 hex digits of MAC>"  e.g. "ESP32-A1B2"
    // Password: last 8 hex digits of MAC            e.g. "C3D4E5F6"
    // WiFi.macAddress() returns "XX:XX:XX:XX:XX:XX" — parse the last 4 bytes.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(ap_ssid,     sizeof(ap_ssid),     "ESP32-%02X%02X",     mac[4], mac[5]);
    snprintf(ap_password, sizeof(ap_password), "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);

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

    // Load saved admin password from EEPROM (or set MAC-derived default)
    loadAdminPasswordFromEEPROM();

    // Load saved devices from EEPROM and initialise their GPIO pins
    loadDevicesFromEEPROM();

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

// Returns true if EEPROM has been written by this firmware at least once
bool eepromIsValid() {
    return EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE;
}

// Function to read WiFi credentials from EEPROM
String readStringFromEEPROM(int addr, int maxLength) {
    if (!eepromIsValid()) {
        return "";
    }
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
    // Stamp the magic byte so future reads know the EEPROM is initialised
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    EEPROM.commit();
}

// Function to attempt WiFi connection
bool connectToWiFi() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Checking saved WiFi...");
    display.display();

    // Read saved credentials (returns "" if EEPROM was never written)
    String ssid = readStringFromEEPROM(SSID_ADDR, MAX_SSID_LENGTH);
    String password = readStringFromEEPROM(PASS_ADDR, MAX_PASS_LENGTH);

    if (ssid.length() == 0) {
        Serial.println("No saved WiFi credentials found.");
        return false;
    }

    Serial.printf("Trying saved WiFi: %s\n", ssid.c_str());

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
    delay(2000);

    // Switch OLED to device status view after showing connection info
    updateOledDeviceStatus();
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

// ── Device storage ────────────────────────────────────────────────────────

void loadDevicesFromEEPROM() {
    if (!eepromIsValid()) {
        deviceCount = 0;
        return;
    }
    deviceCount = EEPROM.read(DEVICE_COUNT_ADDR);
    if (deviceCount > MAX_DEVICES) deviceCount = 0; // sanity check

    for (uint8_t i = 0; i < deviceCount; i++) {
        int base = DEVICE_BASE_ADDR + i * DEVICE_SLOT_SIZE;
        for (int j = 0; j < DEVICE_NAME_LEN; j++) {
            devices[i].name[j] = EEPROM.read(base + j);
        }
        devices[i].name[DEVICE_NAME_LEN - 1] = '\0'; // always null-terminate
        devices[i].pin   = EEPROM.read(base + DEVICE_NAME_LEN);
        devices[i].state = EEPROM.read(base + DEVICE_NAME_LEN + 1);

        // Restore GPIO pin direction and state
        pinMode(devices[i].pin, OUTPUT);
        digitalWrite(devices[i].pin, devices[i].state ? HIGH : LOW);
        Serial.printf("Loaded device[%d]: %s pin=%d state=%d\n",
                      i, devices[i].name, devices[i].pin, devices[i].state);
    }
}

void saveDevicesToEEPROM() {
    EEPROM.write(DEVICE_COUNT_ADDR, deviceCount);
    for (uint8_t i = 0; i < deviceCount; i++) {
        int base = DEVICE_BASE_ADDR + i * DEVICE_SLOT_SIZE;
        for (int j = 0; j < DEVICE_NAME_LEN; j++) {
            EEPROM.write(base + j, devices[i].name[j]);
        }
        EEPROM.write(base + DEVICE_NAME_LEN,     devices[i].pin);
        EEPROM.write(base + DEVICE_NAME_LEN + 1, devices[i].state);
    }
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE); // keep magic valid
    EEPROM.commit();
}

// Update OLED with current device count and states
// Layout (128x64):
//   Line 0: "Home Controller"
//   Line 1: WiFi SSID or "AP Mode"
//   Lines 2-7: device name + state (max 6 shown, scrolls not supported)
void updateOledDeviceStatus() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Home Controller");

    // WiFi line
    display.setCursor(0, 10);
    if (WiFi.status() == WL_CONNECTED) {
        display.printf("WiFi: %s", WiFi.SSID().c_str());
    } else {
        display.printf("AP: %s", ap_ssid);
    }

    // Separator
    display.drawLine(0, 20, 127, 20, WHITE);

    // Device lines — up to 6 fit at y=23,31,39,47,55 (8px each)
    uint8_t shown = deviceCount < 6 ? deviceCount : 6;
    for (uint8_t i = 0; i < shown; i++) {
        display.setCursor(0, 23 + i * 8);
        // Truncate name to 10 chars so state label fits
        char truncated[11];
        strncpy(truncated, devices[i].name, 10);
        truncated[10] = '\0';
        display.printf("%-10s %s", truncated, devices[i].state ? "ON " : "OFF");
    }
    if (deviceCount == 0) {
        display.setCursor(0, 30);
        display.println("No devices added");
    }

    display.display();
}

// ── Admin password ────────────────────────────────────────────────────────

// Load admin password from EEPROM.
// If EEPROM has never been written (fresh chip), derive a default from the
// last 8 hex digits of the MAC and save it — same approach as the AP password.
void loadAdminPasswordFromEEPROM() {
    if (eepromIsValid()) {
        // Read stored password
        for (int i = 0; i < ADMIN_PASS_LEN; i++) {
            adminPassword[i] = (char)EEPROM.read(ADMIN_PASS_ADDR + i);
        }
        adminPassword[ADMIN_PASS_LEN] = '\0';
        // Validate: if the slot is all zeros treat as unset
        if (adminPassword[0] != '\0') {
            Serial.printf("Admin password loaded from EEPROM\n");
            return;
        }
    }

    // No password saved yet — derive default from MAC and store it
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(adminPassword, sizeof(adminPassword),
             "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
    saveAdminPassword(adminPassword);
    Serial.printf("Default admin password set from MAC: %s\n", adminPassword);
}

// Persist a new admin password to EEPROM.
void saveAdminPassword(const char* newPassword) {
    strncpy(adminPassword, newPassword, ADMIN_PASS_LEN);
    adminPassword[ADMIN_PASS_LEN] = '\0';
    for (int i = 0; i < ADMIN_PASS_LEN; i++) {
        EEPROM.write(ADMIN_PASS_ADDR + i, (uint8_t)adminPassword[i]);
    }
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    EEPROM.commit();
}

// Constant-time password comparison to prevent timing attacks.
bool checkAdminPassword(const char* attempt) {
    uint8_t diff = 0;
    for (int i = 0; i < ADMIN_PASS_LEN; i++) {
        diff |= (uint8_t)adminPassword[i] ^ (uint8_t)attempt[i];
    }
    return diff == 0;
}
