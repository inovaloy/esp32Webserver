# ESP32 Device Hub

An ESP32-based device hub with a web interface for GPIO control, WiFi configuration, OLED status, persistent settings, and factory reset support.

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
