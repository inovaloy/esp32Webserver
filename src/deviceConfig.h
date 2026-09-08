#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>
#include "autoGen/autoGenDeviceConfig.h"

// ── EEPROM map ────────────────────────────────────────────────────────────
//   0 –  31 : WiFi SSID          (32 bytes)
// 100 – 163 : WiFi password      (64 bytes)
// 164 – 195 : Admin password     (32 bytes)
// 200        : Magic byte         (1 byte, 0xA5)
// 210        : Device count       (1 byte)
// 220 – 507  : 8 × device slots  (36 bytes each)
//              Slot layout (36 bytes):
//                [0..15]  name         (16 bytes, null-terminated)
//                [16]     pin          (1 byte)
//                [17]     state        (1 byte)
//                [18]     autoEnabled  (1 byte, 0 or 1)
//                [19]     pingInterval (1 byte, seconds 5–255)
//                [20..35] pingIp       (16 bytes, dotted-decimal string)
// 508 – 528  : Controller name   (21 bytes)
// 529 – 530  : Logout minutes    (2 bytes, little-endian)
// 531        : OLED brightness   (1 byte)
// 532        : OLED enabled      (1 byte)
// 533        : Admin password set flag (1 byte, 0xA5 = configured)
// Total used : 534 bytes — well within AT25LC64 (8192 bytes)

// Admin password storage
#define ADMIN_PASS_ADDR         164
#define ADMIN_PASS_LEN           32

// Maximum devices driven by deviceConfig.yaml (via autoGenDeviceConfig.h)
#define MAX_DEVICES       CFG_MAX_DEVICES

// EEPROM layout for device storage
#define DEVICE_COUNT_ADDR  210
#define DEVICE_BASE_ADDR   220
#define DEVICE_SLOT_SIZE    36    // 16 name + 1 pin + 1 state + 1 autoEnabled + 1 pingInterval + 16 pingIp
#define DEVICE_NAME_LEN     16
#define PING_IP_LEN         16    // dotted-decimal, e.g. "192.168.1.1\0"

// Settings (after device slots: 220 + 8*36 = 508)
#define CONTROLLER_NAME_ADDR    508
#define CONTROLLER_NAME_LEN      21
#define LOGOUT_MINUTES_ADDR     529   // 2 bytes
#define DEFAULT_LOGOUT_MINUTES   15
#define OLED_BRIGHTNESS_ADDR    531
#define OLED_ENABLED_ADDR       532
#define ADMIN_PASSWORD_SET_ADDR 533
#define DEFAULT_OLED_BRIGHTNESS 100

// Single controllable GPIO device
struct Device {
    char    name[DEVICE_NAME_LEN];
    uint8_t pin;
    uint8_t state;        // 0 = off, 1 = on
    uint8_t autoEnabled;  // 0 = disabled, 1 = ping-watchdog enabled
    uint8_t pingInterval; // seconds between pings (5–255)
    char    pingIp[PING_IP_LEN]; // target IP as dotted-decimal string
};

#endif // DEVICE_CONFIG_H
