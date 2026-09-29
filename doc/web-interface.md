# Web Interface Guide

This document describes the browser-based web application served by the ESP32 Device Hub firmware: its pages, navigation, authentication flow, theming, and the client-side JavaScript API layer.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Authentication Flow](#2-authentication-flow)
3. [SPA Shell (`base.html`)](#3-spa-shell-basehtml)
4. [Login Page](#4-login-page)
5. [Dashboard Page](#5-dashboard-page)
6. [Settings Page](#6-settings-page)
7. [Wi-Fi Configuration](#7-wi-fi-configuration)
8. [Theming — Dark/Light Mode and Accent Colours](#8-theming--darklight-mode-and-accent-colours)
9. [JavaScript API Client (`app.js`)](#9-javascript-api-client-appjs)
10. [Mobile Support](#10-mobile-support)

---

## 1. Architecture Overview

The web interface is a **Single-Page Application (SPA)** served entirely by the ESP32 itself. There is no external CDN, no build-time JavaScript bundler, and no server-side rendering. Every asset — HTML fragments, CSS, and JS — is compiled into gzip-compressed C byte arrays at build time and embedded directly in the firmware binary.

```
Browser
  └─ GET /              ← base.html (full SPA shell: header, modals, JS router)
       │
       ├─ /css/style.css      (all styles, gzip, long-cache)
       ├─ /js/app.js          (API client + utilities, gzip, long-cache)
       │
       └─ SPA router: fetch page fragment on hash change
            ├─ GET /page/dashboard   ← dashboard.html fragment
            ├─ GET /page/settings    ← settings.html fragment
            └─ GET /page/wifi        ← wifiConfig.html fragment
```

### Caching Strategy

| Resource type | Cache-Control | ETag |
|---|---|---|
| HTML pages | `no-cache, no-store, must-revalidate` | No |
| CSS / JS / images | `public, max-age=31536000, immutable` | Git hash |

HTML pages are never cached (they may be re-rendered with updated device state). Static assets are cached indefinitely — the asset URLs include a version query string (e.g., `/css/style.css?v=48`) which acts as a cache-buster when the build number changes.

---

## 2. Authentication Flow

```
Browser loads /
     │
     ├─ localStorage has "authToken"?
     │    YES ─── continue to load SPA shell
     │    NO  ─── window.location.replace('/login')
     │
     ▼
Load SPA shell (base.html)
     │
     ├─ GET /api/settings → check mustChangePassword
     │       YES → show "Set Admin Password" modal (blocks UI)
     │       NO  → continue normally
     │
     └─ SPA ready; router loads dashboard fragment
```

### Session Token Storage

The session token is stored in `localStorage` under the key `authToken`. It is a 32-character hex string returned by `POST /api/login`.

Every API call includes the header:
```
X-Auth-Token: <token from localStorage>
```

When any API call returns `401 Unauthorized`, the client removes the token from `localStorage` and redirects to `/login`.

### Inactivity Timeout

The server enforces the idle timeout server-side. The client does not need to track the timeout independently — a `401` response on any protected endpoint is the signal that the session has expired.

---

## 3. SPA Shell (`base.html`)

**Route:** `GET /`

The shell is the only full HTML document served. All other "pages" are HTML fragments loaded into the `<div id="app">` container via `fetch()`.

### Components in the Shell

| Element | ID / Class | Purpose |
|---|---|---|
| Alert banner container | `#alertContainer` | Floating alerts shown by `Utils.showAlert()` |
| Page content area | `#app` | Dynamic page fragment injection target |
| Header | `.header` | Controller name, Wi-Fi badge, theme toggle, avatar menu |
| Wi-Fi status badge | `#wifiStatus` | Shows `Ethernet: x.x.x.x · WiFi: x.x.x.x` or "Not connected" |
| Theme toggle button | `#themeToggle` | Switches dark/light theme; persists to `localStorage` |
| Avatar dropdown | `#avatarMenu` | Links to Dashboard, Settings; Reboot and Logout buttons |
| Bottom navigation | `#bottomNav` | Mobile tab bar (Dashboard, Settings icons) |

### Modals in the Shell

These modals are global — accessible from any page:

| Modal ID | Trigger | Purpose |
|---|---|---|
| `#changePwModal` | Settings → Security → "Change Admin Password" | Change admin password |
| `#initialPasswordModal` | Auto-shown when `mustChangePassword: true` | First-time password setup; blocks navigation |
| `#factoryResetModal` | Settings → System → "Factory Reset" | Confirm factory reset with password |
| `#rebootModal` | Avatar menu → "Reboot Device" | Confirm and execute reboot |

### SPA Router

The router reads `window.location.hash` and fetches the corresponding page fragment:

```javascript
const PAGES = {
    '':          '/page/dashboard',
    'dashboard': '/page/dashboard',
    'wifi':      '/page/wifi',
    'settings':  '/page/settings',
};
```

Navigating to `http://device-ip/#settings` loads the settings fragment into `#app`. The `hashchange` event drives navigation; no page reload occurs.

Fragment scripts are re-executed after injection by cloning each `<script>` tag into a new element.

---

## 4. Login Page

**Route:** `GET /login`

The login page is a standalone full HTML page (not a fragment), served with `no-cache` headers.

### Features

- Password input with show/hide toggle
- `POST /api/login` on form submit
- On success: token stored in `localStorage('authToken')`, redirect to `/`
- On failure: error message shown inline with a "shake" animation
- If `mustChangePassword: true` in the login response, the "Set Admin Password" modal opens automatically after redirect

---

## 5. Dashboard Page

**Route:** `GET /page/dashboard` (loaded as SPA fragment via `#dashboard` or `#`)

The dashboard is the main operational view of the Device Hub. It displays all configured GPIO devices and allows control from the browser.

### Device Cards

Each configured device is shown as a card grouped into two sections:
- **High Voltage Devices** — devices on high-voltage GPIO pins
- **Low Voltage Devices** — devices on low-voltage GPIO pins

Each card shows:

| Element | Description |
|---|---|
| Device name | Human-readable label (editable via Rename) |
| GPIO pin | Small badge showing "GPIO 26" etc. |
| Automation badge | "⚡ Auto" when ping watchdog is enabled; "⟳ Cycling" when a power-cycle is in progress |
| ON/OFF toggle switch | Toggle the GPIO output in real time |
| 3-dot menu | Opens dropdown with: Rename, Ping Watchdog, Remove Device |
| Cycle bar | Thin animated bar at the bottom of the card during a power-cycle |

### Add Device

Click **+ Add Device** to open the Add Device modal:

1. Enter a **device name** (max 15 characters)
2. Select **voltage type** (High / Low)
3. Select an **available GPIO pin** — the dropdown only shows unassigned pins for the selected voltage group
4. Optionally enable **Ping Watchdog** and enter the target IP and check interval
5. Click **Add Device** → `POST /api/devices/add`

If the device limit has been reached, the button is disabled and labelled "Add Device (limit reached)".

### Rename Device

Click the 3-dot menu → **Rename** to open the rename modal. The current name is pre-filled. Pressing Enter saves.

### Ping Watchdog Configuration

Click the 3-dot menu → **⚡ Ping Watchdog**:

- **Enable ping watchdog** toggle
- **Target IP address** — the IPv4 address to ICMP-ping
- **Check interval** — how often to ping (5–255 seconds)

When the watchdog is active and the device is ON, the ESP32 pings the target IP. If no reply is received, the device is power-cycled (OFF for 5 seconds, then back ON). The "⟳ Cycling" badge and animated bar appear during the off phase.

### Background Status Poll

The dashboard polls `GET /api/devices` and `GET /api/devices/automation` every **5 seconds**. It patches existing card elements in-place (no DOM rebuild) so open modals and menus are not disturbed.

---

## 6. Settings Page

**Route:** `GET /page/settings` (loaded as SPA fragment via `#settings`)

The settings page is divided into sections accessible via a sidebar (desktop) or a dropdown menu (mobile).

### General

- **Controller Name** — the name shown in the header and on the OLED display (max 20 characters)
- **Save Settings** — `POST /api/settings/save`

### Display

- **OLED Brightness** — range slider (1–255)
- **Enable OLED** — toggle to turn the physical display on/off
- Saved via the same `POST /api/settings/save` endpoint

### Appearance

Client-side only (stored in `localStorage`, not sent to the device):

- **Theme** — System / Light / Dark; applies `data-theme` attribute to `<html>`
- **Accent colour** — palette picker (Green, Ocean Blue, Amber, Rose); applies `data-palette` to `<html>`

### Security

- **Auto Logout (minutes)** — session inactivity timeout (1–1440); saved with `POST /api/settings/save`
- **Change Admin Password** — opens the global `#changePwModal`

### Network

- Current network status card (IP, MAC, gateway, signal strength)
- **Scan for Networks** — `GET /api/wifi/scan`
- Clickable network list — populates the SSID field
- Wi-Fi connect form — `POST /api/wifi/connect`

### Backup & Restore

- **Backup Configuration** — `GET /api/settings/backup` → downloads `controller-config.json`
- **Restore Configuration** — file picker → reads JSON → confirms → `POST /api/settings/restore` (triggers reboot)

### Firmware Update

- Drag-and-drop or file picker for `.bin` files
- Upload progress bar (streamed via `XMLHttpRequest`)
- Reboot countdown after successful upload
- `POST /api/firmware/update` with `Content-Type: application/octet-stream`

### System

- **Factory Reset** — opens `#factoryResetModal`; requires admin password; triggers erase + reboot
- **Firmware version** display

---

## 7. Wi-Fi Configuration

**Route:** `GET /page/wifi` (SPA fragment; accessible via `#wifi` hash)

This is a standalone Wi-Fi setup page, primarily used during initial setup from the captive portal (AP mode) before a saved network exists. It provides the same network scan and connect functionality as Settings → Network.

---

## 8. Theming — Dark/Light Mode and Accent Colours

### Theme

The theme is controlled by the `data-theme` attribute on `<html>` (`"light"` or `"dark"`).

A small inline script at the top of `base.html` applies the theme synchronously before any CSS renders, preventing a flash of the wrong theme:

```javascript
(function(){
    var t = localStorage.getItem('theme');
    var dark = t === 'dark' || (!t && matchMedia('(prefers-color-scheme:dark)').matches);
    document.documentElement.setAttribute('data-theme', dark ? 'dark' : 'light');
    var p = localStorage.getItem('palette');
    document.documentElement.setAttribute('data-palette',
        p === 'blue' || p === 'amber' || p === 'rose' ? p : 'green');
})();
```

When no preference is saved, the theme follows the OS `prefers-color-scheme` media query.

### Accent Colours

Four accent palettes are available, controlled by `data-palette` on `<html>`:

| Value | Colour |
|---|---|
| `green` *(default)* | Natural green |
| `blue` | Ocean blue |
| `amber` | Warm amber/yellow |
| `rose` | Pink/rose |

All colour tokens are CSS custom properties (`--color-primary`, `--color-primary-hover`, etc.) defined in `style.css` for each `[data-palette]` selector. Changing the palette attribute is instant — no reload required.

---

## 9. JavaScript API Client (`app.js`)

The `api` object in [`webApp/assets/js/app.js`](../webApp/assets/js/app.js) wraps all HTTP calls and handles token injection, error normalisation, and `401` redirects automatically.

### Core Methods

```javascript
api.get(path)                          // GET request with auth header
api.post(path, body)                   // POST JSON with auth header
```

Both methods return `{ success: boolean, data: any }`. On network error, `success` is `false` and `data` contains the error. On `401`, the token is cleared and the browser redirects to `/login`.

### Device Methods

```javascript
api.getDevices()                           // GET /api/devices
api.getDeviceConfig()                      // GET /api/device-config
api.addDevice(name, pin, voltage)          // POST /api/devices/add
api.removeDevice(index)                    // POST /api/devices/remove
api.toggleDevice(index, state)             // POST /api/devices/toggle
api.renameDevice(index, name)              // POST /api/devices/rename
api.getDeviceAutomation()                  // GET /api/devices/automation
api.saveDeviceAutomation(index, autoEnabled, pingInterval, pingIp)
                                           // POST /api/devices/automation/save
```

### Auth Methods

```javascript
api.login(password)                         // POST /api/login
api.logout()                                // POST /api/logout
api.changePassword(current, newPw)          // POST /api/auth/change-password
api.setInitialPassword(password)            // POST /api/auth/set-initial-password
```

### Settings Methods

```javascript
api.getSettings()                           // GET /api/settings
api.getSettingsBackup()                     // GET /api/settings/backup
api.restoreSettings(backupObject)           // POST /api/settings/restore
api.factoryReset(password)                  // POST /api/settings/factory-reset
```

### Wi-Fi Methods

```javascript
api.getWiFiStatus()                         // GET /api/wifi/status
api.scanWiFiNetworks()                      // GET /api/wifi/scan
api.connectToWiFi(ssid, password)           // POST /api/wifi/connect
```

### System Methods

```javascript
api.reboot()                                // POST /api/reboot
```

### Utility Helpers (`Utils` object)

```javascript
Utils.showAlert(message, type, containerId)
    // Inserts a dismissible alert ('success', 'danger', 'info', 'warning')
    // into the #alertContainer (or a named container for modal alerts)

Utils.formatBytes(bytes)
    // Formats a byte count as a human-readable string ("1.2 MB")
```

---

## 10. Mobile Support

The web interface is fully responsive:

- Cards switch from a multi-column grid to single-column on small screens
- The settings sidebar collapses to a `<select>` dropdown
- The header avatar menu collapses to icons
- The **bottom navigation bar** (Dashboard / Settings tabs) appears on small viewports
- Touch events work for all buttons, toggles, and modals

No native app is required — the PWA-friendly design means the interface can be added to the iOS home screen (with the `apple-touch-icon.png` used as the icon) or installed as a Chrome/Android PWA.
