# SD Card Setup Checklist

Use this checklist to set up your ESP32 with SD card HTML serving.

## ☐ Hardware Setup

### SD Card Module
- [ ] SD card module connected to ESP32
  - [ ] CS → GPIO 5 (or note your pin: _____)
  - [ ] MOSI → GPIO 23
  - [ ] MISO → GPIO 19
  - [ ] SCK → GPIO 18
  - [ ] VCC → 3.3V
  - [ ] GND → GND

### SD Card
- [ ] SD card formatted (FAT32 recommended)
- [ ] SD card tested in computer (can read/write)
- [ ] SD card inserted in module

## ☐ Software Preparation

### Generate SD Card Files
- [ ] Run: `make sd-prepare`
- [ ] Verify files created in `sd_card_files/html/`
- [ ] Check file sizes match expectations

### Expected Files
- [ ] `sd_card_files/html/index.html` (~314 bytes)
- [ ] `sd_card_files/html/home.html` (~12 KB)
- [ ] `sd_card_files/html/dashboard.html` (~6 KB)
- [ ] `sd_card_files/html/login.html` (~7.5 KB)
- [ ] `sd_card_files/html/logout.html` (~205 bytes)

## ☐ SD Card File Transfer

### Create Directory Structure
- [ ] Insert SD card in computer
- [ ] Create `html` folder at SD card root
- [ ] Verify path: `[SD CARD]/html/`

### Copy Files
- [ ] Copy `sd_card_files/html/index.html` → `[SD CARD]/html/index.html`
- [ ] Copy `sd_card_files/html/home.html` → `[SD CARD]/html/home.html`
- [ ] Copy `sd_card_files/html/dashboard.html` → `[SD CARD]/html/dashboard.html`
- [ ] Copy `sd_card_files/html/login.html` → `[SD CARD]/html/login.html`
- [ ] Copy `sd_card_files/html/logout.html` → `[SD CARD]/html/logout.html`

### Verify on SD Card
- [ ] All 5 HTML files present in `/html/` folder
- [ ] File sizes match source files
- [ ] File names are exactly correct (case-sensitive)

## ☐ Code Modification

### Update Sketch
- [ ] Open `Src/esp32Webserver.ino`
- [ ] Change include: `#include "webServerSD.h"`
- [ ] Change start function: `startWebServerWithSD(true, 5);`
- [ ] If using different CS pin, update pin number: `startWebServerWithSD(true, YOUR_PIN);`
- [ ] Save file

### Alternative: Use Example
- [ ] Or use provided example: `Src/esp32Webserver_SD.ino`
- [ ] Copy to `esp32Webserver.ino` if preferred

## ☐ Build and Flash

### Compile
- [ ] Run: `make build`
- [ ] Verify no compilation errors
- [ ] Check build output for new files included

### Upload
- [ ] Connect ESP32 to computer
- [ ] Run: `make flash`
- [ ] Wait for upload to complete
- [ ] ESP32 should restart automatically

## ☐ Testing

### Initial Boot
- [ ] Open Serial Monitor (115200 baud)
- [ ] Look for: "SD Card initialized successfully"
- [ ] Look for: "Listing HTML files on SD card:"
- [ ] Verify all 5 files listed with sizes

### Web Server Start
- [ ] Look for: "Web server started successfully"
- [ ] Look for: "Mode: SD Card with compiled fallback"
- [ ] Note the IP address shown

### Access Web Pages
- [ ] Open browser
- [ ] Visit: `http://[ESP32_IP]/`
- [ ] Verify index page loads
- [ ] Visit: `http://[ESP32_IP]/login`
- [ ] Verify login page loads
- [ ] Check Serial Monitor for: "Serving xxx.html from SD card"

## ☐ Test Fallback (Optional)

### Remove SD Card Test
- [ ] Power off ESP32
- [ ] Remove SD card
- [ ] Power on ESP32
- [ ] Check Serial: "SD Card initialization failed"
- [ ] Access web pages - should still work (compiled fallback)
- [ ] Check Serial: "Serving xxx.html from compiled data"

## ☐ Test HTML Updates

### Edit and Update
- [ ] Edit `html/login.html` (add a comment: `<!-- TEST -->`)
- [ ] Run: `make sd-prepare`
- [ ] Copy updated `login.html` to SD card
- [ ] Press ESP32 reset button
- [ ] Access `/login` in browser
- [ ] View page source - should see `<!-- TEST -->`

## Troubleshooting

### Problem: "SD Card Mount Failed"
- [ ] Check wiring connections
- [ ] Verify 3.3V power (not 5V)
- [ ] Try different SD card
- [ ] Check CS pin number in code

### Problem: "No SD card attached"
- [ ] Verify SD card is inserted fully
- [ ] Try reinserting SD card
- [ ] Test SD card in computer

### Problem: "File not found: /html/xxx.html"
- [ ] Check folder name is exactly `html` (lowercase)
- [ ] Verify files are in `/html/` not `/html/html/`
- [ ] Check file names match exactly
- [ ] Re-copy files from `sd_card_files/html/`

### Problem: HTML not updating
- [ ] Verify you copied the updated file to SD card
- [ ] Press ESP32 reset button (not just refresh browser)
- [ ] Check Serial Monitor shows "from SD card"
- [ ] Try clearing browser cache

### Problem: Web page shows compiled version
- [ ] Check Serial Monitor output
- [ ] If SD fails, it automatically uses compiled
- [ ] Verify `useSDCardForHTML = true` in logs
- [ ] Check SD card is properly mounted

## Success Criteria

✅ SD card initialization successful
✅ All 5 HTML files found on SD card
✅ Web server starts successfully
✅ All pages load correctly
✅ Serial Monitor shows "Serving xxx from SD card"
✅ HTML updates work without reflashing

## Completion

- [ ] All checks passed ✓
- [ ] System working as expected ✓
- [ ] Documentation read and understood ✓

**Congratulations! Your ESP32 web server with SD card support is ready! 🎉**

---

## Reference Information

**Default CS Pin**: GPIO 5
**SD Card Format**: FAT32
**Folder on SD Card**: `/html/`
**Build Command**: `make build`
**Flash Command**: `make flash`
**Prepare Command**: `make sd-prepare`

**Serial Monitor Baud Rate**: 115200

**Expected Serial Messages**:
- "SD Card initialized successfully"
- "SD Card Size: XXX MB"
- "Listing HTML files on SD card:"
- "Web server started successfully"
- "Mode: SD Card with compiled fallback"
- "Serving xxx.html from SD card"

**Documentation Files**:
- Quick Start: `QUICK_START_SD.md`
- Migration: `MIGRATION_GUIDE.md`
- Full Guide: `SD_CARD_USAGE.md`
- This Checklist: `CHECKLIST.md`
