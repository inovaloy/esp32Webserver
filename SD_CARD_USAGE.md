# SD Card HTML Support for ESP32 Web Server

This guide explains how to use HTML files from SD card instead of compiling them into firmware.

## Overview

The project now supports two modes:
1. **SD Card Mode** (Recommended for development): Reads HTML files from SD card at runtime
2. **Compiled Mode** (Fallback): Uses compiled HTML embedded in firmware

## Benefits of SD Card Mode

- **Easy Development**: Update HTML files without recompiling and flashing firmware
- **Faster Iteration**: Just replace files on SD card and restart ESP32
- **Larger Files**: No firmware size constraints for HTML files
- **Live Updates**: Change files on SD card without rebuilding

## Setup Instructions

### 1. Prepare SD Card Files

Run the preparation script to copy HTML files to SD card format:

```bash
python3 Scripts/prepareSDCard.py
```

This creates a `sd_card_files/html/` directory with all your HTML files.

### 2. Copy Files to SD Card

1. Insert your SD card into your computer
2. Create a folder named `html` on the SD card root
3. Copy all files from `sd_card_files/html/` to the `html` folder on SD card

Your SD card structure should look like:
```
SD Card Root/
├── html/
│   ├── index.html
│   ├── home.html
│   ├── dashboard.html
│   ├── login.html
│   └── logout.html
```

### 3. Wiring (if not already done)

Connect SD card module to ESP32:
- **CS**   → GPIO 5 (or your chosen pin)
- **MOSI** → GPIO 23
- **MISO** → GPIO 19
- **SCK**  → GPIO 18
- **VCC**  → 3.3V
- **GND**  → GND

### 4. Update Your Sketch

Replace the old `startWebServer()` with `startWebServerWithSD()`:

```cpp
// Old way:
// #include "AutoGen/autoGenWebServer.h"
// startWebServer();

// New way:
#include "webServerSD.h"

void setup() {
    // ... your WiFi setup ...

    // Start web server with SD card support
    startWebServerWithSD(true, 5);  // true = enable SD, 5 = CS pin

    // ... rest of setup ...
}
```

### 5. Build and Flash

```bash
make build
make flash
```

## Configuration Options

### Enable/Disable SD Card

```cpp
// Use SD card for HTML files
startWebServerWithSD(true, 5);

// Disable SD card, use compiled HTML only
startWebServerWithSD(false);
```

### Change SD Card CS Pin

```cpp
// If your SD card CS pin is different (e.g., GPIO 15)
startWebServerWithSD(true, 15);
```

### Manual Control

You can also control the mode at runtime:

```cpp
extern bool useSDCardForHTML;

// Switch to SD card mode
useSDCardForHTML = true;

// Switch to compiled mode
useSDCardForHTML = false;
```

## Development Workflow

### Quick HTML Updates (SD Card Mode)

1. Edit HTML files in `html/` directory
2. Run: `python3 Scripts/prepareSDCard.py`
3. Copy updated files to SD card
4. Reset ESP32 (no need to recompile!)

### Production Build (Compiled Mode)

1. Edit HTML files in `html/` directory
2. Run: `make build` (compiles HTML into firmware)
3. Flash to ESP32

## Troubleshooting

### HTML Files Not Loading from SD Card

Check Serial Monitor for messages:
- "SD Card Mount Failed!" - Check wiring and SD card
- "File not found: /html/xxx.html" - Verify file exists on SD card
- "Failed to read complete file" - SD card might be corrupted

### Fallback Behavior

If SD card is not available or files are missing, the system automatically falls back to compiled HTML. This ensures your web server always works!

### Verify SD Card Contents

The startup will list all files found:
```
SD Card initialized successfully
Listing HTML files on SD card:
  FILE: index.html  SIZE: 1234
  FILE: home.html   SIZE: 2345
  ...
```

## File Size Considerations

### SD Card Mode
- No size limit (limited by SD card capacity)
- Files loaded on-demand (saves RAM)
- Slower first load, but same speed after

### Compiled Mode
- Limited by ESP32 flash size
- Faster initial load
- Uses more flash memory

## Maintenance

### Keep Both Modes Working

Always run the autogen scripts to keep compiled versions updated:
```bash
make autogen
```

This ensures fallback mode always works even if SD card fails.

### Version Control

Consider adding to `.gitignore`:
```
sd_card_files/
```

The SD card files are generated from `html/` directory, so you only need to track the source HTML files.

## Advanced: Dynamic Content

Both modes support the same dynamic API endpoints. Only HTML file serving is affected by SD card mode. All API handlers (`/api/login`, `/api/register`, etc.) work the same way.

## Performance Comparison

| Feature | SD Card Mode | Compiled Mode |
|---------|-------------|---------------|
| Update Speed | Instant | Full rebuild |
| Load Time | ~100-200ms | <10ms |
| Memory Usage | Low | Medium |
| Flash Usage | Low | High |
| Development | Excellent | Good |
| Production | Good | Excellent |

## Recommendation

- **Development**: Use SD Card Mode for rapid iteration
- **Production**: Use Compiled Mode for best performance and reliability
- **Hybrid**: Use SD card with compiled fallback (default setup)
