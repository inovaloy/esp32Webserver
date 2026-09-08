#include "qrcode.h"
#include <WiFi.h>
#include <ETH.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "autoGen/autoGenWebServer.h"
#include "autoGen/autoGenOledLogo.h"   // bootLogo[], BOOT_LOGO_W, BOOT_LOGO_H
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
#define FACTORY_RESET_BUTTON_PIN 16
#define FACTORY_RESET_HOLD_TIME 10000
#define OLED_PAGE_BUTTON_PIN 4
#define OLED_PAGE_BUTTON_DEBOUNCE_MS 30
#define OLED_PAGE_BUTTON_HOLD_MS 800

// W5500 uses the ESP32 VSPI pins. GPIO16 is used for the factory-reset button.
// GPIO5 is reserved by the device configuration, so CS uses GPIO14.
#define ETHERNET_CS_PIN 14
#define ETHERNET_SCK_PIN 18
#define ETHERNET_MISO_PIN 19
#define ETHERNET_MOSI_PIN 23
#define ETHERNET_PHY_ADDR 1
#define ETHERNET_DHCP_TIMEOUT_MS 5000
#define ETHERNET_LINK_TIMEOUT_MS 5000
#define WIFI_RECONNECT_INTERVAL_MS 30000

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
const IPAddress ethernetFallbackIP(192, 168, 4, 1);
const IPAddress ethernetFallbackGateway(0, 0, 0, 0);
const IPAddress ethernetFallbackDns(0, 0, 0, 0);

// DNS Server for captive portal
DNSServer dnsServer;
bool isAPMode = false;
enum class NetworkType { None, EthernetDHCP, EthernetStatic, WiFi, AP };
NetworkType activeNetwork = NetworkType::None;
volatile bool ethernetLinkConnected = false;
volatile bool ethernetGotIp = false;
bool ethernetStarted = false;
unsigned long ethernetLinkDownSince = 0;
unsigned long ethernetLinkUpSince = 0;
bool ethernetFailoverHandled = false;
unsigned long wifiReconnectAttemptedAt = 0;
int counter = 0;
volatile bool rebootScheduled = false;
unsigned long rebootAt = 0;
volatile bool eepromDirty = false;   // set by httpd task; committed + rebooted in loop()
volatile bool oledStatusDirty = false;
unsigned long factoryResetButtonPressedAt = 0;
bool factoryResetButtonHandled = false;
bool oledPageButtonStablePressed = false;
bool oledPageButtonLastReading = false;
unsigned long oledPageButtonChangedAt = 0;
unsigned long oledPageButtonPressedAt = 0;
uint8_t oledDevicePage = 0;
unsigned long oledDevicePageChangedAt = 0;
// In AP mode the button toggles between QR page and info page.
// true = show QR (default); false = show info.
bool apShowQr = true;

// ── AP mode OLED pages ────────────────────────────────────────────────────

// Page 1 (default): QR code on the left + WiFi SSID/Key on the right.
// QR: version 3 (29×29) at 2 px/module = 58×58 px, x=3, y=3.
// Right panel: x=64, width=64 px (max 10 chars/line at text size 1).
void drawApQrPage() {
    char payload[64];
    snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", apSsid, apPassword);

    QRCode qr;
    uint8_t qrBuf[qrcode_getBufferSize(3)];
    qrcode_initText(&qr, qrBuf, 3, ECC_LOW, payload);

    display.clearDisplay();

    const uint8_t scale = 2;
    const uint8_t xOff  = 3;
    const uint8_t yOff  = 3;
    for (uint8_t y = 0; y < qr.size; y++) {
        for (uint8_t x = 0; x < qr.size; x++) {
            if (qrcode_getModule(&qr, x, y)) {
                display.fillRect(xOff + x * scale, yOff + y * scale,
                                 scale, scale, WHITE);
            }
        }
    }

    // Right panel — WiFi credentials (all values fit within 10 chars)
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(64,  0); display.println("WiFi:");
    display.setCursor(64,  8); display.println(apSsid);
    display.setCursor(64, 20); display.println("Key:");
    display.setCursor(64, 28); display.println(apPassword);
    display.setCursor(64, 44); display.println("[btn]=info");
    display.display();
}

// Page 2 (button press): full-width text — IP address + admin password.
// Uses full 128 px width so the IP (192.168.4.1) never overflows.
void drawApInfoPage() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,  0); display.println("-- Connect info --");
    display.setCursor(0, 12); display.print("IP:    "); display.println(apIP.toString().c_str());
    display.setCursor(0, 24); display.print("Admin: ");
    // Show the actual password only while it is still the factory default.
    // Once the user has changed it, show **** to confirm it is set but keep it secret.
    if (adminPasswordChangeRequired) {
        display.println(adminPassword);
    } else {
        display.println("********");
    }
    display.setCursor(0, 36); display.println("Open IP in browser");
    display.setCursor(0, 52); display.println("[btn]=QR code");
    display.display();
}

// Function declarations
bool eepromIsValid();
bool startNetwork();
bool startEthernet();
bool configureEthernetFallback();
void handleEthernetLinkLoss();
void checkEthernetAvailability();
void checkWiFiAvailability();
bool connectToWiFi();
void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
void displayNetworkInfo();
void displayWiFiInfo();
void startAPMode();
void scheduleReboot();
String readStringFromEEPROM(int addr, int maxLength);
void writeStringToEEPROM(int addr, String data, int maxLength);
void loadDevicesFromEEPROM();
void saveDevicesToEEPROM();
void updateOledDeviceStatus();
uint8_t oledPageCount();
bool isHighVoltageDevice(uint8_t pin);
void loadAdminPasswordFromEEPROM();
void saveAdminPassword(const char* newPassword, bool markConfigured = true);
bool checkAdminPassword(const char* attempt);
void loadControllerSettings();
void saveControllerSettings(const char* name, uint16_t minutes);
void saveOledSettings(uint8_t brightness, bool enabled);
void applyOledSettings();
void factoryResetSettings();
void checkFactoryResetButton();
void checkOledPageButton();
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
    pinMode(OLED_PAGE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(33, OUTPUT);
    pinMode(FACTORY_RESET_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(33, ledStatus);

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    applyOledSettings();
    display.clearDisplay();

    // Boot splash: logo (48x48) centred vertically on left, "Device Hub" on right
    display.drawBitmap(0, (SCREEN_HEIGHT - BOOT_LOGO_H) / 2,
                       bootLogo, BOOT_LOGO_W, BOOT_LOGO_H, WHITE);
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(52, 12); display.println(F("Device"));
    display.setCursor(52, 30); display.println(F("Hub"));
    display.drawFastHLine(52, 48, 76, WHITE);
    display.setTextSize(1);
    display.setCursor(52, 52); display.println(F("v1.0.1"));
    display.display();
    delay(2000);

    // Load saved admin password (or set MAC-derived default on first boot)
    loadAdminPasswordFromEEPROM();

    // Load saved devices and restore GPIO states
    loadDevicesFromEEPROM();

    startNetwork();
    startWebServer();
}

void loop()
{
    checkFactoryResetButton();
    checkOledPageButton();
    checkEthernetAvailability();
    checkWiFiAvailability();
    if ((activeNetwork == NetworkType::EthernetDHCP || activeNetwork == NetworkType::EthernetStatic) && !ETH.linkUp()) {
        if (ethernetLinkDownSince == 0) ethernetLinkDownSince = millis();
        if (!ethernetFailoverHandled && millis() - ethernetLinkDownSince >= 1000) {
            handleEthernetLinkLoss();
        }
    } else if (ETH.linkUp()) {
        ethernetLinkDownSince = 0;
    }
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
    if (oledStatusDirty) {
        updateOledDeviceStatus();
        oledStatusDirty = false;
    }
    if (!isAPMode && !oledPageButtonStablePressed && millis() - oledDevicePageChangedAt >= 4000) {
        oledDevicePage = (oledDevicePage + 1) % oledPageCount();
        oledDevicePageChangedAt = millis();
        updateOledDeviceStatus();
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

void checkOledPageButton() {
    bool readingPressed = digitalRead(OLED_PAGE_BUTTON_PIN) == LOW;
    unsigned long now = millis();

    if (readingPressed != oledPageButtonLastReading) {
        oledPageButtonChangedAt = now;
        oledPageButtonLastReading = readingPressed;
    }

    if (now - oledPageButtonChangedAt < OLED_PAGE_BUTTON_DEBOUNCE_MS ||
        readingPressed == oledPageButtonStablePressed) {
        return;
    }

    oledPageButtonStablePressed = readingPressed;
    if (readingPressed) {
        oledPageButtonPressedAt = now;
        return;
    }

    // Short press: in AP mode toggle QR/info; otherwise advance normal page.
    if (now - oledPageButtonPressedAt < OLED_PAGE_BUTTON_HOLD_MS) {
        if (isAPMode) {
            apShowQr = !apShowQr;
        } else {
            oledDevicePage = (oledDevicePage + 1) % oledPageCount();
        }
    }
    oledDevicePageChangedAt = now;
    updateOledDeviceStatus();
}

// Returns true if EEPROM has been written by this firmware at least once
bool eepromIsValid() {
    return storageRead(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE;
}

void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("[ETH] Started");
        ETH.setHostname("esp32-device-hub");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        ethernetLinkConnected = true;
        ethernetFailoverHandled = false;
        ethernetLinkUpSince = millis();
        Serial.println("[ETH] Link up");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        ethernetGotIp = true;
        Serial.printf("[ETH] Got IP: %s\r\n", ETH.localIP().toString().c_str());
        oledStatusDirty = true;
        break;
    case ARDUINO_EVENT_ETH_LOST_IP:
        ethernetGotIp = false;
        Serial.println("[ETH] Lost IP");
        oledStatusDirty = true;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        ethernetLinkConnected = false;
        ethernetGotIp = false;
        ethernetLinkUpSince = 0;
        Serial.println("[ETH] Link down");
        oledStatusDirty = true;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        ethernetLinkConnected = false;
        ethernetGotIp = false;
        ethernetStarted = false;
        ethernetLinkUpSince = 0;
        Serial.println("[ETH] Stopped");
        break;
    default:
        break;
    }
}

bool configureEthernetFallback() {
    Serial.println("[ETH] DHCP timeout; using static Ethernet IP 192.168.4.1");
    if (!ETH.config(ethernetFallbackIP, ethernetFallbackGateway, netMsk,
                    ethernetFallbackDns, ethernetFallbackDns)) {
        Serial.println("[ETH] Failed to configure static fallback");
        return false;
    }
    activeNetwork = NetworkType::EthernetStatic;
    return true;
}

bool startEthernet() {
    Serial.println("[ETH] Initializing W5500...");
    ethernetLinkConnected = false;
    ethernetGotIp = false;
    Network.onEvent(onNetworkEvent);

    if (!ETH.begin(ETH_PHY_W5500, ETHERNET_PHY_ADDR, ETHERNET_CS_PIN, -1, -1,
                   SPI2_HOST, ETHERNET_SCK_PIN, ETHERNET_MISO_PIN, ETHERNET_MOSI_PIN)) {
        Serial.println("[ETH] W5500 initialization failed");
        return false;
    }
    ethernetStarted = true;

    unsigned long linkStart = millis();
    while (!ethernetLinkConnected && !ETH.linkUp() && millis() - linkStart < ETHERNET_LINK_TIMEOUT_MS) {
        delay(25);
    }
    if (!ethernetLinkConnected && ETH.linkUp()) {
        ethernetLinkConnected = true;
        Serial.println("[ETH] Link detected by polling");
    }
    if (!ethernetLinkConnected) {
        Serial.println("[ETH] No Ethernet link");
        return false;
    }

    Serial.println("[ETH] Waiting for DHCP...");
    unsigned long dhcpStart = millis();
    while (!ethernetGotIp && millis() - dhcpStart < ETHERNET_DHCP_TIMEOUT_MS) {
        delay(25);
    }
    if (ethernetGotIp) {
        activeNetwork = NetworkType::EthernetDHCP;
        return true;
    }

    return configureEthernetFallback();
}

void checkEthernetAvailability() {
    if (!ethernetStarted) return;

    if (!ethernetLinkConnected && ETH.linkUp()) {
        ethernetLinkConnected = true;
        ethernetLinkUpSince = millis();
        Serial.println("[ETH] Link detected while WiFi is active");
    }
    if (!ethernetLinkConnected) return;

    if (ethernetGotIp && (activeNetwork == NetworkType::WiFi || activeNetwork == NetworkType::AP)) {
        if (isAPMode) {
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
            isAPMode = false;
        }
        activeNetwork = NetworkType::EthernetDHCP;
        ethernetFailoverHandled = false;
        Serial.printf("[NET] Ethernet is now preferred at %s; WiFi remains connected\r\n",
                      ETH.localIP().toString().c_str());
        displayNetworkInfo();
        return;
    }

    if ((activeNetwork == NetworkType::WiFi || activeNetwork == NetworkType::AP) && !ethernetGotIp && ethernetLinkUpSince != 0 &&
        millis() - ethernetLinkUpSince >= ETHERNET_DHCP_TIMEOUT_MS) {
        if (isAPMode) {
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
            isAPMode = false;
        }
        if (configureEthernetFallback()) {
            Serial.println("[NET] Ethernet static fallback is now preferred; WiFi remains connected");
            displayNetworkInfo();
        }
    }
}

bool startNetwork() {
    isAPMode = false;
    ethernetFailoverHandled = false;
    ethernetLinkDownSince = 0;
    ethernetLinkUpSince = 0;
    Serial.println("[NET] Trying saved WiFi credentials...");
    bool ethernetAvailable = startEthernet();
    bool wifiAvailable = connectToWiFi();
    if (ethernetAvailable) {
        activeNetwork = ethernetGotIp ? NetworkType::EthernetDHCP : NetworkType::EthernetStatic;
        Serial.println(wifiAvailable ? "[NET] Ethernet and WiFi are connected" : "[NET] Ethernet connected; WiFi unavailable");
        displayNetworkInfo();
        return true;
    }
    if (wifiAvailable) {
        activeNetwork = NetworkType::WiFi;
        Serial.println("[NET] WiFi connected");
        displayNetworkInfo();
        return true;
    }

    Serial.println("[NET] WiFi connection failed; starting AP mode");
    startAPMode();
    displayNetworkInfo();
    return true;
}

void handleEthernetLinkLoss() {
    ethernetFailoverHandled = true;
    ethernetLinkDownSince = 0;
    Serial.println("[NET] Ethernet cable disconnected; switching to saved WiFi");
    ethernetLinkConnected = false;
    ethernetGotIp = false;
    if (WiFi.status() == WL_CONNECTED) {
        activeNetwork = NetworkType::WiFi;
        Serial.println("[NET] WiFi remains connected after Ethernet link loss");
        displayNetworkInfo();
        return;
    }

    if (connectToWiFi()) {
        activeNetwork = NetworkType::WiFi;
        Serial.println("[NET] WiFi connected after Ethernet link loss");
        displayNetworkInfo();
        return;
    }

    Serial.println("[NET] WiFi unavailable after Ethernet link loss; starting AP mode");
    startAPMode();
    displayNetworkInfo();
}

void checkWiFiAvailability() {
    if (isAPMode || WiFi.status() == WL_CONNECTED) return;
    if (millis() - wifiReconnectAttemptedAt < WIFI_RECONNECT_INTERVAL_MS) return;

    wifiReconnectAttemptedAt = millis();
    Serial.println("[NET] WiFi is unavailable; trying saved credentials");
    if (connectToWiFi()) {
        Serial.println("[NET] WiFi connected while Ethernet remains active");
        if (!ethernetGotIp) {
            activeNetwork = NetworkType::WiFi;
            displayNetworkInfo();
        }
    }
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

void displayNetworkInfo() {
    String networkName;
    IPAddress networkIP;
    if (activeNetwork == NetworkType::EthernetDHCP) {
        networkName = "Ethernet";
        networkIP = ETH.localIP();
    } else if (activeNetwork == NetworkType::EthernetStatic) {
        networkName = "Ethernet Direct";
        networkIP = ethernetFallbackIP;
    } else if (activeNetwork == NetworkType::WiFi) {
        networkName = "WiFi";
        networkIP = WiFi.localIP();
    } else {
        networkName = "AP Mode";
        networkIP = apIP;
    }

    Serial.printf("[NET] %s active at http://%s\r\n", networkName.c_str(), networkIP.toString().c_str());
    // AP mode already shows the QR page from startAPMode(); for all other
    // network types go straight to the device-status view.
    if (!isAPMode) {
        updateOledDeviceStatus();
    }
}

void displayWiFiInfo() {
    activeNetwork = NetworkType::WiFi;
    displayNetworkInfo();
}

// Function to start AP mode with captive portal
void startAPMode() {
    isAPMode = true;
    activeNetwork = NetworkType::AP;

    Serial.printf("Starting AP Mode - SSID: %s, Password: %s\r\n", apSsid, apPassword);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(apSsid, apPassword);

    // Start DNS server for captive portal
    dnsServer.start(53, "*", apIP);

    Serial.printf("AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
    Serial.println("Connect to the AP and navigate to 192.168.4.1 to configure WiFi");

    // Start on QR page; user can press the page button to see the info page
    apShowQr = true;
    drawApQrPage();
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

bool isHighVoltageDevice(uint8_t pin) {
    for (uint8_t i = 0; i < CFG_HIGH_VOLTAGE_PIN_COUNT; i++) {
        if (CFG_HIGH_VOLTAGE_PINS[i] == pin) return true;
    }
    return false;
}

// Returns the total number of OLED pages based on how many devices are configured.
// Pages: 0=network info, 1=device summary, 2+=device list (4 devices per page).
// Device list pages are only included when there are devices to show.
uint8_t oledPageCount() {
    if (deviceCount == 0) return 2;                      // no devices: info + summary only
    return 2 + ((deviceCount + 3) / 4);                  // ceil(deviceCount / 4) list pages
}

void updateOledDeviceStatus() {
    // In AP mode: render whichever page the button has selected
    if (isAPMode) {
        if (apShowQr) drawApQrPage();
        else          drawApInfoPage();
        return;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    if (oledDevicePage == 0) {
        // When the admin password has never been changed, show it on this page
        // so the user knows what to log in with.  Once changed, hide it.
        // Tighten row spacing slightly when the extra row is needed.
        const bool showPwd = adminPasswordChangeRequired;
        const uint8_t rowStep = showPwd ? 10 : 14;

        display.setCursor(0, 0);
        display.println(controllerName);
        display.setCursor(0, rowStep);
        if (ETH.hasIP()) {
            display.print("ETH: ");
            display.println(ETH.localIP());
        } else {
            display.println("ETH: unavailable");
        }
        display.setCursor(0, rowStep * 2);
        if (WiFi.status() == WL_CONNECTED) {
            display.print("WiFi: ");
            display.println(WiFi.localIP());
        } else {
            display.println("WiFi: unavailable");
        }
        if (showPwd) {
            display.setCursor(0, rowStep * 3);
            display.print("Admin: ");
            display.println(adminPassword);
        }
        display.setCursor(0, 42);
        display.println("Firmware: 1.0.1");
    } else if (oledDevicePage == 1) {
        uint8_t highVoltageCount = 0;
        uint8_t lowVoltageCount = 0;
        uint8_t onCount = 0;
        for (uint8_t i = 0; i < deviceCount; i++) {
            if (isHighVoltageDevice(devices[i].pin)) highVoltageCount++;
            else lowVoltageCount++;
            if (devices[i].state) onCount++;
        }
        display.setCursor(0, 0);
        display.println("Device summary");
        display.setCursor(0, 14);
        display.printf("Total devices: %d", deviceCount);
        display.setCursor(0, 26);
        display.printf("High power:   %d", highVoltageCount);
        display.setCursor(0, 38);
        display.printf("Low power:    %d", lowVoltageCount);
        display.setCursor(0, 50);
        display.printf("ON: %d       OFF: %d", onCount, deviceCount - onCount);
    } else {
        uint8_t firstDevice = (oledDevicePage - 2) * 4;
        uint8_t lastDevice = firstDevice + 4;
        if (lastDevice > deviceCount) lastDevice = deviceCount;
        display.setCursor(0, 0);
        display.printf("Devices %d-%d", firstDevice + 1, firstDevice + 4);
        for (uint8_t i = firstDevice; i < lastDevice && i < MAX_DEVICES; i++) {
            display.setCursor(0, 12 + (i - firstDevice) * 12);
            display.print(devices[i].name);
            const char* status = devices[i].state ? "ON" : "OFF";
            display.setCursor(128 - strlen(status) * 6, 12 + (i - firstDevice) * 12);
            display.print(status);
        }
        if (deviceCount == 0) {
            display.setCursor(0, 24);
            display.println("No devices added");
        }
    }
    oledDevicePageChangedAt = millis();
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
    // Default password is visible on the AP info page (button press) —
    // no separate boot screen needed.
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
    oledStatusDirty = true;
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
