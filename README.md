# ESP32 Web Server

A flexible ESP32-based web server with support for both compiled HTML and SD card-based HTML serving.

## Features

- 🌐 Full-featured web server with WiFi connectivity
- 📱 Multiple HTML pages (index, home, dashboard, login, logout)
- 💾 **NEW: SD Card support** - Serve HTML files from SD card without recompiling
- 🔄 Automatic fallback to compiled HTML if SD card unavailable
- 📊 OLED display integration (SSD1306)
- 🔌 RESTful API endpoints
- ⚡ Fast development workflow for HTML updates

## Quick Start

### Traditional Build (Compiled HTML)
```bash
make configure  # One-time setup
make build      # Build with compiled HTML
make flash      # Upload to ESP32
```

### Development Build (SD Card HTML) - NEW! 🎉
```bash
make configure     # One-time setup
make sd-prepare    # Prepare HTML files for SD card
# Copy sd_card_files/html/* to SD card's /html/ folder
make build         # Build with SD card support
make flash         # Upload to ESP32
```

See [QUICK_START_SD.md](QUICK_START_SD.md) for detailed SD card setup.

## Why Use SD Card Mode?

| Feature | Compiled Mode | SD Card Mode |
|---------|--------------|--------------|
| HTML Update Time | 2-5 minutes | 10 seconds |
| Requires Reflash | Yes | No |
| Flash Size Impact | High | Low |
| Best For | Production | Development |

## Build Targets

- `make configure` - Set up the ESP32 Arduino environment (one-time)
- `make autogen` - Generate compiled HTML files
- `make sd-prepare` - **NEW**: Prepare HTML files for SD card
- `make build` - Compile the sketch
- `make flash` - Upload to ESP32
- `make clean` - Clean build artifacts
- `make help` - Show all available targets

## Documentation

- 📖 [Quick Start Guide](QUICK_START_SD.md) - Get started with SD card in 3 steps
- 📘 [Migration Guide](MIGRATION_GUIDE.md) - Upgrade your existing code
- 📕 [SD Card Usage](SD_CARD_USAGE.md) - Comprehensive SD card guide
- 📗 [Implementation Summary](IMPLEMENTATION_SUMMARY.md) - Technical details

## Project Structure

```
esp32Webserver/
├── html/                      # Source HTML files
├── sd_card_files/            # Generated SD card files (NEW)
├── Src/                      # Source code
│   ├── esp32Webserver.ino    # Main sketch (original)
│   ├── esp32Webserver_SD.ino # Example with SD card (NEW)
│   ├── webServerSD.*         # SD card web server (NEW)
│   ├── sdCardHandler.*       # SD card operations (NEW)
│   └── AutoGen/              # Auto-generated compiled HTML
├── Scripts/
│   ├── compileHtml.py        # Compile HTML to C arrays
│   ├── updateWebServer.py    # Generate web server code
│   └── prepareSDCard.py      # Prepare SD card files (NEW)
└── Libs/                     # External libraries
```

## Hardware Requirements

- ESP32 development board
- SSD1306 OLED display (I2C)
- **Optional**: MicroSD card module + SD card (for SD card mode)

### SD Card Wiring (Optional)

```
SD Card Module    ESP32
--------------    -----
CS           →    GPIO 5
MOSI         →    GPIO 23
MISO         →    GPIO 19
SCK          →    GPIO 18
VCC          →    3.3V
GND          →    GND
```

## Getting Started

### Option 1: Quick Demo (Compiled HTML)
```bash
make configure
make build
make flash
```

### Option 2: Development Setup (SD Card)
```bash
# 1. Prepare files
make configure
make sd-prepare

# 2. Copy to SD card
# Copy sd_card_files/html/* to SD card's /html/ folder

# 3. Build and flash
make build
make flash

# 4. Now you can update HTML files without recompiling!
```

## Updating HTML Files

### Compiled Mode (Production)
```bash
# Edit html/*.html files
make build    # Recompiles everything
make flash    # Upload to ESP32
```

### SD Card Mode (Development) - Fast! ⚡
```bash
# Edit html/*.html files
make sd-prepare              # Regenerate SD card files
# Copy to SD card
# Press ESP32 reset button
# Done! No compilation needed!
```

## WiFi Configuration

Edit `Src/esp32Webserver.ino`:
```cpp
const char* ssid     = "Your_WiFi_SSID";
const char* password = "Your_WiFi_Password";
```

## Available Endpoints

- `/` - Index page
- `/home` - Home page
- `/dshbrd` - Dashboard
- `/login` - Login page
- `/logout` - Logout page
- `/api/login` - Login API (POST)
- `/api/register` - Registration API (POST)

## Dependencies

- ESP32 Arduino Core
- Adafruit GFX Library
- Adafruit SSD1306 Library
- Adafruit BusIO Library
- cJSON (included)

## License

[Your License Here]

## Contributing

Contributions welcome! Please feel free to submit a Pull Request.

## Support

Need help? Check the documentation:
- [Quick Start (SD Card)](QUICK_START_SD.md)
- [Migration Guide](MIGRATION_GUIDE.md)
- [Full Documentation](SD_CARD_USAGE.md)
