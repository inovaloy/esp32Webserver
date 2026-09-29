# Configuration Reference

The ESP32 Device Hub project uses two YAML files to configure hardware and HTTP routing. Both files are consumed by Python build scripts that generate C/C++ header files — no manual C editing is needed for routine configuration changes.

---

## Table of Contents

1. [deviceConfig.yaml — Hardware Configuration](#1-deviceconfigyaml--hardware-configuration)
2. [linkerData.yaml — HTTP Route Declaration](#2-linkerdatayaml--http-route-declaration)
3. [YAML Variables and Anchors](#3-yaml-variables-and-anchors)
4. [Adding a New Route](#4-adding-a-new-route)
5. [Adding a New HTML Page](#5-adding-a-new-html-page)
6. [Runtime Settings (Web Interface)](#6-runtime-settings-web-interface)

---

## 1. `deviceConfig.yaml` — Hardware Configuration

**File:** [`webApp/deviceConfig.yaml`](../webApp/deviceConfig.yaml)
**Generated output:** `Src/autoGen/autoGenDeviceConfig.h`

This file controls the hardware capabilities of the firmware: which GPIO pins are available for device control, how many devices can be registered, and which storage backend to use.

### Full File Example

```yaml
# Maximum number of devices the user can add via the web interface.
# Firmware hard limit is 16; EEPROM reserves a fixed slot per device.
maxDevices: 8

# storageBackend selects the EEPROM implementation.
# internal  → ESP32 flash emulation (EEPROM library, NVS-backed)
# external  → I²C 24LC64 at address 0x50
storageBackend: external

# GPIO pins exposed to the "High Voltage" device category.
# Use this for relay outputs that switch mains-voltage loads.
highVoltageGpioPins:
  - 26
  - 27
  - 25

# GPIO pins exposed to the "Low Voltage" device category.
# Use this for signal-level outputs: LED strips, fans, small DC loads.
lowVoltageGpioPins:
  - 33
  - 32
  - 17
  - 15
  - 5
```

### Field Reference

| Field | Type | Valid Values | Default | Description |
|---|---|---|---|---|
| `maxDevices` | integer | 1–16 | 16 | Maximum devices the user can add; clamped to 16 by the build script |
| `storageBackend` | string | `internal`, `external` | `internal` | Selects the EEPROM driver at compile time |
| `highVoltageGpioPins` | list of integers | Any valid ESP32 GPIO | `[]` | Pins shown as "High Voltage" in the Add Device form |
| `lowVoltageGpioPins` | list of integers | Any valid ESP32 GPIO | `[]` | Pins shown as "Low Voltage" in the Add Device form |

### Generated Macros (`autoGenDeviceConfig.h`)

| Macro / Symbol | Type | Example |
|---|---|---|
| `CFG_MAX_DEVICES` | `#define` | `8` |
| `CFG_STORAGE_EXTERNAL` | `#define` | `1` (external) or `0` (internal) |
| `CFG_EXTERNAL_EEPROM_ADDRESS` | `#define` | `0x50` (fixed) |
| `CFG_EXTERNAL_EEPROM_SIZE` | `#define` | `8192` (fixed) |
| `CFG_HIGH_VOLTAGE_PIN_COUNT` | `#define` | count of high-voltage pins |
| `CFG_HIGH_VOLTAGE_PINS[]` | `static const uint8_t[]` | `{26, 27, 25}` |
| `CFG_LOW_VOLTAGE_PIN_COUNT` | `#define` | count of low-voltage pins |
| `CFG_LOW_VOLTAGE_PINS[]` | `static const uint8_t[]` | `{33, 32, 17, 15, 5}` |

### Important Constraints

- **GPIO 4** — reserved for the OLED page button (`INPUT_PULLUP`)
- **GPIO 16** — reserved for the factory-reset button (`INPUT_PULLUP`)
- **GPIO 18, 19, 23, 14** — reserved for W5500 SPI (SCLK, MISO, MOSI, CS)
- **GPIO 21, 22** — reserved for I²C (OLED + external EEPROM)
- **GPIO 33** — used as status LED; can also be listed in `lowVoltageGpioPins` if desired but conflicts with LED behaviour
- The total number of configured pins may exceed `maxDevices`; the build script will warn but truncate the list to `maxDevices`

---

## 2. `linkerData.yaml` — HTTP Route Declaration

**File:** [`webApp/linkerData.yaml`](../webApp/linkerData.yaml)
**Generated output:** `Src/autoGen/autoGenWebServer.h` and `Src/autoGen/autoGenWebServer.cpp`

This file is the single source of truth for every URL the web server handles. The build script reads it and generates all the `httpd_uri_t` structs and handler wrappers — you never write HTTP glue code by hand.

### File Structure

The file has two sections:

1. **Variable definitions** (`httpMethods`, `responseTypes`, `contentTypes`) — YAML anchors used by the route entries to keep values consistent and avoid typos
2. **Route entries** — one YAML mapping per URL path

### Variable Definitions

```yaml
# HTTP request method aliases
httpMethods: &httpMethods
    GET : &httpGET  "GET"
    POST: &httpPOST "POST"

# Response type aliases (determines handler template used in generated code)
responseTypes: &responseTypes
    HTML : &rtnHTML  "HTML"   # serve a gzip HTML byte array
    JSON : &rtnJSON  "JSON"   # call a hook function, return JSON
    ASSET: &rtnASSET "ASSET"  # serve a gzip asset byte array with MIME type

# Content type aliases for assets
contentTypes: &contentTypes
    CSS       : &cssType  "text/css"
    JAVASCRIPT: &jsType   "application/javascript"
    IMAGE_JPG : &jpgType  "image/jpg"
    IMAGE_PNG : &pngType  "image/png"
    JSON      : &jsonType "application/json"
```

These are referenced in route entries using the `*alias` YAML syntax.

### Route Entry Schemas

#### HTML Page Route

Serves a gzip-compressed HTML file from `autoGenHtmlData.h`.

```yaml
"/path":
    rtnType : *rtnHTML           # must be "HTML"
    fileName: "page.html"        # source file relative to webApp/html/
    macro   : "PAGE_MACRO_NAME"  # name for the webServerMacro enum entry
    reqType : *httpGET           # HTTP method
```

| Field | Required | Description |
|---|---|---|
| `rtnType` | Yes | Must be `"HTML"` |
| `fileName` | Yes | HTML source file name (relative to `webApp/html/`) |
| `macro` | Yes | Identifier added to the `webServerMacro` C enum; passed to `webHandlerHook()` |
| `reqType` | Yes | `"GET"` or `"POST"` (always `"GET"` for HTML pages) |

#### Asset Route

Serves a gzip-compressed static file (CSS, JS, image) from `autoGenAssets.h`.

```yaml
"/css/style.css":
    rtnType    : *rtnASSET           # must be "ASSET"
    fileName   : "css/style.css"     # source file relative to webApp/assets/
    macro      : "STYLE_CSS"         # C enum entry name
    contentType: *cssType            # MIME type sent in Content-Type header
    reqType    : *httpGET
```

| Field | Required | Description |
|---|---|---|
| `rtnType` | Yes | Must be `"ASSET"` |
| `fileName` | Yes | Asset source path relative to `webApp/assets/` |
| `macro` | Yes | C enum identifier |
| `contentType` | Yes | MIME type string |
| `reqType` | Yes | HTTP method (always `"GET"` for assets) |

#### JSON (API) Route

Calls a hook function in `webServer.cpp` and returns the JSON string it produces.

```yaml
"/api/endpoint":
    rtnType: *rtnJSON    # must be "JSON"
    reqType: *httpPOST   # "GET" or "POST"
```

| Field | Required | Description |
|---|---|---|
| `rtnType` | Yes | Must be `"JSON"` |
| `reqType` | Yes | `"GET"` or `"POST"` |

> The hook function name is derived automatically from the route path. `/api/devices/add` → `apiDevicesAddHandlerHook`. See [build-system.md](build-system.md) for the naming rules.

### Complete Route List (as shipped)

| Route | Method | Type | Purpose |
|---|---|---|---|
| `/` | GET | HTML | SPA shell (`base.html`) |
| `/login` | GET | HTML | Login page |
| `/page/dashboard` | GET | HTML | Dashboard fragment |
| `/page/wifi` | GET | HTML | Wi-Fi config fragment |
| `/page/settings` | GET | HTML | Settings fragment |
| `/favicon.png` | GET | ASSET | Browser favicon |
| `/apple-touch-icon.png` | GET | ASSET | iOS home screen icon |
| `/css/style.css` | GET | ASSET | Stylesheet |
| `/js/app.js` | GET | ASSET | Frontend JS |
| `/api/login` | POST | JSON | Authenticate and get session token |
| `/api/logout` | POST | JSON | Invalidate session token |
| `/api/auth/change-password` | POST | JSON | Change admin password |
| `/api/auth/set-initial-password` | POST | JSON | Set password on first boot |
| `/api/wifi/status` | GET | JSON | Current network status |
| `/api/wifi/scan` | GET | JSON | Scan for Wi-Fi networks |
| `/api/wifi/connect` | POST | JSON | Connect to a Wi-Fi network |
| `/api/settings` | GET | JSON | Read controller settings |
| `/api/settings/save` | POST | JSON | Save controller settings |
| `/api/settings/backup` | GET | JSON | Download configuration backup |
| `/api/settings/restore` | POST | JSON | Restore from backup JSON |
| `/api/settings/factory-reset` | POST | JSON | Factory reset (requires password) |
| `/api/firmware/update` | POST | JSON | OTA firmware upload |
| `/api/devices` | GET | JSON | List all configured devices |
| `/api/device-config` | GET | JSON | Available pins and device limit |
| `/api/devices/add` | POST | JSON | Add a new device |
| `/api/devices/remove` | POST | JSON | Remove a device |
| `/api/devices/toggle` | POST | JSON | Toggle a device ON/OFF |
| `/api/devices/rename` | POST | JSON | Rename a device |
| `/api/devices/automation` | GET | JSON | Read automation settings for all devices |
| `/api/devices/automation/save` | POST | JSON | Save automation settings for a device |
| `/api/reboot` | POST | JSON | Schedule a device reboot |

---

## 3. YAML Variables and Anchors

YAML anchors (`&name`) define a value once; aliases (`*name`) reuse it. This prevents typos and makes it easy to update a value in one place.

```yaml
# Define the anchor:
httpMethods: &httpMethods
    GET: &httpGET "GET"

# Use the alias:
"/some/route":
    reqType: *httpGET     # expands to "GET"
```

The top-level `httpMethods`, `responseTypes`, and `contentTypes` keys are **only** anchor definitions — the build script ignores them when processing routes (they contain no `rtnType` field).

---

## 4. Adding a New Route

**Example:** Add a new JSON API endpoint `POST /api/devices/all-off` that turns all devices off.

### Step 1 — Add the route to `linkerData.yaml`

```yaml
"/api/devices/all-off":
    rtnType: *rtnJSON
    reqType: *httpPOST
```

### Step 2 — Run autogen

```bash
make autogen
```

The build script generates the hook declaration in `autoGenWebServer.h`:

```cpp
char* apiDevicesAllOffHandlerHook(httpd_req_t *req);
```

And the wrapper handler + URI registration in `autoGenWebServer.cpp`.

### Step 3 — Implement the hook in `webServer.cpp`

```cpp
char* apiDevicesAllOffHandlerHook(httpd_req_t *req) {
    if (!isAuthorised(req)) { sendUnauthorised(req); return nullptr; }
    for (uint8_t i = 0; i < deviceCount; i++) {
        devices[i].state = 0;
        digitalWrite(devices[i].pin, LOW);
    }
    saveDevicesToEEPROM();
    oledStatusDirty = true;
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    char *out = cJSON_Print(response);
    cJSON_Delete(response);
    return out;
}
```

### Step 4 — Build and flash

```bash
make flash
```

---

## 5. Adding a New HTML Page

**Example:** Add a page at `/page/about` served from `about.html`.

### Step 1 — Create the HTML fragment

Create `webApp/html/about.html`. This file is an HTML *fragment* (no `<html>`, `<head>`, or `<body>` tags — just the inner content), loaded dynamically into the SPA shell via `fetch()`.

```html
<div class="page-heading">
    <h2>About</h2>
    <p>ESP32 Device Hub — firmware v1.0.1</p>
</div>
```

### Step 2 — Declare the route in `linkerData.yaml`

```yaml
"/page/about":
    rtnType : *rtnHTML
    fileName: "about.html"
    macro   : "ABOUT_HTML"
    reqType : *httpGET
```

### Step 3 — Run autogen, build, and flash

```bash
make autogen
make flash
```

### Step 4 — Add the route to the SPA router in `base.html`

```javascript
const PAGES = {
    '':        '/page/dashboard',
    'dashboard': '/page/dashboard',
    'settings':  '/page/settings',
    'about':     '/page/about',   // ← add this
};
```

---

## 6. Runtime Settings (Web Interface)

The following settings are not YAML-configured; they are stored in EEPROM and managed through the web interface:

| Setting | Default | Range / Notes |
|---|---|---|
| Controller name | `"Home Controller"` | Up to 20 characters |
| Session timeout | 15 minutes | 1–1440 minutes |
| OLED brightness | 100 | 1–255 |
| OLED enabled | `true` | On/Off |
| Admin password | 8-char MAC-derived hex | Min 6, max 32 characters; set via Settings → Security or forced on first login |
| Wi-Fi SSID | *(empty)* | Saved via Settings → Network or captive portal |
| Wi-Fi password | *(empty)* | Saved with SSID |
