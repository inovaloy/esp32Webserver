# Architecture

This document describes the firmware architecture, source code organisation, and how the individual subsystems interact at runtime.

---

## Table of Contents

1. [High-Level System Overview](#1-high-level-system-overview)
2. [Source Tree](#2-source-tree)
3. [Firmware Layers](#3-firmware-layers)
4. [Boot Sequence](#4-boot-sequence)
5. [Main Loop](#5-main-loop)
6. [Network Subsystem](#6-network-subsystem)
7. [HTTP Web Server](#7-http-web-server)
8. [Autogen Pipeline (Code Generation)](#8-autogen-pipeline-code-generation)
9. [Storage Subsystem (EEPROM)](#9-storage-subsystem-eeprom)
10. [Authentication and Session Management](#10-authentication-and-session-management)
11. [Device Control and Ping Watchdog](#11-device-control-and-ping-watchdog)
12. [OLED Display](#12-oled-display)
13. [Data Flow Diagram](#13-data-flow-diagram)
14. [Threading and Task Safety](#14-threading-and-task-safety)

---

## 1. High-Level System Overview

```
┌─────────────────────────────────────────────────────────┐
│                     Browser (SPA)                       │
│  Login ─── Dashboard ─── Settings ─── WiFi Config       │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP (Wi-Fi / Ethernet)
┌──────────────────────▼──────────────────────────────────┐
│                   ESP32 Firmware                        │
│                                                         │
│  ┌───────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │  Network  │  │  HTTP Server │  │  Device Control │  │
│  │ (ETH/WiFi │  │ (esp-idf     │  │  (GPIO + Ping   │  │
│  │  / AP)    │  │  httpd)      │  │   Watchdog)     │  │
│  └─────┬─────┘  └──────┬───────┘  └────────┬────────┘  │
│        │               │                   │            │
│  ┌─────▼───────────────▼───────────────────▼─────────┐  │
│  │           Storage (EEPROM / 24LC64)               │  │
│  └────────────────────────────────────────────────────┘  │
│                                                         │
│  ┌────────────────────────────────────────────────────┐  │
│  │           OLED Display (SSD1306 I²C)               │  │
│  └────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## 2. Source Tree

```
esp32Webserver/
│
├── Src/                               Firmware (C/C++ Arduino)
│   ├── esp32DeviceHub.ino             Main sketch
│   ├── webServer.cpp                  REST API handler hook implementations
│   ├── webServer.h                    Forward declarations (delegates to autoGen)
│   ├── webServerHelper.cpp            HTTP utility helpers
│   ├── webServerHelper.h              Helper declarations
│   ├── deviceConfig.h                 EEPROM map constants + Device struct
│   └── autoGen/                       ← AUTO-GENERATED (never edit manually)
│       ├── autoGenWebServer.h         Hook function declarations + webServerMacro enum
│       ├── autoGenWebServer.cpp       URI registration + HTTP handler wrappers
│       ├── autoGenHtmlData.h          HTML pages as gzip C byte arrays
│       ├── autoGenAssets.h            CSS/JS/images as gzip C byte arrays
│       ├── autoGenDeviceConfig.h      GPIO pin lists, max devices, storage config
│       └── autoGenOledLogo.h          Boot logo bitmap
│
├── webApp/                            Web front-end sources
│   ├── html/
│   │   ├── base.html                  SPA shell (header, modals, router script)
│   │   ├── login.html                 Login page
│   │   ├── dashboard.html             Device control page fragment
│   │   ├── settings.html              Settings page fragment
│   │   └── wifiConfig.html            Wi-Fi configuration page fragment
│   ├── assets/
│   │   ├── css/style.css              All styles (variables, dark/light theme, components)
│   │   ├── js/app.js                  API client (`api` object) and utility helpers
│   │   └── image/                     Logo and favicon images
│   ├── deviceConfig.yaml              GPIO and device configuration source
│   └── linkerData.yaml                HTTP route declarations
│
├── Scripts/                           Python build scripts
│   ├── common.py                      Shared paths and utility imports
│   ├── utility.py                     YAML reader, validator, naming helpers
│   ├── compileDeviceConfig.py         Generates autoGenDeviceConfig.h
│   ├── compileHtml.py                 Generates autoGenHtmlData.h
│   ├── compileAssets.py               Generates autoGenAssets.h
│   ├── updateWebServer.py             Generates autoGenWebServer.h/.cpp
│   └── compileOledLogo.py             Generates autoGenOledLogo.h
│
├── Libs/                              Third-party libraries (git submodules)
│   ├── Adafruit-GFX-Library/          Graphics primitives
│   ├── Adafruit_SSD1306/              SSD1306 OLED driver
│   ├── Adafruit_BusIO/                I²C/SPI bus abstraction
│   └── qrcode/                        QR code generator (C)
│
├── Makefile                           Build orchestration
├── requirements.txt                   Python package requirements
└── doc/                               Documentation (you are here)
```

---

## 3. Firmware Layers

```
┌─────────────────────────────────────────────────┐
│  Application Layer                              │
│  esp32DeviceHub.ino  ·  webServer.cpp           │
│  (device logic, EEPROM helpers, network mgmt)   │
├─────────────────────────────────────────────────┤
│  Auto-Generated Glue Layer                      │
│  autoGenWebServer.cpp  ·  autoGenWebServer.h    │
│  (URI registration, HTTP wrapper handlers)      │
├─────────────────────────────────────────────────┤
│  HTTP Server (ESP-IDF httpd)                    │
│  esp_http_server.h  ·  httpd_*  APIs            │
├─────────────────────────────────────────────────┤
│  Network Stack                                  │
│  ESP-IDF LwIP  ·  Arduino WiFi  ·  ETH (W5500)  │
├─────────────────────────────────────────────────┤
│  Hardware Abstraction                           │
│  Arduino-ESP32  ·  Wire/I²C  ·  SPI  ·  GPIO   │
└─────────────────────────────────────────────────┘
```

### Key Design Principle — Hook Pattern

The auto-generated `autoGenWebServer.cpp` handles all the HTTP boilerplate (creating `httpd_uri_t` structs, calling `httpd_register_uri_handler`, gzip-encoding headers, chunked sends). It calls *hook functions* declared in `autoGenWebServer.h` that are implemented in `webServer.cpp`. This separation means:

- Adding a new route is done entirely in `linkerData.yaml` (no C/C++ edits needed for the HTTP plumbing)
- Business logic stays in `webServer.cpp`, isolated from the generated scaffolding
- The generated files are always consistent with the route declaration

---

## 4. Boot Sequence

```
setup()
  │
  ├─ Serial.begin(115200)
  ├─ Derive AP SSID + password from eFuse MAC
  ├─ EEPROM.begin()  (if internal storage)
  ├─ Wire.begin()
  ├─ I²C scan: detect OLED (0x3C) and external EEPROM (0x50)
  ├─ loadControllerSettings()  ── reads name, timeout, OLED settings
  ├─ pinMode() for buttons (GPIO 4, 16) and LED (GPIO 33)
  ├─ display.begin() + applyOledSettings()
  ├─ Draw boot splash (2 s)
  ├─ loadAdminPasswordFromEEPROM()  ── or derive from MAC on first boot
  ├─ loadDevicesFromEEPROM()        ── restore GPIO states
  ├─ startNetwork()
  │     ├─ startEthernet()   ── W5500 SPI init, link wait, DHCP
  │     ├─ connectToWiFi()   ── if saved SSID exists
  │     └─ startAPMode()     ── fallback; starts DNS captive portal
  └─ startWebServer()        ── registers all URI handlers (auto-generated)
```

---

## 5. Main Loop

`loop()` runs continuously in the main Arduino task (FreeRTOS task). It performs no blocking I/O — every operation is either a fast check or a flag-based deferred action:

| Check | Purpose |
|---|---|
| `checkFactoryResetButton()` | Detect 10 s hold on GPIO 16; call `factoryResetSettings()` + schedule reboot |
| `checkOledPageButton()` | Debounced short-press on GPIO 4; advance OLED page |
| `checkEthernetAvailability()` | Promote Ethernet to preferred network if link + IP appears after boot |
| `checkWiFiAvailability()` | Periodically retry saved Wi-Fi if not connected (30 s interval) |
| `checkDeviceAutomation()` | Ping watchdog: send ICMP echo per device; power-cycle on failure |
| `dnsServer.processNextRequest()` | Only in AP mode; resolves all DNS to `192.168.4.1` (captive portal) |
| `eepromDirty` flag | Commits EEPROM from the correct main task (ESP-IDF NVS is not thread-safe) |
| `oledStatusDirty` flag | Redraws the OLED when device state changes (set by the httpd FreeRTOS task) |
| Auto-rotate OLED | Every 4 s, advance to the next page if the button isn't held |
| `rebootScheduled` flag | `ESP.restart()` after an 800 ms delay (allows HTTP response to flush) |

---

## 6. Network Subsystem

The firmware supports four network states represented by the `NetworkType` enum:

| State | Description |
|---|---|
| `EthernetDHCP` | W5500 connected with DHCP-assigned IP |
| `EthernetStatic` | W5500 connected with static fallback IP `192.168.4.1` |
| `WiFi` | Connected to a saved SSID via station mode |
| `AP` | Access point mode with captive DNS portal |

### Priority and Failover

1. **Ethernet is always tried first** at boot and promoted to preferred if it becomes available later
2. **Wi-Fi stays connected** even when Ethernet is the active path — it serves as an instant failover
3. **AP mode** is the last resort, activated when both Ethernet and Wi-Fi are unavailable
4. **Background reconnection** — `checkWiFiAvailability()` retries every 30 seconds if Wi-Fi is not connected

### Network Events

The `onNetworkEvent()` function handles `ARDUINO_EVENT_ETH_*` events from the ESP-IDF network stack. Key events:
- `ETH_CONNECTED` → sets `ethernetLinkConnected = true`
- `ETH_GOT_IP` → sets `ethernetGotIp = true`, triggers OLED refresh
- `ETH_DISCONNECTED` → triggers Ethernet link-loss handler

---

## 7. HTTP Web Server

The web server uses the **ESP-IDF native HTTP server** (`esp_http_server`), *not* the Arduino `WebServer` library. This provides lower overhead and better integration with FreeRTOS.

### Server Configuration

```cpp
config.max_req_hdr_len  = 4096;   // Mobile browser headers exceed 1 KB default
config.max_resp_headers = 16;
config.stack_size       = 12288;  // Increased for large response handling
config.task_priority    = 5;
config.max_open_sockets = 10;
config.max_uri_handlers = N + 5;  // N = total routes from linkerData.yaml
```

The HTTP server runs in its own FreeRTOS task (created internally by `httpd_start`). Handlers execute in this task context.

### Handler Types

| Type | Content-Type | Caching | Source |
|---|---|---|---|
| HTML page | `text/html` | `no-cache, no-store, must-revalidate` | gzip byte array |
| Static asset | varies (css, js, png…) | `public, max-age=31536000, immutable` + `ETag` | gzip byte array |
| API endpoint | `application/json` | `no-cache, no-store, must-revalidate` | `webServer.cpp` hook |

All content is served **gzip-encoded** (`Content-Encoding: gzip`). The browser decompresses on the fly. This dramatically reduces the size of embedded byte arrays (HTML/CSS/JS are typically 60–80% smaller).

### Chunked Response Helper

Large gzip payloads are sent in 4 KB chunks via `sendLargeResponse()` in [`Src/webServerHelper.cpp`](../Src/webServerHelper.cpp) to avoid exceeding the HTTP server's internal send buffer.

---

## 8. Autogen Pipeline (Code Generation)

This is the most distinctive part of the project architecture. Instead of manually maintaining HTTP routing boilerplate, the build pipeline generates it from declarative YAML.

### Flow

```
webApp/linkerData.yaml
        │
        ▼  Scripts/updateWebServer.py
        │
        ├── autoGenWebServer.h   ← hook function declarations + enum
        └── autoGenWebServer.cpp ← URI structs, handler wrappers, startWebServer()

webApp/deviceConfig.yaml
        │
        ▼  Scripts/compileDeviceConfig.py
        │
        └── autoGenDeviceConfig.h ← CFG_* macros, pin arrays

webApp/html/*.html
        │
        ▼  Scripts/compileHtml.py
        │
        └── autoGenHtmlData.h ← extern byte[] per HTML file (gzip)

webApp/assets/**
        │
        ▼  Scripts/compileAssets.py
        │
        └── autoGenAssets.h ← extern byte[] per asset (gzip)

Assets/logo.*
        │
        ▼  Scripts/compileOledLogo.py
        │
        └── autoGenOledLogo.h ← bootLogo[], BOOT_LOGO_W, BOOT_LOGO_H
```

For the full per-script reference see [build-system.md](build-system.md).

---

## 9. Storage Subsystem (EEPROM)

Storage is abstracted by three functions in `esp32DeviceHub.ino`:

```cpp
uint8_t storageRead(int address);
void    storageWrite(int address, uint8_t value);
void    storageCommit();                           // no-op for external EEPROM
```

These functions are compiled to either:

- **Internal** — Arduino `EEPROM` library (NVS-backed, `EEPROM_SIZE = 600` bytes)
- **External** — raw I²C byte-by-byte access to the 24LC64 at `0x50` (8 192 bytes)

The `CFG_STORAGE_EXTERNAL` macro (from `autoGenDeviceConfig.h`) selects the backend at compile time.

### EEPROM Memory Map

| Address | Size | Content |
|---|---|---|
| 0 – 31 | 32 B | Wi-Fi SSID |
| 100 – 163 | 64 B | Wi-Fi password |
| 164 – 195 | 32 B | Admin password |
| 200 | 1 B | Magic byte `0xA5` (validates EEPROM) |
| 210 | 1 B | Device count |
| 220 – 507 | 288 B | 8 × 36-byte device slots |
| 508 – 528 | 21 B | Controller name |
| 529 – 530 | 2 B | Logout minutes (little-endian) |
| 531 | 1 B | OLED brightness |
| 532 | 1 B | OLED enabled flag |
| 533 | 1 B | Admin password configured flag `0xA5` |

Each 36-byte device slot:

| Offset | Size | Field |
|---|---|---|
| 0 – 15 | 16 B | Name (null-terminated) |
| 16 | 1 B | GPIO pin number |
| 17 | 1 B | State (0=OFF, 1=ON) |
| 18 | 1 B | Automation enabled (0/1) |
| 19 | 1 B | Ping interval (seconds, 5–255) |
| 20 – 35 | 16 B | Ping target IP (dotted-decimal string) |

### Thread-Safety Note

`EEPROM.commit()` must be called from the same FreeRTOS task that called `EEPROM.begin()`, which is the main Arduino task. The HTTP server runs in a separate task. Therefore:

- HTTP handlers call `storageWrite()` freely (writes to the RAM buffer)
- HTTP handlers set `eepromDirty = true` instead of calling `storageCommit()`
- `loop()` calls `storageCommit()` when it sees `eepromDirty = true`

---

## 10. Authentication and Session Management

The server maintains **a single session slot** (one logged-in user at a time):

```cpp
static char sessionToken[33] = {0};   // 32-char hex + null
static unsigned long sessionLastActivity = 0;
```

### Token Lifecycle

1. **Login** (`POST /api/login`): password verified → `esp_random()` generates 32 hex chars → token stored in RAM
2. **Authorised request**: `X-Auth-Token` header compared with constant-time XOR; `sessionLastActivity` updated
3. **Idle timeout**: if `millis() - sessionLastActivity >= logoutMinutes * 60000`, token zeroed
4. **Logout** (`POST /api/logout`): token zeroed
5. **Reboot**: token lost (RAM only — intentional)

All protected endpoints call `isAuthorised(req)` before processing. Unauthenticated requests receive `401 Unauthorized` JSON.

---

## 11. Device Control and Ping Watchdog

### Device Struct

```cpp
struct Device {
    char    name[16];       // human-readable label
    uint8_t pin;            // GPIO output pin number
    uint8_t state;          // 0 = OFF, 1 = ON
    uint8_t autoEnabled;    // 0 = manual only, 1 = ping-watchdog active
    uint8_t pingInterval;   // seconds between pings (5–255)
    char    pingIp[16];     // target IP dotted-decimal
};
```

Devices are stored in the global array `devices[MAX_DEVICES]`. GPIO pins are configured as outputs at boot via `loadDevicesFromEEPROM()`.

### Ping Watchdog

`checkDeviceAutomation()` runs every loop iteration. For each device with `autoEnabled == 1` and `state == 1` (ON):

1. If `now - pingLastCheckedAt[i] >= pingInterval * 1000`: ping the target IP
2. Uses `esp_ping` (IDF native ICMP, works on Ethernet and Wi-Fi)
3. **If reply received**: no action
4. **If no reply**: turn the GPIO OFF → set `pingCyclingActive[i] = true` → after 5 s turn GPIO ON again

This implements automatic power-cycling for network devices (routers, IP cameras, etc.) that become unresponsive.

---

## 12. OLED Display

The OLED is driven by the Adafruit SSD1306 library over I²C.

### Page Logic

In normal (non-AP) mode, `updateOledDeviceStatus()` renders one of N pages:

- **Page 0** — network info (controller name, ETH IP, Wi-Fi IP, admin password hint, firmware version)
- **Page 1** — device summary (total, high/low-voltage counts, ON/OFF counts)
- **Pages 2+** — device detail list, 4 devices per page

The active page rotates automatically every 4 s when the page button is not held. A button short-press immediately advances to the next page.

In AP mode, `drawApQrPage()` and `drawApInfoPage()` are used instead, toggled by button press.

### Dirty Flag

`oledStatusDirty` is a `volatile bool` set by HTTP handlers whenever device state or settings change. `loop()` calls `updateOledDeviceStatus()` and clears the flag.

---

## 13. Data Flow Diagram

```
Browser                    ESP32 HTTP Server              Application
  │                              │                             │
  │── POST /api/login ──────────▶│                             │
  │                              │── apiLoginHandlerHook() ──▶│
  │                              │◀── { token: "..." } ───────│
  │◀── 200 { token: "..." } ─────│                             │
  │                              │                             │
  │── POST /api/devices/toggle ─▶│  (X-Auth-Token: ...)       │
  │                              │── isAuthorised() ──────────▶│
  │                              │── apiDevicesToggleHook() ──▶│
  │                              │                        digitalWrite()
  │                              │                        saveDevicesToEEPROM()
  │                              │                        oledStatusDirty = true
  │                              │◀── { success: true } ──────│
  │◀── 200 { success: true } ────│                             │
  │                              │                        loop(): updateOledDeviceStatus()
```

---

## 14. Threading and Task Safety

The firmware runs two concurrent FreeRTOS tasks:

| Task | Created by | Responsibilities |
|---|---|---|
| Main (Arduino loop) | Arduino runtime | `loop()`, EEPROM commits, OLED updates, button checks, automation, DNS |
| HTTP server | `httpd_start()` | Handling incoming HTTP requests, calling hook functions |

### Shared State and Guards

| Variable | Type | Access Pattern |
|---|---|---|
| `devices[]` | struct array | Written by httpd task; read by both tasks. Updates are infrequent and atomic at field level; no mutex used (array updates complete before the next `loop()` check). |
| `deviceCount` | `uint8_t` | Written by httpd task; read by both |
| `eepromDirty` | `volatile bool` | Set by httpd; cleared by loop after commit |
| `oledStatusDirty` | `volatile bool` | Set by httpd; cleared by loop after redraw |
| `rebootScheduled` | `volatile bool` | Set by httpd; acted on by loop |
| `sessionToken[]` | `char[33]` | Written only by httpd task |

> All `volatile` flags use the C `volatile` keyword to prevent the compiler from caching reads. For the `devices[]` array, the risk of tearing is low because the httpd task writes a single slot at a time and the loop task only reads for display purposes, but production use with safety-critical loads should add a mutex.
