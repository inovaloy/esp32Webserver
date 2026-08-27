#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

// ── EEPROM map ────────────────────────────────────────────────────────────
//   0 –  31 : WiFi SSID          (32 bytes)
// 100 – 163 : WiFi password      (64 bytes)
// 164 – 195 : Admin password     (32 bytes)
// 200        : Magic byte         (1 byte, 0xA5)
// 210        : Device count       (1 byte)
// 220 – 507  : 16 × device slots (18 bytes each)

// Admin password storage
#define ADMIN_PASS_ADDR   164
#define ADMIN_PASS_LEN    32

// Maximum number of controllable GPIO devices
#define MAX_DEVICES       16

// EEPROM layout for device storage
#define DEVICE_COUNT_ADDR 210
#define DEVICE_BASE_ADDR  220
#define DEVICE_SLOT_SIZE  18    // 16 bytes name + 1 byte pin + 1 byte state
#define DEVICE_NAME_LEN   16

// Single controllable GPIO device
struct Device {
    char    name[DEVICE_NAME_LEN];
    uint8_t pin;
    uint8_t state;  // 0 = off, 1 = on
};

#endif // DEVICE_CONFIG_H
