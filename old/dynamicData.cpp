#include "dynamicData.h"
#include "Arduino.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "WiFi.h"
#include <cJSON.h>

// Global variables to store sensor data
static float temperature = 25.0;
static float humidity = 50.0;
static int lightLevel = 780;
static bool ledState = false;
static unsigned long systemUptime = 0;

// Function to simulate sensor readings (replace with actual sensor code)
void updateSensorData() {
    // Simulate changing values
    temperature = 20.0 + (millis() % 10000) / 1000.0; // 20-30°C
    humidity = 40.0 + (millis() % 3000) / 100.0;      // 40-70%
    lightLevel = 500 + (millis() % 500);              // 500-1000 lux
    systemUptime = millis() / 1000;                   // uptime in seconds
}

// Get system information as JSON
char* getSystemInfoJson() {
    cJSON *json = cJSON_CreateObject();
    cJSON *systemInfo = cJSON_CreateObject();

    // System metrics
    cJSON_AddStringToObject(systemInfo, "status", "Online");
    cJSON_AddStringToObject(systemInfo, "wifi", "Connected");

    // Uptime formatting
    unsigned long uptime_hours = systemUptime / 3600;
    unsigned long uptime_minutes = (systemUptime % 3600) / 60;
    char uptime_str[32];
    sprintf(uptime_str, "%luh %lum", uptime_hours, uptime_minutes);
    cJSON_AddStringToObject(systemInfo, "uptime", uptime_str);

    // Memory info
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    char memory_str[16];
    sprintf(memory_str, "%zu KB", free_heap / 1024);
    cJSON_AddStringToObject(systemInfo, "freeMemory", memory_str);

    // CPU usage (simulated)
    int cpu_usage = 10 + (millis() % 20);
    char cpu_str[8];
    sprintf(cpu_str, "%d%%", cpu_usage);
    cJSON_AddStringToObject(systemInfo, "cpuUsage", cpu_str);

    // Temperature (from sensor or internal)
    char temp_str[16];
    sprintf(temp_str, "%.1f°C", temperature);
    cJSON_AddStringToObject(systemInfo, "temperature", temp_str);

    cJSON_AddItemToObject(json, "system", systemInfo);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

// Get sensor readings as JSON
char* getSensorDataJson() {
    updateSensorData(); // Update sensor values

    cJSON *json = cJSON_CreateObject();
    cJSON *sensors = cJSON_CreateObject();

    // Temperature
    char temp_str[16];
    sprintf(temp_str, "%.1f°C", temperature);
    cJSON_AddStringToObject(sensors, "temperature", temp_str);

    // Humidity
    char humid_str[16];
    sprintf(humid_str, "%.1f%%", humidity);
    cJSON_AddStringToObject(sensors, "humidity", humid_str);

    // Pressure (simulated)
    int pressure = 1010 + (millis() % 10);
    char pressure_str[16];
    sprintf(pressure_str, "%d hPa", pressure);
    cJSON_AddStringToObject(sensors, "pressure", pressure_str);

    // Light Level
    char light_str[16];
    sprintf(light_str, "%d lux", lightLevel);
    cJSON_AddStringToObject(sensors, "lightLevel", light_str);

    // Sound Level (simulated)
    int sound = 30 + (millis() % 20);
    char sound_str[16];
    sprintf(sound_str, "%d dB", sound);
    cJSON_AddStringToObject(sensors, "soundLevel", sound_str);

    // Air Quality
    const char* air_quality = (millis() % 10000 < 7000) ? "Good" : "Moderate";
    cJSON_AddStringToObject(sensors, "airQuality", air_quality);

    cJSON_AddItemToObject(json, "sensors", sensors);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

// Get network information as JSON
char* getNetworkInfoJson() {
    cJSON *json = cJSON_CreateObject();
    cJSON *network = cJSON_CreateObject();

    // Use Arduino WiFi library for compatibility
    // IP Address
    IPAddress ip = WiFi.localIP();
    char ip_str[16];
    sprintf(ip_str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    cJSON_AddStringToObject(network, "ipAddress", ip_str);

    // Gateway
    IPAddress gateway = WiFi.gatewayIP();
    char gateway_str[16];
    sprintf(gateway_str, "%d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
    cJSON_AddStringToObject(network, "gateway", gateway_str);

    // Subnet mask
    IPAddress subnet = WiFi.subnetMask();
    char netmask_str[16];
    sprintf(netmask_str, "%d.%d.%d.%d", subnet[0], subnet[1], subnet[2], subnet[3]);
    cJSON_AddStringToObject(network, "subnetMask", netmask_str);

    // MAC Address
    String mac = WiFi.macAddress();
    cJSON_AddStringToObject(network, "macAddress", mac.c_str());

    // Signal strength (RSSI)
    int32_t rssi = WiFi.RSSI();
    char rssi_str[16];
    sprintf(rssi_str, "%ld dBm", (long)rssi);
    cJSON_AddStringToObject(network, "signalStrength", rssi_str);

    // DNS Server
    IPAddress dns = WiFi.dnsIP();
    char dns_str[16];
    sprintf(dns_str, "%d.%d.%d.%d", dns[0], dns[1], dns[2], dns[3]);
    cJSON_AddStringToObject(network, "dnsServer", dns_str);

    cJSON_AddItemToObject(json, "network", network);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

// Handle device control commands
bool handleDeviceControl(const char* device, const char* action) {
    if (strcmp(device, "led") == 0) {
        if (strcmp(action, "on") == 0) {
            ledState = true;
            // Add your LED control code here
            Serial.println("LED turned ON");
            return true;
        } else if (strcmp(action, "off") == 0) {
            ledState = false;
            // Add your LED control code here
            Serial.println("LED turned OFF");
            return true;
        }
    }
    // Add more device control logic here
    return false;
}

// Get device control status as JSON
char* getControlStatusJson() {
    cJSON *json = cJSON_CreateObject();
    cJSON *controls = cJSON_CreateObject();

    cJSON_AddBoolToObject(controls, "ledState", ledState);
    // Add more control states here

    cJSON_AddItemToObject(json, "controls", controls);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}