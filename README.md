# ESP32 Device Hub

An ESP32-based device hub with a web interface for GPIO control, WiFi configuration, OLED status, persistent settings, and factory reset support.

## First-time setup and build

### Prerequisites

- ESP32 development board and a USB data cable. An OLED, external EEPROM, and W5500 Ethernet module are optional.
- Git, GNU Make, and Python 3.9 or newer.
- A computer with internet access for the initial toolchain download.

On Linux, GNU Make is usually already installed. On macOS, install it with `brew install make`; use `gmake` in place of `make` in the commands below.

### 1. Clone the repository

Clone the source repository:

```bash
git clone https://github.com/inovaloy/esp32Webserver.git
cd esp32Webserver
```

Initialize the library submodules and check out the versions pinned by this repository:

```bash
git submodule update --init --recursive
```

### 2. Install the Python build dependencies

The build scripts use a virtual environment in `.venv/`:

```bash
python3 -m venv .venv
.venv/bin/python3 -m pip install --upgrade pip
.venv/bin/python3 -m pip install -r requirements.txt
```

### 3. Configure the firmware

Review `webApp/deviceConfig.yaml` and adjust available GPIO pins, device count, and storage backend for your hardware. The default route definitions and web application are in `webApp/linkerData.yaml`, `webApp/html/`, and `webApp/assets/`.

### 4. Download the ESP32 build toolchain

Run this once per checkout. It downloads the Arduino-ESP32 core and `makeEspArduino` into `.temp/`, initializes the Arduino core's own Git submodules, and requires an internet connection. If the Arduino core is already present, rerunning this target updates its submodules; it also pulls updates for the existing `makeEspArduino` checkout:

```bash
make configure
```

### 5. Build the firmware

```bash
make build
```

This regenerates the C/C++ files in `src/autoGen/` from the web app and configuration, then compiles the firmware. The build output is under `build/esp32DeviceHub/`.

### 6. Flash the ESP32 (optional)

Connect the board over USB, then build and upload in one step:

```bash
make flash
```

The default serial port is `/dev/ttyUSB0`. To use a different port, pass it on the command line, for example:

```bash
make flash UPLOAD_PORT=/dev/ttyACM0
```

On Linux, if access to the serial port is denied, add your user to the `dialout` group and log out and back in:

```bash
sudo usermod -a -G dialout "$USER"
```

### 7. Connect after the first boot

With no saved Wi-Fi credentials, the device starts an access point named `ESP32-XXXX`. Use the password displayed on the OLED, connect to that access point, and open [http://192.168.4.1](http://192.168.4.1). The default admin password is also displayed on the OLED. After signing in, go to **Settings > Network** to configure Wi-Fi; the device reboots and displays its assigned address.

For the optional Ethernet module wiring, see below. More detail is available in the [getting started guide](doc/getting-started.md) and [network setup guide](doc/network-setup.md).

## W5500 Ethernet wiring

The firmware tries W5500 Ethernet before saved WiFi credentials. It uses the ESP32 VSPI pins:

| W5500 | ESP32 |
| --- | --- |
| SCLK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| CS/SCS | GPIO14 |
| GND | GND |
| 3.3V | 3.3V |

The factory-reset button is now on GPIO16. GPIO5 and GPIO27 are already reserved by the application device configuration. The W5500 interrupt and reset pins are not required by this implementation; the driver uses polling and the module's hardware reset behavior. If DHCP times out while a link is present, use `192.168.4.1/24` for direct laptop connections. Configure the laptop as `192.168.4.2/24` and browse to `http://192.168.4.1`.

## Local API access

An authenticated administrator can generate up to five named API tokens from **Settings > Security > Local API access**. The raw token is displayed only after generation and is not stored in the browser or returned by later requests. The device stores only SHA-256 hashes. Each token has its own revoke button; generating is disabled when all five slots are used.

Use the token in the `X-API-Key` header:

```bash
curl -H "X-API-Key: YOUR_TOKEN" http://192.168.1.83/api/devices
```

Use these endpoints to discover device indexes, names, states, and available GPIO pins:

```bash
# Configured devices: index, name, pin, and state
curl -H "X-API-Key: YOUR_TOKEN" http://192.168.1.83/api/devices

# All configured and currently available GPIO pins
curl -H "X-API-Key: YOUR_TOKEN" http://192.168.1.83/api/device-config
```

Generating a new token uses an available token slot. Revoking it immediately disables scripts using it. Keep the token private and use HTTPS if requests can leave the trusted local network.
