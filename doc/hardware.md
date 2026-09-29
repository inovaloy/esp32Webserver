# Hardware Reference

This document covers all hardware components used by the ESP32 Device Hub, their wiring, pin assignments, and electrical notes.

---

## Table of Contents

1. [Bill of Materials](#1-bill-of-materials)
2. [ESP32 Pin Assignment Summary](#2-esp32-pin-assignment-summary)
3. [W5500 Ethernet Module](#3-w5500-ethernet-module)
4. [SSD1306 OLED Display](#4-ssd1306-oled-display)
5. [External EEPROM (AT24C64 / 24LC64)](#5-external-eeprom-at24c64--24lc64)
6. [Control Buttons](#6-control-buttons)
7. [Status LED](#7-status-led)
8. [GPIO Device Outputs (Relays / Signal Lines)](#8-gpio-device-outputs-relays--signal-lines)
9. [Full Wiring Diagram (ASCII)](#9-full-wiring-diagram-ascii)
10. [Power Supply Notes](#10-power-supply-notes)
11. [I²C Bus Notes](#11-i2c-bus-notes)

---

## 1. Bill of Materials

| # | Component | Spec / Notes | Required |
|---|---|---|---|
| 1 | ESP32 development board | ESP32-WROOM-32 or WROVER; ≥ 4 MB flash | **Yes** |
| 2 | W5500 SPI Ethernet module | 3.3V-compatible SPI; 100BASE-TX | Optional (enables wired network) |
| 3 | SSD1306 OLED display | 128 × 64 pixels; I²C; 3.3 V | Optional (provides status display) |
| 4 | AT24C64 / 24LC64 EEPROM | 8 KB; I²C address 0x50; 3.3 V | Optional (external persistent storage) |
| 5 | 2 × momentary push-button | Normally open; any rating | Optional (factory reset + OLED page) |
| 6 | LED + 220–470 Ω resistor | 3 mm or 5 mm through-hole | Optional (status indicator) |
| 7 | Relay module(s) | 3.3V-logic input; any mains rating | For high-voltage device control |
| 8 | USB-to-serial cable or dev board USB | CP2102/CH340/FTDI compatible | **Yes** (for flashing) |
| 9 | 3.3 V power supply or dev board | ≥ 500 mA recommended with Ethernet | **Yes** |

---

## 2. ESP32 Pin Assignment Summary

| GPIO | Direction | Function | Notes |
|---|---|---|---|
| **4** | Input (PULLUP) | OLED page button | Active LOW; debounced in firmware |
| **5** | Output | Default low-voltage device pin | Can be reassigned in `deviceConfig.yaml` |
| **14** | Output (SPI) | W5500 SPI Chip Select (CS) | |
| **15** | Output | Default low-voltage device pin | Can be reassigned |
| **16** | Input (PULLUP) | Factory reset button | Hold ≥ 10 s; active LOW |
| **17** | Output | Default low-voltage device pin | |
| **18** | Output (SPI) | W5500 SPI Clock (SCLK) | VSPI SCK |
| **19** | Input (SPI) | W5500 SPI MISO | VSPI MISO |
| **21** | Bidirectional (I²C) | I²C SDA | Shared: OLED + external EEPROM |
| **22** | Output (I²C) | I²C SCL | Shared: OLED + external EEPROM |
| **23** | Output (SPI) | W5500 SPI MOSI | VSPI MOSI |
| **25** | Output | Default high-voltage device pin | Can be reassigned |
| **26** | Output | Default high-voltage device pin | Can be reassigned |
| **27** | Output | Default high-voltage device pin | Can be reassigned |
| **32** | Output | Default low-voltage device pin | |
| **33** | Output | Status LED + default device pin | Blinks during Wi-Fi connection; HIGH when connected |

> **Note:** GPIOs 6–11 are connected to the internal SPI flash on most ESP32 modules and **must not be used**.

---

## 3. W5500 Ethernet Module

The W5500 is a hardwired TCP/IP controller with an SPI interface. It provides 100BASE-TX Ethernet connectivity independent of the ESP32's Wi-Fi radio.

### Wiring Table

| W5500 Pin | ESP32 GPIO | Signal | Notes |
|---|---|---|---|
| SCLK | 18 | SPI clock | VSPI peripheral |
| MISO | 19 | Master-in slave-out | VSPI peripheral |
| MOSI | 23 | Master-out slave-in | VSPI peripheral |
| CS / SCS | 14 | Chip select (active LOW) | GPIO 5 reserved; GPIO 14 used |
| GND | GND | Ground | |
| 3.3V | 3.3V | Power | Do not use 5V; W5500 is 3.3V |
| INT | — | Interrupt | Not connected; firmware uses polling |
| RST | — | Hardware reset | Not connected; module self-resets |

### Driver Notes

- Uses ESP-Arduino `ETH.begin(ETH_PHY_W5500, ...)` with `SPI2_HOST` (VSPI)
- **DHCP timeout:** 5 seconds; falls back to static `192.168.4.1/24`
- **Link detection timeout:** 5 seconds
- Hostname set to `esp32-device-hub` via `ETH.setHostname()`
- Ethernet is preferred over Wi-Fi when both are available

### Direct Laptop Connection (Static Fallback)

If no DHCP server is available (e.g., plugged directly into a laptop):

1. The ESP32 assigns itself `192.168.4.1/24`
2. Configure your laptop Ethernet adapter: IP `192.168.4.2`, mask `255.255.255.0`, no gateway
3. Browse to `http://192.168.4.1`

---

## 4. SSD1306 OLED Display

The SSD1306 is a 128 × 64 monochrome OLED display driven over I²C.

### Wiring Table

| OLED Pin | ESP32 GPIO | Signal |
|---|---|---|
| SDA | 21 | I²C data |
| SCL | 22 | I²C clock |
| GND | GND | Ground |
| VCC | 3.3V | Power |

### Configuration

| Parameter | Value | Constant |
|---|---|---|
| I²C address | `0x3C` | `OLED_I2C_ADDRESS` |
| Display width | 128 px | `SCREEN_WIDTH` |
| Display height | 64 px | `SCREEN_HEIGHT` |
| Reset pin | None (shared with MCU reset) | `OLED_RESET = -1` |
| Driver library | Adafruit SSD1306 v2.5.15 | `Libs/Adafruit_SSD1306/` |

### Alternative I²C Address

Some modules use `0x3D` instead of `0x3C` (determined by a solder bridge on the PCB). If the OLED does not initialise, check this and update `OLED_I2C_ADDRESS` in [`Src/esp32DeviceHub.ino`](../Src/esp32DeviceHub.ino).

### Display Pages

See [architecture.md — OLED Display](architecture.md#12-oled-display) for the full page-layout reference.

---

## 5. External EEPROM (AT24C64 / 24LC64)

The external EEPROM provides 8 KB of non-volatile storage on the I²C bus. It is optional — the firmware can use the ESP32's internal flash EEPROM emulation instead.

### Wiring Table

| EEPROM Pin | ESP32 GPIO | Signal | Notes |
|---|---|---|---|
| SDA | 21 | I²C data | Shared with OLED |
| SCL | 22 | I²C clock | Shared with OLED |
| GND | GND | Ground | |
| VCC | 3.3V | Power | |
| A0 | GND | Address bit 0 | Sets address to 0x50 |
| A1 | GND | Address bit 1 | Sets address to 0x50 |
| A2 | GND | Address bit 2 | Sets address to 0x50 |
| WP | GND | Write protect | Connect to GND to enable writes |

With all address pins tied to GND, the I²C address is `0x50`. This is the value used by the firmware (`EXTERNAL_EEPROM_I2C_ADDRESS = 0x50`).

### Enabling External EEPROM

In `webApp/deviceConfig.yaml`:

```yaml
storageBackend: external
```

Run `make autogen` and rebuild to activate the external EEPROM driver. The firmware detects the device at boot and prints a status message on serial.

### Internal vs External Storage

| Feature | Internal (flash) | External (24LC64) |
|---|---|---|
| Capacity | 600 bytes (configured) | 8 192 bytes |
| Endurance | ~10 000 write cycles per page | ~1 000 000 write cycles per byte |
| Extra hardware | None | 24LC64 module + I²C wiring |
| Thread safety | Requires main-task commit | Byte-level; no `commit()` needed |

The internal flash EEPROM emulation is backed by the ESP-IDF NVS partition and requires `EEPROM.commit()` to be called from the same task as `EEPROM.begin()` (the main Arduino task).

---

## 6. Control Buttons

Both buttons are connected with internal pull-up resistors enabled (`INPUT_PULLUP`). They are active LOW — pressing the button connects the pin to GND.

### Factory Reset Button (GPIO 16)

| Property | Value |
|---|---|
| GPIO | 16 |
| Mode | `INPUT_PULLUP` |
| Active state | LOW (pressed) |
| Action trigger | Hold for 10 000 ms (`FACTORY_RESET_HOLD_TIME`) |
| Effect | Calls `factoryResetSettings()`, schedules reboot |

**Wiring:**
```
GPIO 16 ──┬── [Button] ── GND
          └── (internal 47 kΩ pull-up to 3.3V)
```

> Do not press and release quickly — the firmware requires a **sustained hold** of 10 seconds to prevent accidental resets.

### OLED Page Button (GPIO 4)

| Property | Value |
|---|---|
| GPIO | 4 |
| Mode | `INPUT_PULLUP` |
| Active state | LOW (pressed) |
| Debounce | 30 ms (`OLED_PAGE_BUTTON_DEBOUNCE_MS`) |
| Short press (< 800 ms) | Advance OLED page / toggle QR↔info in AP mode |
| Hold (≥ 800 ms) | No special action currently |

**Wiring:**
```
GPIO 4 ──┬── [Button] ── GND
         └── (internal 47 kΩ pull-up to 3.3V)
```

---

## 7. Status LED

A standard LED with a current-limiting resistor on GPIO 33.

| Property | Value |
|---|---|
| GPIO | 33 |
| Mode | `OUTPUT` |
| Behaviour | Blinks at 500 ms during Wi-Fi connect attempt; solid HIGH when connected; LOW otherwise |

**Wiring:**
```
GPIO 33 ──[220–470 Ω]──[LED Anode]──[LED Cathode]── GND
```

> GPIO 33 is also listed as a low-voltage device pin in the default `deviceConfig.yaml`. Adding a device on pin 33 will share the pin with the status LED behaviour. If this is undesirable, remove 33 from `lowVoltageGpioPins`.

---

## 8. GPIO Device Outputs (Relays / Signal Lines)

Device GPIO pins are configured as **push-pull outputs** (`OUTPUT`) and driven `HIGH` (ON) or `LOW` (OFF) by the firmware.

### High-Voltage Device Pins (Default: 26, 27, 25)

Intended for **relay module** inputs that switch mains-voltage (AC) loads:

```
GPIO 26 ──▶ Relay module IN1 ──▶ (relay coil switches 240VAC load)
GPIO 27 ──▶ Relay module IN2
GPIO 25 ──▶ Relay module IN3
```

Most 5V relay modules accept a 3.3V logic signal. Check the relay module's datasheet — some require the IN pin to be pulled to VCC via a transistor if 3.3V drive is insufficient.

> ⚠️ **Safety warning:** Mains-voltage wiring must comply with local electrical codes and should be performed by a qualified electrician. Always ensure the relay module is rated for the load current and voltage.

### Low-Voltage Device Pins (Default: 33, 32, 17, 15, 5)

Intended for **signal-level outputs**: LED strips (via MOSFET driver), fans (via driver board), small DC loads, indicator lights, etc.

```
GPIO 32 ──▶ MOSFET gate / LED driver input / relay coil
```

These pins source a maximum of ~40 mA each and should not directly drive high-current loads.

---

## 9. Full Wiring Diagram (ASCII)

```
                         ┌─────────────────────────┐
                         │         ESP32            │
   Factory Reset ────────┤ GPIO 16     GPIO 18 ─────┼──── W5500 SCLK
   OLED Page Btn ────────┤ GPIO 4      GPIO 19 ─────┼──── W5500 MISO
   Status LED ───────────┤ GPIO 33     GPIO 23 ─────┼──── W5500 MOSI
                         │             GPIO 14 ─────┼──── W5500 CS
   Device Out 1 ─────────┤ GPIO 26                  │
   Device Out 2 ─────────┤ GPIO 27     GPIO 21 ─────┼──┬─ OLED SDA
   Device Out 3 ─────────┤ GPIO 25     GPIO 22 ─────┼──┤─ OLED SCL
   Device Out 4 ─────────┤ GPIO 32                  │  ├─ EEPROM SDA
   Device Out 5 ─────────┤ GPIO 17                  │  └─ EEPROM SCL
   Device Out 6 ─────────┤ GPIO 15                  │
   Device Out 7 ─────────┤ GPIO 5                   │
                         │             3.3V ─────────┼──── W5500 VCC
                         │             GND ──────────┼──── W5500 GND
                         │             3.3V ─────────┼──── OLED VCC
                         │             GND ──────────┼──── OLED GND
                         │             3.3V ─────────┼──── EEPROM VCC
                         │             GND ──────────┼──── EEPROM GND / A0-A2
                         └─────────────────────────┘
```

---

## 10. Power Supply Notes

- **All logic signals are 3.3V** — the ESP32, W5500, SSD1306, and 24LC64 all operate at 3.3V
- **Total current budget** (approximate):
  - ESP32 active with Wi-Fi: ~240 mA peak
  - W5500 with active link: ~140 mA peak
  - SSD1306 OLED: ~20 mA
  - External EEPROM: negligible
  - Total peak: ~400 mA
- Use a **regulated 3.3V supply rated ≥ 500 mA** or power via the ESP32 dev board's USB (typical dev boards include a 500 mA–1 A LDO regulator)
- Relay module coils typically require 5V (separate supply); logic signals (IN pins) remain 3.3V compatible

---

## 11. I²C Bus Notes

The OLED display and external EEPROM share the same I²C bus (GPIO 21 = SDA, GPIO 22 = SCL):

| Device | I²C Address | Speed |
|---|---|---|
| SSD1306 OLED | `0x3C` (or `0x3D`) | 400 kHz (fast mode) |
| AT24C64 EEPROM | `0x50` | 400 kHz (fast mode) |

At boot, the firmware scans both addresses with `Wire.beginTransmission()` and prints the result on the serial monitor:

```
[I2C] OLED (0x3C): FOUND
[I2C] External EEPROM 24LC64 (0x50): FOUND
```

**Pull-up resistors:** The I²C bus requires pull-ups on SDA and SCL. Most SSD1306 breakout boards and EEPROM modules include 4.7 kΩ pull-ups on the board. If you are wiring the bare ICs, add a 4.7 kΩ resistor from each line to 3.3V.
