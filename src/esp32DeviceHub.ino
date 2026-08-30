#include <WiFi.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "autoGen/autoGenWebServer.h"
#include "deviceConfig.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_I2C_ADDRESS 0x3C
#define EXTERNAL_EEPROM_I2C_ADDRESS 0x50
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Configuration
#define WIFI_TIMEOUT 20000
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 100
#define MAX_SSID_LENGTH 32
#define MAX_PASS_LENGTH 64
#define EEPROM_MAGIC_ADDR 200
#define EEPROM_MAGIC_BYTE 0xA5
#define FACTORY_RESET_BUTTON_PIN 23
#define FACTORY_RESET_HOLD_TIME 10000

// Runtime device list (loaded from EEPROM at boot)
Device  devices[MAX_DEVICES];
uint8_t deviceCount = 0;

// Admin password buffer
char adminPassword[ADMIN_PASS_LEN + 1];
char controllerName[CONTROLLER_NAME_LEN];
uint16_t logoutMinutes = DEFAULT_LOGOUT_MINUTES;
uint8_t oledBrightness = DEFAULT_OLED_BRIGHTNESS;
bool oledEnabled = true;
bool adminPasswordChangeRequired = true;

// AP credentials — derived from MAC at runtime
char apSsid[32];
char apPassword[16];
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMsk(255, 255, 255, 0);

// DNS Server for captive portal
DNSServer dnsServer;
bool isAPMode = false;
int counter = 0;
volatile bool rebootScheduled = false;
unsigned long rebootAt = 0;
volatile bool eepromDirty = false;   // set by httpd task; committed + rebooted in loop()
unsigned long factoryResetButtonPressedAt = 0;
bool factoryResetButtonHandled = false;

// Function declarations
bool eepromIsValid();
bool connectToWiFi();
void displayWiFiInfo();
void startAPMode();
void scheduleReboot();
String readStringFromEEPROM(int addr, int maxLength);
void writeStringToEEPROM(int addr, String data, int maxLength);
void loadDevicesFromEEPROM();
void saveDevicesToEEPROM();
void updateOledDeviceStatus();
void loadAdminPasswordFromEEPROM();
void saveAdminPassword(const char* newPassword, bool markConfigured = true);
bool checkAdminPassword(const char* attempt);
void loadControllerSettings();
void saveControllerSettings(const char* name, uint16_t minutes);
void saveOledSettings(uint8_t brightness, bool enabled);
void applyOledSettings();
void factoryResetSettings();
void checkFactoryResetButton();
uint8_t storageRead(int address);
void storageWrite(int address, uint8_t value);
void storageCommit();

bool i2cDevicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void setup()
{
    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    // Derive AP credentials from the factory eFuse MAC — safe before WiFi init
    uint64_t chipId = ESP.getEfuseMac();
    uint8_t mac[6];
    mac[0] = (chipId >> 40) & 0xFF;
    mac[1] = (chipId >> 32) & 0xFF;
    mac[2] = (chipId >> 24) & 0xFF;
    mac[3] = (chipId >> 16) & 0xFF;
    mac[4] = (chipId >>  8) & 0xFF;
    mac[5] = (chipId >>  0) & 0xFF;
    snprintf(apSsid,     sizeof(apSsid),     "ESP32-%02X%02X",     mac[4], mac[5]);
    snprintf(apPassword, sizeof(apPassword), "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);

    // Initialize EEPROM
    #if !CFG_STORAGE_EXTERNAL
    EEPROM.begin(EEPROM_SIZE);
    #endif

    // Check the I2C peripherals before using them.
    Wire.begin();
    bool oledPresent = i2cDevicePresent(OLED_I2C_ADDRESS);
    bool externalEepromPresent = i2cDevicePresent(EXTERNAL_EEPROM_I2C_ADDRESS);
    Serial.printf("[I2C] OLED (0x%02X): %s\r\n", OLED_I2C_ADDRESS,
                  oledPresent ? "FOUND" : "NOT FOUND");
    Serial.printf("[I2C] External EEPROM 24LC64 (0x%02X): %s\r\n", EXTERNAL_EEPROM_I2C_ADDRESS,
                  externalEepromPresent ? "FOUND" : "NOT FOUND");
    #if CFG_STORAGE_EXTERNAL
    if (!externalEepromPresent) {
        Serial.println("[STORAGE] External EEPROM selected but not found; storage is unavailable");
    } else {
        Serial.println("[STORAGE] Using external 24LC64");
    }
    #else
    Serial.println("[STORAGE] Using internal ESP32 flash EEPROM");
    #endif

    // Load display preferences before the first OLED update.
    loadControllerSettings();

    // Initialize pins
    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    pinMode(FACTORY_RESET_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    applyOledSettings();
    display.clearDisplay();

    display.setTextSize(2);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(0,0);             // Start at top-left corner
    display.println(F("WebServer"));
    display.display();
    delay(2000);

    // Load saved admin password (or set MAC-derived default on first boot)
    loadAdminPasswordFromEEPROM();

    // Load saved devices and restore GPIO states
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
    checkFactoryResetButton();
    if (isAPMode) {
        dnsServer.processNextRequest();
    }
    // Commit EEPROM from the main task — EEPROM.commit() must be called from the
    // same task that called EEPROM.begin(), which is this one (the Arduino main task).
    // The httpd FreeRTOS task only writes to the RAM buffer and sets this flag.
    if (eepromDirty) {
        storageCommit();
        eepromDirty = false;
    }
    if (rebootScheduled && millis() >= rebootAt) {
        Serial.println("Rebooting...");
        ESP.restart();
    }
}

void scheduleReboot() {
    rebootAt = millis() + 800;   // 800 ms — enough for HTTP response to flush
    rebootScheduled = true;
}

void checkFactoryResetButton() {
    bool buttonPressed = digitalRead(FACTORY_RESET_BUTTON_PIN) == LOW;

    if (!buttonPressed) {
        factoryResetButtonPressedAt = 0;
        factoryResetButtonHandled = false;
        return;
    }

    if (factoryResetButtonHandled || rebootScheduled) return;

    if (factoryResetButtonPressedAt == 0) {
        factoryResetButtonPressedAt = millis();
        Serial.println("Factory reset button pressed; hold for 10 seconds.");
        return;
    }

    if (millis() - factoryResetButtonPressedAt >= FACTORY_RESET_HOLD_TIME) {
        factoryResetButtonHandled = true;
        Serial.println("Factory reset button held for 10 seconds; resetting.");
        factoryResetSettings();
        scheduleReboot();
    }
}

// Returns true if EEPROM has been written by this firmware at least once
bool eepromIsValid() {
    return storageRead(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE;
}

uint8_t storageRead(int address) {
    #if CFG_STORAGE_EXTERNAL
    Wire.beginTransmission(EXTERNAL_EEPROM_I2C_ADDRESS);
    Wire.write((uint8_t)(address >> 8));
    Wire.write((uint8_t)(address & 0xFF));
    if (Wire.endTransmission() != 0 || Wire.requestFrom(EXTERNAL_EEPROM_I2C_ADDRESS, 1) != 1)
        return 0xFF;
    return Wire.read();
    #else
    return EEPROM.read(address);
    #endif
}

void storageWrite(int address, uint8_t value) {
    #if CFG_STORAGE_EXTERNAL
    Wire.beginTransmission(EXTERNAL_EEPROM_I2C_ADDRESS);
    Wire.write((uint8_t)(address >> 8));
    Wire.write((uint8_t)(address & 0xFF));
    Wire.write(value);
    Wire.endTransmission();
    delay(5);
    #else
    EEPROM.write(address, value);
    #endif
}

void storageCommit() {
    #if !CFG_STORAGE_EXTERNAL
    EEPROM.commit();
    #endif
}

// Function to read WiFi credentials from EEPROM
String readStringFromEEPROM(int addr, int maxLength) {
    if (!eepromIsValid()) return "";
    String data = "";
    char c;
    for (int i = 0; i < maxLength; i++) {
        uint8_t raw = storageRead(addr + i);
        if (raw == 0x00 || raw == 0xFF) break;
        c = (char)raw;
        data += c;
    }
    return data;
}

// Function to write WiFi credentials to EEPROM
void writeStringToEEPROM(int addr, String data, int maxLength) {
    for (int i = 0; i < maxLength; i++) {
        if (i < data.length()) {
            storageWrite(addr + i, data[i]);
        } else {
            storageWrite(addr + i, '\0');
            break;
        }
    }
    storageWrite(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    eepromDirty = true;  // committed from loop() in the main task
}

// Function to attempt WiFi connection
bool connectToWiFi() {
    // Read saved credentials
    String ssid = readStringFromEEPROM(SSID_ADDR, MAX_SSID_LENGTH);
    String password = readStringFromEEPROM(PASS_ADDR, MAX_PASS_LENGTH);

    if (ssid.length() == 0) {
        Serial.println("No saved WiFi SSID; starting AP mode.");
        return false;
    }
    Serial.printf("Trying saved WiFi: %s\r\n", ssid.c_str());

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Checking saved WiFi...");
    display.display();

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
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(33, HIGH);
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
    updateOledDeviceStatus();
}

// Function to start AP mode with captive portal
void startAPMode() {
    isAPMode = true;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("AP Mode Started");
    display.printf("SSID: %s\n", apSsid);
    display.printf("Password: %s\n", apPassword);
    display.printf("IP: %s\n", apIP.toString().c_str());
    display.display();

    Serial.printf("Starting AP Mode - SSID: %s, Password: %s\r\n", apSsid, apPassword);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(apSsid, apPassword);

    // Start DNS server for captive portal
    dnsServer.start(53, "*", apIP);

    Serial.printf("AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
    Serial.println("Connect to the AP and navigate to 192.168.4.1 to configure WiFi");
}

// ── EEPROM: devices ───────────────────────────────────────────────────────

void loadDevicesFromEEPROM() {
    Serial.printf("[EEPROM] magic=0x%02X count_addr=%d count=0x%02X\r\n",
                  storageRead(EEPROM_MAGIC_ADDR),
                  DEVICE_COUNT_ADDR,
                  storageRead(DEVICE_COUNT_ADDR));

    if (!eepromIsValid()) { deviceCount = 0; Serial.println("[EEPROM] invalid magic — skipping device load"); return; }
    uint8_t storedDeviceCount = storageRead(DEVICE_COUNT_ADDR);
    if (storedDeviceCount == 0xFF || storedDeviceCount > MAX_DEVICES) {
        Serial.printf("[EEPROM] count %d invalid for max %d — reset\r\n", storedDeviceCount, MAX_DEVICES);
        deviceCount = 0;
    } else {
        deviceCount = storedDeviceCount;
    }
    for (uint8_t i = 0; i < deviceCount; i++) {
        int base = DEVICE_BASE_ADDR + i * DEVICE_SLOT_SIZE;
        for (int j = 0; j < DEVICE_NAME_LEN; j++)
            devices[i].name[j] = storageRead(base + j);
        devices[i].name[DEVICE_NAME_LEN - 1] = '\0';
        devices[i].pin   = storageRead(base + DEVICE_NAME_LEN);
        devices[i].state = storageRead(base + DEVICE_NAME_LEN + 1);
        pinMode(devices[i].pin, OUTPUT);
        digitalWrite(devices[i].pin, devices[i].state ? HIGH : LOW);
        Serial.printf("[EEPROM] device[%d]: name=%s pin=%d state=%d\r\n",
                      i, devices[i].name, devices[i].pin, devices[i].state);
    }
}

void saveDevicesToEEPROM() {
    storageWrite(DEVICE_COUNT_ADDR, deviceCount);
    for (uint8_t i = 0; i < deviceCount; i++) {
        int base = DEVICE_BASE_ADDR + i * DEVICE_SLOT_SIZE;
        for (int j = 0; j < DEVICE_NAME_LEN; j++)
            storageWrite(base + j, devices[i].name[j]);
        storageWrite(base + DEVICE_NAME_LEN,     devices[i].pin);
        storageWrite(base + DEVICE_NAME_LEN + 1, devices[i].state);
    }
    storageWrite(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    // Do NOT call EEPROM.commit() here — this runs in the httpd FreeRTOS task,
    // which does not own the NVS handle opened by EEPROM.begin() in setup().
    // Calling commit() from the wrong task silently does nothing on ESP32 Arduino.
    // Set the dirty flag instead; loop() will commit from the correct main task.
    eepromDirty = true;
}

void updateOledDeviceStatus() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println(controllerName);
    display.setCursor(0, 10);
    if (WiFi.status() == WL_CONNECTED)
        display.printf("WiFi: %s", WiFi.SSID().c_str());
    else
        display.printf("AP: %s", apSsid);
    display.drawLine(0, 20, 127, 20, WHITE);
    uint8_t shown = deviceCount < 6 ? deviceCount : 6;
    for (uint8_t i = 0; i < shown; i++) {
        display.setCursor(0, 23 + i * 8);
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

// ── EEPROM: admin password ────────────────────────────────────────────────

void loadAdminPasswordFromEEPROM() {
    if (eepromIsValid()) {
        for (int i = 0; i < ADMIN_PASS_LEN; i++)
            adminPassword[i] = (char)storageRead(ADMIN_PASS_ADDR + i);
        adminPassword[ADMIN_PASS_LEN] = '\0';
        if (adminPassword[0] != '\0') {
            adminPasswordChangeRequired = storageRead(ADMIN_PASSWORD_SET_ADDR) != 0xA5;
            Serial.println("Admin password loaded from EEPROM");
            return;
        }
    }
    // First boot — derive default from MAC
    uint64_t chipId = ESP.getEfuseMac();
    uint8_t mac[6];
    mac[0] = (chipId >> 40) & 0xFF;
    mac[1] = (chipId >> 32) & 0xFF;
    mac[2] = (chipId >> 24) & 0xFF;
    mac[3] = (chipId >> 16) & 0xFF;
    mac[4] = (chipId >>  8) & 0xFF;
    mac[5] = (chipId >>  0) & 0xFF;
    snprintf(adminPassword, sizeof(adminPassword),
             "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
    saveAdminPassword(adminPassword, false);
    Serial.printf("Default admin password set from MAC: %s\r\n", adminPassword);
}

void saveAdminPassword(const char* newPassword, bool markConfigured) {
    strncpy(adminPassword, newPassword, ADMIN_PASS_LEN);
    adminPassword[ADMIN_PASS_LEN] = '\0';
    for (int i = 0; i < ADMIN_PASS_LEN; i++)
        storageWrite(ADMIN_PASS_ADDR + i, (uint8_t)adminPassword[i]);
    storageWrite(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    if (markConfigured) {
        storageWrite(ADMIN_PASSWORD_SET_ADDR, 0xA5);
        adminPasswordChangeRequired = false;
    }
    // Same rule: if called from httpd task, only set dirty flag.
    // If called from setup() (first boot), commit directly — setup() IS the main task.
    eepromDirty = true;
}

bool checkAdminPassword(const char* attempt) {
    uint8_t diff = 0;
    for (int i = 0; i < ADMIN_PASS_LEN; i++)
        diff |= (uint8_t)adminPassword[i] ^ (uint8_t)attempt[i];
    return diff == 0;
}

void loadControllerSettings() {
    if (eepromIsValid()) {
        for (int i = 0; i < CONTROLLER_NAME_LEN; i++)
            controllerName[i] = (char)storageRead(CONTROLLER_NAME_ADDR + i);
        controllerName[CONTROLLER_NAME_LEN - 1] = '\0';
        uint16_t storedMinutes = storageRead(LOGOUT_MINUTES_ADDR) |
                     ((uint16_t)storageRead(LOGOUT_MINUTES_ADDR + 1) << 8);
        uint8_t storedBrightness = storageRead(OLED_BRIGHTNESS_ADDR);
        uint8_t storedEnabled = storageRead(OLED_ENABLED_ADDR);
        if (controllerName[0] != '\0' && storedMinutes >= 1 && storedMinutes <= 1440) {
            logoutMinutes = storedMinutes;
            oledBrightness = storedBrightness == 0 ? DEFAULT_OLED_BRIGHTNESS : storedBrightness;
            oledEnabled = storedEnabled != 0;
            return;
        }
    }
    saveControllerSettings("Home Controller", DEFAULT_LOGOUT_MINUTES);
}

void saveControllerSettings(const char* name, uint16_t minutes) {
    strncpy(controllerName, name, CONTROLLER_NAME_LEN - 1);
    controllerName[CONTROLLER_NAME_LEN - 1] = '\0';
    logoutMinutes = minutes;
    for (int i = 0; i < CONTROLLER_NAME_LEN; i++)
        storageWrite(CONTROLLER_NAME_ADDR + i, (uint8_t)controllerName[i]);
    storageWrite(LOGOUT_MINUTES_ADDR, (uint8_t)(minutes & 0xFF));
    storageWrite(LOGOUT_MINUTES_ADDR + 1, (uint8_t)(minutes >> 8));
    storageWrite(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    eepromDirty = true;
}

void saveOledSettings(uint8_t brightness, bool enabled) {
    oledBrightness = brightness;
    oledEnabled = enabled;
    applyOledSettings();
    storageWrite(OLED_BRIGHTNESS_ADDR, brightness);
    storageWrite(OLED_ENABLED_ADDR, enabled ? 1 : 0);
    eepromDirty = true;
}

void applyOledSettings() {
    display.dim(!oledEnabled);
    if (oledEnabled) {
        display.ssd1306_command(SSD1306_SETCONTRAST);
        display.ssd1306_command(oledBrightness);
    }
}

void factoryResetSettings() {
    for (int i = 0; i < EEPROM_SIZE; i++) storageWrite(i, 0);
    deviceCount = 0;
    saveControllerSettings("Home Controller", DEFAULT_LOGOUT_MINUTES);
    saveOledSettings(DEFAULT_OLED_BRIGHTNESS, true);
    eepromDirty = true;
}
