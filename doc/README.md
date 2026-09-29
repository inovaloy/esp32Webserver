# ESP32 Device Hub — Documentation

> A full-featured IoT controller running on an ESP32 with a browser-based web interface, dual-network support, OLED status display, and a Python-driven code-generation build pipeline.

---

## Table of Contents

### 👤 For Users

| Document | Purpose |
|---|---|
| [user-guide.md](user-guide.md) | First boot, logging in, the dashboard, and everyday tasks |
| [device-management.md](device-management.md) | Add, rename, remove devices; set up the ping watchdog |
| [network-setup.md](network-setup.md) | Connect to Wi-Fi, use Ethernet, understand AP mode and failover |
| [factory-reset.md](factory-reset.md) | When and how to reset — web interface and physical button methods |
| [api-examples.md](api-examples.md) | Python scripts and `curl` commands for programmatic control |

### 🛠 For Developers / Installers

| Document | Purpose |
|---|---|
| **README.md** *(this file)* | Project overview, feature summary, and navigation guide |
| [getting-started.md](getting-started.md) | Hardware requirements, environment setup, build & flash instructions |
| [architecture.md](architecture.md) | System design, source tree layout, firmware layers, autogen pipeline |
| [configuration.md](configuration.md) | `deviceConfig.yaml` and `linkerData.yaml` reference |
| [api-reference.md](api-reference.md) | Complete REST API documentation with request/response examples |
| [web-interface.md](web-interface.md) | Web UI pages, SPA router, theming, and feature walkthroughs |
| [hardware.md](hardware.md) | Wiring diagrams, pin assignments, W5500 Ethernet, OLED, EEPROM |
| [build-system.md](build-system.md) | Python autogen scripts, Makefile targets, and code generation details |

---

## What Is This Project?

**ESP32 Device Hub** is an open-source firmware project that turns an ESP32 microcontroller into a smart home/lab device controller with a fully self-hosted web interface. No cloud, no third-party services—everything runs on the chip itself.

You can control GPIO outputs (relays, LED drivers, low-voltage signals), monitor and configure network settings, receive OLED status updates, and manage the device entirely from any modern browser on your local network.

The project uses a **Python build pipeline** to compile HTML, CSS, JavaScript, and images into gzip-compressed C arrays that are embedded directly in the firmware image. Routes, pages, and API endpoints are declared in a single YAML file; the corresponding C/C++ handler boilerplate is auto-generated—you only write the business logic.

---

## Key Features

### Connectivity
- **W5500 SPI Ethernet** — tried first at boot; DHCP with automatic static-IP fallback (`192.168.4.1/24`)
- **Wi-Fi Station mode** — connects to a saved SSID/passphrase stored in EEPROM
- **Automatic failover** — Ethernet → Wi-Fi → Access Point, with background reconnection
- **Access Point (captive portal)** — appears when neither Ethernet nor Wi-Fi is available; shows a QR code on the OLED

### Web Interface
- **Single-page application (SPA)** — hash-based router, no full-page reloads
- **Dashboard** — add/remove/rename/toggle GPIO devices; per-card ON/OFF switch
- **Settings** — controller name, session timeout, OLED brightness, password management, network scan & connect, backup/restore, firmware OTA, factory reset
- **Login page** — token-based authentication; session expires after a configurable idle timeout
- **Dark/light theme + accent colour** — stored in `localStorage`, applied before first paint

### Device Control & Automation
- Up to **8 configurable GPIO devices** (limit adjustable in `deviceConfig.yaml`, max 16)
- Devices split into **high-voltage** and **low-voltage** pin groups
- **Ping watchdog (automation)** — the ESP32 pings a target IP on a configurable interval; if no reply is received the device is power-cycled (5 s off → on)
- State and configuration persisted to **EEPROM** (internal flash emulation or external AT24/24LC64 via I²C)

### OLED Display
- 128 × 64 SSD1306 display connected over I²C
- **Boot splash** — logo + "Device Hub" text with firmware version
- **Multi-page status** — rotates automatically every 4 s; physical button advances pages
  - Page 0: controller name, Ethernet IP, Wi-Fi IP, firmware version
  - Page 1: device summary (total, high-voltage, low-voltage, ON/OFF counts)
  - Page 2+: device name + state list (4 devices per page)
- **AP mode pages** — QR code (default) and connect-info page toggled by button press

### Security
- Session token (32-character hex, generated with `esp_random`); sent as `X-Auth-Token` header
- Configurable inactivity logout (1–1440 minutes)
- Admin password stored in EEPROM; default derived from device MAC; change forced on first login
- Constant-time password comparison (XOR accumulator)
- Factory reset requires admin password confirmation

### Build Pipeline
- **YAML-based routing** — `webApp/linkerData.yaml` declares all HTTP routes; Python scripts generate the C++ handler stubs and registration code
- **HTML/asset compilation** — HTML fragments and static assets are gzip-compressed and embedded as C byte arrays
- **Device config generation** — `webApp/deviceConfig.yaml` is compiled to `autoGenDeviceConfig.h` (GPIO lists, max devices, storage backend)
- **OLED logo** — bitmap logo compiled to a C array from source image

### OTA Firmware Update
- Upload a new `.bin` from the browser (drag-and-drop or file picker)
- Raw binary streamed with `XHR`; server validates the ESP32 image header (`0xE9`) before committing
- Device reboots automatically after a successful update

---

## Project Structure (top level)

```
esp32Webserver/
├── Src/                    Firmware source (Arduino sketch + C++ modules)
│   ├── esp32DeviceHub.ino  Main sketch — setup(), loop(), network, EEPROM helpers
│   ├── webServer.cpp/.h    API handler hook implementations
│   ├── webServerHelper.cpp Chunked HTTP send helpers
│   ├── deviceConfig.h      EEPROM map constants and Device struct
│   └── autoGen/            Auto-generated files (do not edit manually)
├── webApp/                 Web application sources
│   ├── html/               HTML page fragments (base, login, dashboard, settings, wifi)
│   ├── assets/             CSS, JS, images
│   ├── linkerData.yaml     HTTP route declarations
│   └── deviceConfig.yaml   GPIO and storage configuration
├── Scripts/                Python build scripts
├── Libs/                   Git submodule libraries (Adafruit SSD1306, GFX, BusIO, qrcode)
├── doc/                    Project documentation (you are here)
├── Makefile                Build targets (autogen, build, flash, configure, clean)
└── requirements.txt        Python dependencies (PyYAML, Pillow, …)
```

---

## Quick Start (summary)

> Full instructions are in [getting-started.md](getting-started.md).

```bash
# 1. Install Python dependencies
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt

# 2. Configure your GPIO pins and device limits
nano webApp/deviceConfig.yaml

# 3. Run the autogen pipeline (compiles HTML/CSS/JS → C arrays)
make autogen

# 4. Build and flash (requires Arduino-ESP32 toolchain in .temp/)
make configure   # first time only
make flash
```

After flashing, the device boots and either:
- connects to a saved Wi-Fi network, or
- starts AP mode (`ESP32-XXXX` / password shown on OLED) and shows a captive portal

Browse to the IP shown on the OLED (or `192.168.4.1` in AP mode) to open the web interface.

---

## Supported Hardware

| Component | Details |
|---|---|
| MCU | ESP32 (any variant with ≥ 4 MB flash) |
| Ethernet | W5500 SPI module (optional but strongly recommended for reliability) |
| Display | SSD1306 128×64 OLED (I²C, address `0x3C`) |
| Storage | Internal ESP32 flash EEPROM emulation *or* external AT24C64 / 24LC64 (I²C, `0x50`) |
| Factory reset button | Momentary push-button on GPIO 16, active LOW, hold 10 s |
| OLED page button | Momentary push-button on GPIO 4, active LOW |

---

## Version

Current firmware version: **1.0.1**

See the individual documentation files for deep-dives into each subsystem.
