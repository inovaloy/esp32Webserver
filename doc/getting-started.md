# Getting Started

This guide covers everything you need to go from a bare ESP32 to a running ESP32 Device Hub: hardware wiring, software environment setup, configuration, building, and flashing.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Clone the Repository](#2-clone-the-repository)
3. [Install Python Dependencies](#3-install-python-dependencies)
4. [Hardware Assembly](#4-hardware-assembly)
5. [Configure the Project](#5-configure-the-project)
6. [Run the Autogen Pipeline](#6-run-the-autogen-pipeline)
7. [Build the Firmware](#7-build-the-firmware)
8. [Flash the Device](#8-flash-the-device)
9. [First Boot](#9-first-boot)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Prerequisites

### Hardware

| Item | Notes |
|---|---|
| ESP32 development board | Any variant with ≥ 4 MB flash; WROOM-32 or WROVER work well |
| W5500 SPI Ethernet module | Optional; enables wired networking |
| SSD1306 128×64 OLED | I²C, address `0x3C` |
| AT24C64 / 24LC64 EEPROM | Optional; I²C external EEPROM at address `0x50` |
| 2 × momentary push-buttons | Factory reset (GPIO 16) and OLED page (GPIO 4) |
| USB-to-serial cable | For flashing and serial monitor |
| Breadboard / PCB, jumper wires | Prototyping |

### Software

| Tool | Version / Notes |
|---|---|
| Python 3 | 3.9 or newer |
| Git | Any recent version |
| Arduino-ESP32 toolchain | Fetched automatically by `make configure` |
| GNU Make | macOS: `brew install make`; Linux: usually pre-installed |

> **macOS note:** The default `make` on macOS is very old. Install GNU Make via Homebrew: `brew install make` and use `gmake` instead of `make`, or add `/opt/homebrew/opt/make/libexec/gnubin` to your `PATH`.

---

## 2. Clone the Repository

```bash
git clone --recurse-submodules https://github.com/your-org/esp32Webserver.git
cd esp32Webserver
```

If you already cloned without `--recurse-submodules`, initialise the library submodules now:

```bash
git submodule update --init --recursive
```

The submodules populate `Libs/`:

```
Libs/Adafruit-GFX-Library/   v1.12.3
Libs/Adafruit_SSD1306/        v2.5.15
Libs/Adafruit_BusIO/          v1.17.4
Libs/qrcode/                  (master)
```

---

## 3. Install Python Dependencies

Create a virtual environment (strongly recommended):

```bash
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install -r requirements.txt
```

The `requirements.txt` includes at minimum:

- **PyYAML** — parses `linkerData.yaml` and `deviceConfig.yaml`
- **Pillow** — used by `compileOledLogo.py` to process the boot logo image

Verify:

```bash
.venv/bin/python3 -c "import yaml, PIL; print('OK')"
```

---

## 4. Hardware Assembly

### Minimum Setup (Wi-Fi only, no OLED)

You can flash and run the firmware with just the bare ESP32. Wi-Fi credentials are configured through the captive portal at first boot.

### Full Setup Wiring

#### W5500 Ethernet (SPI)

| W5500 Pin | ESP32 GPIO | Notes |
|---|---|---|
| SCLK | GPIO 18 | SPI clock (VSPI SCK) |
| MISO | GPIO 19 | SPI master-in (VSPI MISO) |
| MOSI | GPIO 23 | SPI master-out (VSPI MOSI) |
| CS / SCS | GPIO 14 | Chip select |
| GND | GND | Common ground |
| 3.3V | 3.3V | Power |
| INT | — | Not used (polled driver) |
| RST | — | Not used |

> **GPIO 5** is reserved by the default application device configuration. GPIO 16 is the factory-reset button. Both are unavailable for W5500 signals.

#### SSD1306 OLED (I²C)

| OLED Pin | ESP32 | Notes |
|---|---|---|
| SDA | GPIO 21 | Default Arduino I²C SDA |
| SCL | GPIO 22 | Default Arduino I²C SCL |
| GND | GND | |
| VCC | 3.3V | |

I²C address: `0x3C` (typical for 128×64 modules; solder bridge may change to `0x3D`)

#### External EEPROM AT24C64 / 24LC64 (I²C, optional)

| EEPROM Pin | ESP32 | Notes |
|---|---|---|
| SDA | GPIO 21 | Shared with OLED |
| SCL | GPIO 22 | Shared with OLED |
| GND | GND (A0, A1, A2 to GND) | Sets I²C address to `0x50` |
| VCC | 3.3V | |

#### Control Buttons

| Button | GPIO | Active | Behaviour |
|---|---|---|---|
| Factory reset | GPIO 16 | LOW (INPUT_PULLUP) | Hold ≥ 10 seconds to erase all settings |
| OLED page | GPIO 4 | LOW (INPUT_PULLUP) | Short press: advance page / toggle QR/info in AP mode |

#### Status LED (GPIO 33)

The firmware drives GPIO 33 as a status LED:
- Blinks during Wi-Fi connection attempt
- Solid HIGH when Wi-Fi is connected

---

## 5. Configure the Project

### GPIO and Device Configuration — `webApp/deviceConfig.yaml`

Edit this file to declare which GPIO pins are available, how many devices can be added, and which storage backend to use:

```yaml
maxDevices: 8          # 1–16; EEPROM reserves space for this many slots

storageBackend: external   # "internal" (flash) or "external" (24LC64 at 0x50)

highVoltageGpioPins:   # GPIOs for relay / mains-switched loads
  - 26
  - 27
  - 25

lowVoltageGpioPins:    # GPIOs for signal-level outputs (LED strips, fans, etc.)
  - 33
  - 32
  - 17
  - 15
  - 5
```

> After editing `deviceConfig.yaml`, always re-run `make autogen` before building.

For the full reference see [configuration.md](configuration.md).

### Route Configuration — `webApp/linkerData.yaml`

This file declares every HTTP route. You normally only edit it when adding a new page or API endpoint. See [configuration.md](configuration.md) for full syntax.

---

## 6. Run the Autogen Pipeline

```bash
make autogen
```

This runs five Python scripts in sequence:

| Script | Output |
|---|---|
| `compileDeviceConfig.py` | `Src/autoGen/autoGenDeviceConfig.h` — GPIO arrays, max devices, storage backend |
| `compileHtml.py` | `Src/autoGen/autoGenHtmlData.h` — gzip-compressed HTML as C byte arrays |
| `compileAssets.py` | `Src/autoGen/autoGenAssets.h` — gzip-compressed CSS/JS/images as C byte arrays |
| `updateWebServer.py` | `Src/autoGen/autoGenWebServer.h` + `.cpp` — URI handlers and `startWebServer()` |
| `compileOledLogo.py` | `Src/autoGen/autoGenOledLogo.h` — boot logo bitmap as a C array |

All generated files live in `Src/autoGen/`. **Do not edit them manually**; they are overwritten on every `make autogen`.

---

## 7. Build the Firmware

### One-time toolchain setup

Downloads the Arduino-ESP32 core and `makeEspArduino` build wrapper into `.temp/`:

```bash
make configure
```

This step requires an internet connection and can take several minutes.

### Compile

```bash
make build
```

The compiled firmware binary is written to `build/`.

---

## 8. Flash the Device

Connect the ESP32 to your computer via USB, then:

```bash
make flash
```

This builds (if needed) and uploads to `/dev/ttyUSB0` at 460800 baud. To use a different port, edit the `Makefile` or pass the port inline:

```bash
cd Src && make flash UPLOAD_PORT=/dev/tty.usbserial-0001 ...
```

### Erase + Flash (first time or after another firmware)

If the ESP32 had a different firmware with a different partition table, erase the entire flash first:

```bash
make erase_and_flash
```

---

## 9. First Boot

1. **Power on** — the OLED shows the boot splash (logo + "Device Hub" + firmware version) for ~2 seconds.

2. **Network connection attempt** — the firmware tries in order:
   - **W5500 Ethernet** (5 s link timeout, then 5 s DHCP timeout)
   - **Saved Wi-Fi** (reads SSID/password from EEPROM; 20 s timeout)
   - **AP Mode** (fallback when both fail)

3. **AP Mode on first boot** — because no Wi-Fi is saved, the device starts an access point:
   - **SSID:** `ESP32-XXXX` (last 4 MAC hex digits)
   - **Password:** 8-character hex string derived from the MAC
   - **IP:** `192.168.4.1`
   - The OLED shows a QR code on the left and the credentials on the right.
   - Press the page button to switch to the text info screen.

4. **Connect to the AP** — scan for `ESP32-XXXX`, enter the password shown on the OLED, then navigate to `http://192.168.4.1` in your browser.

5. **Login** — the default admin password is the same 8-character hex string shown on the OLED (same as the AP password, derived from the MAC). After first login you are prompted to set a new password.

6. **Configure Wi-Fi** — go to **Settings → Network**, scan, select your SSID, enter your password, and click **Connect**. The device saves the credentials and reboots.

After the reboot the device connects to your Wi-Fi. The OLED shows the assigned IP address. Browse to that IP to access the dashboard.

---

## 10. Troubleshooting

### OLED not initialising

```
SSD1306 allocation failed
```

Check wiring (SDA/SCL, power) and confirm the I²C address is `0x3C`. If the display uses `0x3D`, update `OLED_I2C_ADDRESS` in [`Src/esp32DeviceHub.ino`](../Src/esp32DeviceHub.ino).

### External EEPROM not found

The serial monitor prints:

```
[I2C] External EEPROM 24LC64 (0x50): NOT FOUND
[STORAGE] External EEPROM selected but not found; storage is unavailable
```

Either check the hardware wiring or switch `storageBackend` to `internal` in `deviceConfig.yaml`.

### W5500 not detected

```
[ETH] W5500 initialization failed
```

Verify SPI wiring (GPIO 18/19/23/14) and that the module is powered (3.3 V).

### Device not reachable after Ethernet DHCP timeout

If DHCP times out while a physical link is present (e.g., direct laptop connection), the device falls back to static `192.168.4.1/24`. Configure your laptop's Ethernet adapter to `192.168.4.2 / 255.255.255.0` and browse to `http://192.168.4.1`.

### Build fails: Python script errors

Ensure you are using the `.venv` Python, not the system Python:

```bash
.venv/bin/python3 Scripts/compileHtml.py
```

### Flash permission error (Linux)

Add yourself to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
# log out and back in
```
