# ESP32 Web Server Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Web Server                     │
│                                                         │
│  ┌───────────────┐           ┌──────────────────────┐   │
│  │  WiFi Client  │◄─────────►│   HTTP Web Server    │   │
│  │   (Browser)   │  HTTP     │  (esp_http_server)   │   │
│  └───────────────┘  Request  └──────────┬───────────┘   │
│                                         │               │
│                              ┌──────────▼────────────┐  │
│                              │  webServerSD.cpp      │  │
│                              │  Route Handler        │  │
│                              └───────────┬───────────┘  │
│                                          │              │
│                        ┌─────────────────┴──────────┐   │
│                        │                            │   │
│           ┌────────────▼────────┐      ┌────────────▼─┐ │
│           │  SD Card Handler    │      │  Compiled    │ │
│           │  sdCardHandler.cpp  │      │  HTML Arrays │ │
│           └────────────┬────────┘      └────────────┬─┘ │
│                        │                            │   │
│                 ┌──────▼──────┐                     │   │
│                 │   SD Card   │                     │   │
│                 │   Module    │                     │   │
│                 │  (SPI Bus)  │                     │   │
│                 └─────────────┘                     │   │
│                                                     │   │
│                   ┌─────────────────────────────────┘   │
│                   │                                     │
│           ┌───────▼────────┐                            │
│           │  HTTP Response │                            │
│           │   (Chunked)    │                            │
│           └────────────────┘                            │
└─────────────────────────────────────────────────────────┘
```

## Request Flow

### SD Card Mode (Enabled)

```
Browser Request (/login)
        ↓
    Route Handler (loginHandlerSD)
        ↓
    genericHtmlHandler()
        ↓
    [Check: useSDCardForHTML?]
        ↓
    [YES] → readFileFromSD("/html/login.html")
        ↓
    [File Found?]
        ↓
    [YES] → sendLargeResponse() → Browser
        │
    [NO] → Fall back to compiled HTML (loginHtmlGz)
        ↓
    sendLargeResponse() → Browser
```

### Compiled Mode (Fallback)

```
Browser Request (/login)
        ↓
    Route Handler (loginHandlerSD)
        ↓
    genericHtmlHandler()
        ↓
    [Check: useSDCardForHTML?]
        ↓
    [NO] → Use compiled HTML (loginHtmlGz)
        ↓
    sendLargeResponse() → Browser
```

## Component Relationships

```
┌──────────────────────────────────────────────────────────────┐
│                    Initialization Layer                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  startWebServerWithSD(enable, csPin)                         │
│         │                                                    │
│         ├─► initSDCard(csPin) ────► SD Card Hardware        │
│         │                                                    │
│         ├─► listSDDir("/html/") ──► Verify Files            │
│         │                                                    │
│         └─► httpd_start() ────────► Start HTTP Server       │
│                                                              │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│                      Request Layer                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  HTTP Request                                                │
│         │                                                    │
│         ├─► dashboardHandlerSD() ──► Dashboard Page         │
│         ├─► homeHandlerSD() ────────► Home Page             │
│         ├─► indexHandlerSD() ───────► Index Page            │
│         ├─► loginHandlerSD() ───────► Login Page            │
│         ├─► logoutHandlerSD() ──────► Logout Page           │
│         │                                                    │
│         ├─► apiLoginHandlerSD() ────► Login API             │
│         └─► apiRegisterHandlerSD() ─► Register API          │
│                                                              │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│                    Data Source Layer                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  genericHtmlHandler()                                        │
│         │                                                    │
│         ├─► [SD Mode] ──► readFileFromSD()                  │
│         │                      │                            │
│         │                      ├─► SD.open()                │
│         │                      ├─► file.read()              │
│         │                      └─► file.close()             │
│         │                                                    │
│         └─► [Compiled] ─► xxxHtmlGz[] Array                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│                    Response Layer                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  sendLargeResponse(req, data, size)                          │
│         │                                                    │
│         └─► Loop: httpd_resp_send_chunk()                   │
│                   │                                          │
│                   ├─► Chunk 1 (1KB)                          │
│                   ├─► Chunk 2 (1KB)                          │
│                   ├─► ...                                    │
│                   └─► Final Chunk (0 bytes = end)            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## File Organization

```
Source Files (html/)
        │
        ├─► compileHtml.py ────────► AutoGen/autoGenHtmlData.h
        │                            (Compiled arrays for fallback)
        │
        └─► prepareSDCard.py ──────► sd_card_files/html/*.html
                                     (Files for SD card)
```

## Build Process Comparison

### Traditional Build (Compiled HTML)
```
html/*.html
    ↓
Scripts/compileHtml.py
    ↓
.temp/AutoGen/autoGenHtmlData.h
    ↓
Src/AutoGen/autoGenHtmlData.h
    ↓
Compile with firmware
    ↓
Flash to ESP32
    ↓
HTML embedded in firmware
```

### SD Card Build
```
html/*.html
    ↓
Scripts/prepareSDCard.py
    ↓
sd_card_files/html/*.html
    ↓
Copy to SD Card
    ↓
Insert in ESP32
    ↓
Read at runtime
    ↓
HTML stored on SD card
```

## Memory Usage

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Memory Map                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Flash Memory (Firmware)                                │
│  ├─ Code (~200KB)                                       │
│  ├─ Libraries (~100KB)                                  │
│  ├─ [Compiled Mode]: HTML Arrays (~50KB)                │
│  └─ [SD Mode]: Only code (~20KB)                        │
│                                                         │
│  SRAM (Runtime)                                         │
│  ├─ Stack (~8KB)                                        │
│  ├─ Global variables (~10KB)                            │
│  ├─ WiFi (~30KB)                                        │
│  ├─ HTTP Server (~20KB)                                 │
│  └─ [SD Mode]: File buffer (~10KB temporary)            │
│                                                         │
│  SD Card (External Storage)                             │
│  └─ HTML Files (~30KB total)                            │
│      ├─ index.html (314B)                               │
│      ├─ home.html (12KB)                                │
│      ├─ dashboard.html (6KB)                            │
│      ├─ login.html (7.5KB)                              │
│      └─ logout.html (205B)                              │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Development Workflow

```
┌─────────────────────┐     ┌─────────────────────┐
│   Compiled Mode     │     │    SD Card Mode     │
│   (Production)      │     │   (Development)     │
└─────────────────────┘     └─────────────────────┘
          │                            │
          ▼                            ▼
    Edit HTML file              Edit HTML file
          │                            │
          ▼                            ▼
    make autogen              make sd-prepare
          │                            │
          ▼                            ▼
    make build               Copy to SD card
          │                            │
          ▼                            ▼
    make flash                  Reset ESP32
          │                            │
          ▼                            ▼
    Wait 2-5 min              Wait 10 sec
          │                            │
          └──────────┬─────────────────┘
                     ▼
                Test changes
```

## Error Handling Flow

```
startWebServerWithSD(true, 5)
    ↓
initSDCard(5)
    │
    ├─► [Success] ─────────────────► useSDCardForHTML = true
    │                                        │
    │                                        ▼
    │                              Serve from SD card
    │                                        │
    │                              [File found?]
    │                                    │   │
    │                                    │   └─► [NO] ──┐
    │                                    ▼              │
    │                              [YES] Serve file     │
    │                                                   │
    └─► [Fail] ──────────────────► useSDCardForHTML = false
                                           │            │
                                           ▼            │
                                   Serve compiled ◄────┘
                                   (Automatic fallback)
```

## Performance Characteristics

```
                    Compiled Mode    SD Card Mode
                    ─────────────    ────────────
First Request:         <10ms           100-200ms
Subsequent:            <10ms           100-200ms
Flash Usage:           +50KB           +20KB
SRAM Usage:            Medium          Medium
Update Time:           2-5 min         10 sec
Reliability:           ★★★★★           ★★★★☆
Dev Speed:             ★★★☆☆           ★★★★★
```

## Key Classes and Functions

```
┌──────────────────────────────────────────────────────────┐
│ sdCardHandler.cpp                                        │
├──────────────────────────────────────────────────────────┤
│ • initSDCard(csPin) → bool                               │
│   Initialize SD card hardware                            │
│                                                          │
│ • readFileFromSD(path, size) → char*                     │
│   Read entire file into memory                           │
│                                                          │
│ • fileExistsOnSD(path) → bool                            │
│   Check if file exists                                   │
│                                                          │
│ • listSDDir(path, levels)                                │
│   List directory contents                                │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│ webServerSD.cpp                                          │
├──────────────────────────────────────────────────────────┤
│ • startWebServerWithSD(enable, csPin)                    │
│   Initialize and start web server                        │
│                                                          │
│ • genericHtmlHandler(req, filename, fallback, macro)     │
│   Smart handler with SD + fallback logic                 │
│                                                          │
│ • dashboardHandlerSD(req) → esp_err_t                    │
│ • homeHandlerSD(req) → esp_err_t                         │
│ • indexHandlerSD(req) → esp_err_t                        │
│ • loginHandlerSD(req) → esp_err_t                        │
│ • logoutHandlerSD(req) → esp_err_t                       │
│   Individual page handlers                               │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│ webServerHelper.cpp                                      │
├──────────────────────────────────────────────────────────┤
│ • sendLargeResponse(req, data, size) → esp_err_t         │
│   Send data in chunks (1KB each)                         │
│                                                          │
│ • serveFileFromSD(req, path, type) → esp_err_t           │
│   Complete SD file serving function                      │
│                                                          │
│ • getContentFromReq(req) → char*                         │
│   Extract POST data from request                         │
└──────────────────────────────────────────────────────────┘
```

This architecture provides flexibility, reliability, and excellent developer experience!
