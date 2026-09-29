# Build System

This document explains the Python-based build pipeline, all autogen scripts, the Makefile targets, and the naming conventions used when converting YAML routes to C/C++ identifiers.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Makefile Targets](#2-makefile-targets)
3. [Python Environment and Dependencies](#3-python-environment-and-dependencies)
4. [Common Module (`Scripts/common.py`)](#4-common-module-scriptscommonpy)
5. [Utility Module (`Scripts/utility.py`)](#5-utility-module-scriptsutilitypy)
6. [Script: `compileDeviceConfig.py`](#6-script-compiledeviceconfigpy)
7. [Script: `compileHtml.py`](#7-script-compilehtmlpy)
8. [Script: `compileAssets.py`](#8-script-compileassetsspy)
9. [Script: `updateWebServer.py`](#9-script-updatewebserverpy)
10. [Script: `compileOledLogo.py`](#10-script-compileoledlogopy)
11. [Naming Conventions](#11-naming-conventions)
12. [Build Artefacts and Clean](#12-build-artefacts-and-clean)
13. [Extending the Pipeline](#13-extending-the-pipeline)

---

## 1. Overview

The build pipeline transforms high-level source files (YAML configuration, HTML fragments, CSS, JS, images) into C/C++ header and source files that are compiled directly into the firmware binary. This means:

- The web interface is served entirely from flash memory — no SD card, no filesystem partition required
- Assets are gzip-compressed, reducing firmware size and network transfer time
- CSS and JS are minified before compression
- HTTP routing boilerplate is generated from a declarative YAML file, not written by hand

### Pipeline Execution Order

```
1. compileDeviceConfig.py   ← must run first (defines CFG_* macros used by firmware)
2. compileHtml.py           ← compress HTML pages to C arrays
3. compileAssets.py         ← minify + compress CSS/JS/images to C arrays
4. updateWebServer.py       ← generate URI handlers and startWebServer()
5. compileOledLogo.py       ← convert logo image to C bitmap array
```

All five scripts are run together by `make autogen`.

### File Locations

| Stage | Input | Output |
|---|---|---|
| Intermediate build | — | `.temp/autoGen/` |
| Firmware include | — | `Src/autoGen/` |

Scripts write to `.temp/autoGen/` first, then copy the final files to `Src/autoGen/`.

---

## 2. Makefile Targets

All targets are defined in the root [`Makefile`](../Makefile).

| Target | Command | Description |
|---|---|---|
| `help` | `make` or `make help` | Prints available targets (default target) |
| `autogen` | `make autogen` | Runs all 5 Python build scripts |
| `build` | `make build` | Runs `autogen` then compiles the Arduino sketch |
| `flash` | `make flash` | Runs `build` then uploads to the ESP32 |
| `configure` | `make configure` | First-time setup: clones Arduino-ESP32 core and `makeEspArduino` into `.temp/` |
| `erase_flash` | `make erase_flash` | Erases the entire ESP32 flash using `esptool` |
| `erase_and_flash` | `make erase_and_flash` | Erases flash then runs `flash` (use after different firmware/bootloader) |
| `clean` | `make clean` | Removes build artefacts, `Src/autoGen/`, `.temp/autoGen/`, `build/` |

### Environment Variables

The Makefile passes these paths to the Arduino build sub-make:

| Variable | Default | Description |
|---|---|---|
| `TEMP_DIR` | `.temp/` | Scratch directory for downloaded toolchain |
| `ESP32_PATH` | `.temp/esp32/` | Arduino-ESP32 core |
| `ESP_MAKE_PATH` | `.temp/espMake/` | `makeEspArduino` wrapper |
| `ROOT` | Project root | Absolute path to the repo root |

---

## 3. Python Environment and Dependencies

The build scripts run in the `.venv/` virtual environment:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

### `requirements.txt` Contents

| Package | Purpose |
|---|---|
| `PyYAML` | Parses `linkerData.yaml` and `deviceConfig.yaml` |
| `Pillow` | Reads and converts the OLED boot logo image |
| `rjsmin` | JavaScript minifier (regex-safe, template-literal-safe) |
| `rcssmin` | CSS minifier (handles `@media`, CSS variables, all valid CSS) |

The Makefile invokes `.venv/bin/python3` explicitly to ensure the correct environment is always used.

---

## 4. Common Module (`Scripts/common.py`)

Defines shared path constants and configuration used by all build scripts.

### Path Constants

| Constant | Value | Description |
|---|---|---|
| `WEB_APP_DIR` | `"webApp"` | Root of the web application sources |
| `HTML_DIR` | `"webApp/html"` | HTML fragment source files |
| `ASSETS_DIR` | `"webApp/assets"` | CSS, JS, and image source files |
| `LINKER_DATA_FILE` | `"webApp/linkerData.yaml"` | HTTP route declarations |
| `AUTOGEN_DEST_DIR` | `"src/autoGen"` | Final destination for generated C/C++ files |
| `BUILD_DIR` | `".temp/autoGen"` | Intermediate build output |

### Supported Asset Extensions

| Extension | MIME Type |
|---|---|
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.png` | `image/png` |
| `.jpg` / `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.ico` | `image/x-icon` |
| `.woff` / `.woff2` | `font/woff` / `font/woff2` |
| `.ttf` | `font/ttf` |

Text assets (`.css`, `.js`, `.svg`) are minified before gzip compression. Binary assets (images, fonts) are compressed without modification.

### Required Fields per Route Type (`LINKER_YAML_REQUIRED_FIELDS`)

| `rtnType` | Required fields |
|---|---|
| `HTML` | `rtnType`, `fileName`, `macro`, `reqType` |
| `ASSET` | `rtnType`, `fileName`, `contentType`, `macro`, `reqType` |
| `JSON` | `rtnType`, `reqType` |

---

## 5. Utility Module (`Scripts/utility.py`)

Helper functions shared by the build scripts.

### `readLinkerData(linkerDataPath)`

Reads `linkerData.yaml` and returns a dictionary of route entries, filtered to keys starting with `/`. The top-level anchor definitions (`httpMethods`, `responseTypes`, `contentTypes`) are excluded.

```python
routes = readLinkerData("webApp/linkerData.yaml")
# → { "/": {...}, "/login": {...}, "/api/login": {...}, ... }
```

### `validateLinkerData(linkerData)`

Validates every route entry against `LINKER_YAML_REQUIRED_FIELDS`. Raises `ValueError` with a list of all errors if any route is malformed. This is called by `updateWebServer.py` before code generation begins.

### `convertToCamelCase(filename, separator)`

Converts a filename or path fragment to a C-safe camelCase identifier by replacing all non-alphanumeric characters with underscores, then capitalising each segment:

```python
convertToCamelCase("base.html")       # → "baseHtml"
convertToCamelCase("css/style.css")   # → "cssStyleCss"
convertToCamelCase("image/webLogo.png") # → "imageWeblogoPng"
```

### `minifyCss(cssContent)` / `minifyJavaScript(jsContent)`

Thin wrappers around `rcssmin.cssmin()` and `rjsmin.jsmin()` for use in `compileAssets.py`.

---

## 6. Script: `compileDeviceConfig.py`

**Input:** `webApp/deviceConfig.yaml`
**Output:** `Src/autoGen/autoGenDeviceConfig.h`

Reads the hardware configuration YAML and emits a C header with preprocessor macros and static arrays.

### Logic

1. Parse `deviceConfig.yaml` with `yaml.safe_load()`
2. Extract `maxDevices`, `storageBackend`, `highVoltageGpioPins`, `lowVoltagePins`
3. Clamp `maxDevices` to 16 (firmware hard limit); warn if exceeded
4. Truncate pin lists if total pins exceed `maxDevices`; warn
5. Emit `autoGenDeviceConfig.h` with:
   - `CFG_MAX_DEVICES`
   - `CFG_STORAGE_EXTERNAL` (0 or 1)
   - `CFG_EXTERNAL_EEPROM_ADDRESS` (always `0x50`)
   - `CFG_EXTERNAL_EEPROM_SIZE` (always `8192`)
   - `CFG_HIGH_VOLTAGE_PIN_COUNT` and `CFG_HIGH_VOLTAGE_PINS[]`
   - `CFG_LOW_VOLTAGE_PIN_COUNT` and `CFG_LOW_VOLTAGE_PINS[]`

### Generated Header Example

```c
#define CFG_MAX_DEVICES 8
#define CFG_STORAGE_EXTERNAL 1
#define CFG_EXTERNAL_EEPROM_ADDRESS 0x50
#define CFG_EXTERNAL_EEPROM_SIZE 8192
#define CFG_HIGH_VOLTAGE_PIN_COUNT 3
static const uint8_t CFG_HIGH_VOLTAGE_PINS[3] = { 26, 27, 25 };
#define CFG_LOW_VOLTAGE_PIN_COUNT 5
static const uint8_t CFG_LOW_VOLTAGE_PINS[5] = { 33, 32, 17, 15, 5 };
```

---

## 7. Script: `compileHtml.py`

**Input:** `webApp/html/*.html` (files declared in `linkerData.yaml` with `rtnType: HTML`)
**Output:** `Src/autoGen/autoGenHtmlData.h`

### Processing Steps

1. Read each HTML file listed in `linkerData.yaml`
2. Encode as UTF-8 bytes
3. Gzip-compress with maximum compression (`gzip.compress(data, compresslevel=9)`)
4. Emit as a C `extern const uint8_t` array and a `size_t` length constant
5. All arrays are collected into a single `autoGenHtmlData.h` header

### Example Output (abbreviated)

```c
// base.html → gzip compressed
extern const uint8_t baseHtmlGz[];
extern const size_t  baseHtmlGzLen;

// In the .cpp or .h:
const uint8_t baseHtmlGz[] = { 0x1f, 0x8b, 0x08, ... };
const size_t  baseHtmlGzLen = 1234;
```

The array name is derived from the file name via `convertToCamelCase("base.html.gz")` → `baseHtmlGz`.

---

## 8. Script: `compileAssets.py`

**Input:** Files declared in `linkerData.yaml` with `rtnType: ASSET` (sourced from `webApp/assets/`)
**Output:** `Src/autoGen/autoGenAssets.h`

### Processing Steps

1. For each asset route in `linkerData.yaml`, read the source file
2. If the file extension is in `TEXT_ASSET_EXTENSIONS` (`.css`, `.js`, `.svg`): minify first
   - `.css` → `minifyCss()`
   - `.js` → `minifyJavaScript()`
3. Gzip-compress with level 9
4. Emit as C byte array with length constant

### Naming

The C array name is derived from the asset `fileName` (relative to `webApp/assets/`):

```python
convertToCamelCase("css/style.css")   # → "cssStyleCss"
# array: cssStyleCss[], cssStyleCssLen
```

---

## 9. Script: `updateWebServer.py`

**Input:** `webApp/linkerData.yaml`, `Src/autoGen/autoGenHtmlData.h`, `Src/autoGen/autoGenAssets.h`
**Output:** `Src/autoGen/autoGenWebServer.h`, `Src/autoGen/autoGenWebServer.cpp`

This is the most complex script. It generates all the HTTP server glue code.

### What It Generates

#### `autoGenWebServer.h`

- `extern httpd_handle_t webServerHttpd;`
- `void startWebServer();` and `void stopWebServer();`
- `typedef enum { PAGE_MACRO_1, PAGE_MACRO_2, ... } webServerMacro;`
- Hook function declarations for every JSON API route:
  ```c
  char* apiLoginHandlerHook(httpd_req_t *req);
  char* apiDevicesToggleHandlerHook(httpd_req_t *req);
  // ... one per JSON route
  ```

#### `autoGenWebServer.cpp`

For each **HTML route**:
```c
static esp_err_t baseHtmlHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    webHandlerHook(BASE_HTML);
    return sendLargeResponse(req, (const char *)baseHtmlGz, baseHtmlGzLen);
}
```

For each **asset route**:
```c
static esp_err_t cssStyleCssHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    httpd_resp_set_hdr(req, "ETag", "\"a3f2e1c\"");  // git short hash
    return sendLargeResponse(req, (const char *)cssStyleCss, cssStyleCssLen);
}
```

For each **JSON API route**:
```c
static esp_err_t apiLoginHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    char *jsonData = apiLoginHandlerHook(req);
    if (jsonData != NULL) {
        esp_err_t result = httpd_resp_sendstr(req, jsonData);
        free(jsonData);
        return result;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
}
```

`startWebServer()` is generated with:
- `httpd_config_t` tuned for the total number of handlers
- One `httpd_uri_t` struct per route
- `httpd_register_uri_handler()` calls for all routes

The ETag value is the short `git rev-parse --short HEAD` hash, captured at build time. If not in a git repo, it falls back to `"dev"`.

---

## 10. Script: `compileOledLogo.py`

**Input:** Logo image file (PNG or BMP, typically in `Assets/`)
**Output:** `Src/autoGen/autoGenOledLogo.h`

Converts a logo image to a 1-bit (monochrome) C byte array suitable for `Adafruit_GFX`'s `drawBitmap()` function.

### Processing Steps

1. Open the image with Pillow
2. Resize to fit within the OLED display (target: 48 × 48 px or configured dimensions)
3. Convert to 1-bit monochrome (`image.convert("1")`)
4. Pack pixels into bytes (MSB first, 8 pixels per byte)
5. Emit as `const uint8_t bootLogo[]` with `BOOT_LOGO_W` and `BOOT_LOGO_H` constants

### Generated Output

```c
#define BOOT_LOGO_W 48
#define BOOT_LOGO_H 48
const uint8_t bootLogo[] PROGMEM = {
    0xFF, 0xFE, 0x00, ...
};
```

---

## 11. Naming Conventions

### Route → C Identifier (API Hook Function Name)

The hook function name is derived from the API route path by the `getFunctionNameFromRoute()` helper in `updateWebServer.py`:

1. Strip the leading `/`
2. Replace all `/` with `_`
3. Apply `convertToCamelCase()` with `_` as separator

| Route | Hook Function |
|---|---|
| `/api/login` | `apiLoginHandlerHook` |
| `/api/devices/add` | `apiDevicesAddHandlerHook` |
| `/api/settings/factory-reset` | `apiSettingsFactoryResetHandlerHook` |
| `/api/devices/automation/save` | `apiDevicesAutomationSaveHandlerHook` |
| `/api/auth/change-password` | `apiAuthChangePasswordHandlerHook` |

### File → C Array Name

HTML and asset file paths are converted to C variable names using `convertToCamelCase()`:

| File / Path | C Array Name |
|---|---|
| `base.html` (gzip) | `baseHtmlGz` |
| `login.html` (gzip) | `loginHtmlGz` |
| `css/style.css` | `cssStyleCss` |
| `js/app.js` | `jsAppJs` |
| `image/webLogo.png` | `imageWeblogoPng` |

### HTML Page Macro Enum

Each HTML route's `macro` field becomes an enum entry in `webServerMacro`:

```yaml
"/page/dashboard":
    macro: "DASHBOARD_HTML"
```

```c
typedef enum {
    BASE_HTML,
    LOGIN_HTML,
    DASHBOARD_HTML,
    WIFI_CONFIG_HTML,
    SETTINGS_HTML,
} webServerMacro;
```

---

## 12. Build Artefacts and Clean

### Generated Files (do not edit manually)

| File | Generated by |
|---|---|
| `Src/autoGen/autoGenDeviceConfig.h` | `compileDeviceConfig.py` |
| `Src/autoGen/autoGenHtmlData.h` | `compileHtml.py` |
| `Src/autoGen/autoGenAssets.h` | `compileAssets.py` |
| `Src/autoGen/autoGenWebServer.h` | `updateWebServer.py` |
| `Src/autoGen/autoGenWebServer.cpp` | `updateWebServer.py` |
| `Src/autoGen/autoGenOledLogo.h` | `compileOledLogo.py` |

### Clean

```bash
make clean
```

Removes:
- `Src/autoGen/` — all generated headers and source files
- `.temp/autoGen/` — intermediate build outputs
- `build/` — compiled firmware

> After `make clean`, you must run `make autogen` (or `make build`) before the firmware will compile.

---

## 13. Extending the Pipeline

### Adding a New Asset Type

To support a new file extension (e.g., `.wasm`):

1. Add the extension and MIME type to `SUPPORTED_ASSET_EXTENSIONS` in `Scripts/common.py`:
   ```python
   '.wasm': 'application/wasm'
   ```
2. If it is a text format, add it to `TEXT_ASSET_EXTENSIONS` to enable minification
3. Declare the route in `linkerData.yaml`
4. Run `make autogen`

### Adding Custom Processing

Each script is self-contained with a `main()` function. You can run scripts individually for debugging:

```bash
.venv/bin/python3 Scripts/compileHtml.py
.venv/bin/python3 Scripts/updateWebServer.py
```

Verbose output shows each file processed and the generated output path.

### Adding a New Script to the Pipeline

1. Create `Scripts/myNewStep.py` with a `main()` function
2. Add the invocation to the `autogen` target in `Makefile`:
   ```makefile
   autogen:
       .venv/bin/python3 Scripts/compileDeviceConfig.py
       .venv/bin/python3 Scripts/compileHtml.py
       .venv/bin/python3 Scripts/compileAssets.py
       .venv/bin/python3 Scripts/updateWebServer.py
       .venv/bin/python3 Scripts/compileOledLogo.py
       .venv/bin/python3 Scripts/myNewStep.py
   ```
