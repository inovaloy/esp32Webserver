# Migration Guide: From Compiled to SD Card HTML

## Quick Comparison

### Before (Current esp32Webserver.ino)
```cpp
#include <WiFi.h>
#include "AutoGen/autoGenWebServer.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ... display setup ...

void setup() {
    // ... WiFi setup ...

    startWebServer();  // ← Old way

    // ... rest of setup ...
}
```

### After (With SD Card Support)
```cpp
#include <WiFi.h>
#include "webServerSD.h"  // ← Changed from AutoGen/autoGenWebServer.h
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ... display setup ...

void setup() {
    // ... WiFi setup ...

    startWebServerWithSD(true, 5);  // ← New way (SD enabled, CS pin 5)

    // ... rest of setup ...
}
```

## Detailed Changes

### Change 1: Include Statement
```cpp
// OLD:
#include "AutoGen/autoGenWebServer.h"

// NEW:
#include "webServerSD.h"
```

### Change 2: Start Function
```cpp
// OLD:
startWebServer();

// NEW:
startWebServerWithSD(true, 5);
// Parameters:
//   true = enable SD card mode
//   5 = SD card CS pin number
```

## Complete Modified esp32Webserver.ino

Here's your complete modified sketch:

```cpp
#include <WiFi.h>
#include "webServerSD.h"  // Changed from AutoGen/autoGenWebServer.h
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SD Card CS pin (change if your wiring is different)
#define SD_CS_PIN 5

int counter = 0;

void setup()
{
    const char* ssid     = "Biswas family";
    const char* password = "biswas2022";

    bool ledStatus = LOW;

    Serial.begin(115200);
    Serial.println();

    pinMode(4, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(4, LOW);
    digitalWrite(33, ledStatus);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();

    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println(F("ESP32 WebServer"));
    display.display();
    delay(2000);

    Serial.println("Connecting...");
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Connecting to WiFi...");
    display.display();

    WiFi.begin(ssid, password);
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        ledStatus = !ledStatus;
        digitalWrite(33, ledStatus);
    }
    Serial.println("");
    Serial.println("WiFi connected");
    display.clearDisplay();
    display.println("WiFi connected");
    display.display();

    // Start web server with SD card support
    // Will automatically fall back to compiled HTML if SD card fails
    startWebServerWithSD(true, SD_CS_PIN);

    Serial.print("Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
    display.print("http://");
    display.print(WiFi.localIP());
    display.display();
}

void loop()
{
    bool ledStatus = HIGH;
    digitalWrite(33, ledStatus);
    delay(100);
    ledStatus = LOW;
    digitalWrite(33, ledStatus);
    delay(100);
}
```

## Configuration Options

### Option 1: SD Card Enabled (Recommended for Development)
```cpp
startWebServerWithSD(true, 5);
// Tries SD card first, falls back to compiled if unavailable
```

### Option 2: Compiled Only (Production)
```cpp
startWebServerWithSD(false);
// Uses only compiled HTML, no SD card needed
// Same as old behavior but with new function name
```

### Option 3: Different CS Pin
```cpp
startWebServerWithSD(true, 15);
// If your SD card CS is connected to GPIO 15
```

### Option 4: Conditional Compilation
```cpp
#ifdef DEBUG_BUILD
    startWebServerWithSD(true, 5);   // SD for development
#else
    startWebServerWithSD(false);     // Compiled for production
#endif
```

## Step-by-Step Migration

### Step 1: Keep Backup
```bash
cp Src/esp32Webserver.ino Src/esp32Webserver.ino.backup
```

### Step 2: Prepare SD Card Files
```bash
make sd-prepare
```

### Step 3: Copy to SD Card
1. Insert SD card in computer
2. Create `html` folder on SD card root
3. Copy all files from `sd_card_files/html/` to SD card's `html/` folder

### Step 4: Modify Sketch
Only 2 lines to change in `Src/esp32Webserver.ino`:

```cpp
// Line ~2: Change include
#include "webServerSD.h"

// Line ~75: Change start function
startWebServerWithSD(true, 5);
```

### Step 5: Build and Flash
```bash
make build
make flash
```

### Step 6: Verify
Watch Serial Monitor for:
```
SD Card initialized successfully
Listing HTML files on SD card:
  FILE: index.html  SIZE: 314
  FILE: home.html   SIZE: 12207
  ...
Web server started successfully
Mode: SD Card with compiled fallback
```

## Troubleshooting Migration

### Problem: Compilation errors
**Solution**: Make sure you have the new files:
```bash
ls Src/sdCardHandler.h
ls Src/sdCardHandler.cpp
ls Src/webServerSD.h
ls Src/webServerSD.cpp
```

### Problem: SD card not mounting
**Solution**: Check Serial Monitor output:
- "SD Card Mount Failed!" → Check wiring
- "No SD card attached" → Insert SD card
- Falls back to compiled HTML automatically

### Problem: HTML not updating on SD card
**Solution**:
1. Make sure you're looking at the right IP address
2. Check Serial Monitor for "Serving xxx from SD card"
3. Press reset button after changing files

### Problem: Want to go back to old version
**Solution**: Just change back:
```cpp
#include "AutoGen/autoGenWebServer.h"
startWebServer();
```
Both versions coexist peacefully!

## Testing Your Migration

### Test 1: Verify SD Card Mode
1. Flash the new code
2. Check Serial Monitor for "Serving xxx from SD card"
3. Access web pages - should work

### Test 2: Verify Fallback
1. Remove SD card
2. Reset ESP32
3. Check Serial Monitor for "SD Card initialization failed"
4. Access web pages - should still work (using compiled)

### Test 3: Update HTML on SD Card
1. Edit `html/login.html` (add a comment)
2. Run `make sd-prepare`
3. Copy updated `login.html` to SD card
4. Reset ESP32
5. Access `/login` - should see changes

## Makefile Updates

The Makefile now supports:

```bash
make sd-prepare  # Prepare files for SD card
make build       # Build with compiled HTML (still works)
make flash       # Flash to ESP32 (still works)
make help        # See all commands
```

## What Stays the Same

✅ All API endpoints (`/api/login`, `/api/register`)
✅ Hook functions (`webHandlerHook`, `apiLoginHandlerHook`)
✅ WiFi connection code
✅ Display code
✅ LED blink code
✅ Build process (`make build`, `make flash`)

## What's New

✨ SD card support with automatic fallback
✨ Faster HTML development workflow
✨ No firmware reflash for HTML changes
✨ Larger HTML files possible (no flash constraint)
✨ Runtime mode switching capability

## Summary

**Minimal changes required**: Just 2 lines in your sketch!
**Maximum benefit**: Update HTML files in seconds instead of minutes!
**Zero risk**: Automatic fallback ensures your server always works!

Ready to migrate? Start with Step 1 above! 🚀
