# REST API Reference

This document describes every HTTP endpoint exposed by the ESP32 Device Hub firmware. All API endpoints return `application/json` responses and live under the `/api/` prefix.

---

## Table of Contents

- [Conventions](#conventions)
- [Authentication](#authentication)
  - [POST /api/login](#post-apilogin)
  - [POST /api/logout](#post-apilogout)
  - [POST /api/auth/change-password](#post-apiauthchange-password)
  - [POST /api/auth/set-initial-password](#post-apiauthset-initial-password)
- [Wi-Fi](#wi-fi)
  - [GET /api/wifi/status](#get-apiwifistatus)
  - [GET /api/wifi/scan](#get-apiwifiscan)
  - [POST /api/wifi/connect](#post-apiwificonnect)
- [Settings](#settings)
  - [GET /api/settings](#get-apisettings)
  - [POST /api/settings/save](#post-apisettingssave)
  - [GET /api/settings/backup](#get-apisettingsbackup)
  - [POST /api/settings/restore](#post-apisettingsrestore)
  - [POST /api/settings/factory-reset](#post-apisettingsfactory-reset)
- [Firmware](#firmware)
  - [POST /api/firmware/update](#post-apifirmwareupdate)
- [Devices](#devices)
  - [GET /api/devices](#get-apidevices)
  - [GET /api/device-config](#get-apidevice-config)
  - [POST /api/devices/add](#post-apidevicesadd)
  - [POST /api/devices/remove](#post-apidevicesremove)
  - [POST /api/devices/toggle](#post-apidevicestoggle)
  - [POST /api/devices/rename](#post-apidevicesrename)
  - [GET /api/devices/automation](#get-apidevicesautomation)
  - [POST /api/devices/automation/save](#post-apidevicesautomationsave)
- [System](#system)
  - [POST /api/reboot](#post-apireboot)
- [Error Responses](#error-responses)

---

## Conventions

### Base URL

All requests are relative to the device IP, e.g. `http://192.168.1.42`.

### Authentication

Protected endpoints require the `X-Auth-Token` header set to the session token obtained from `POST /api/login`. Requests without a valid token receive `401 Unauthorized`.

```
X-Auth-Token: a3f2e1c9...  (32-character hex string)
```

The token is stored in RAM only; it is lost on reboot. Sessions expire after a configurable idle period (default 15 minutes, range 1–1440 minutes).

### Request Body

All `POST` endpoints expect `Content-Type: application/json` (except `/api/firmware/update` which expects `application/octet-stream`).

### Response Format

All JSON responses share a common envelope:

```json
{
  "success": true | false,
  "message": "Human-readable status string"
}
```

Additional fields are documented per endpoint.

### HTTP Status Codes

| Code | Meaning |
|---|---|
| `200 OK` | Request succeeded (check `success` field in body) |
| `401 Unauthorized` | Missing or invalid session token |
| `500 Internal Server Error` | Hook function returned `null` |

---

## Authentication

### POST /api/login

Authenticates with the admin password and returns a session token.

**Authentication required:** No

**Request body:**

```json
{
  "password": "your-password"
}
```

**Success response (`success: true`):**

```json
{
  "success": true,
  "token": "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7",
  "mustChangePassword": false
}
```

| Field | Type | Description |
|---|---|---|
| `token` | string | 32-char hex session token; include in subsequent requests as `X-Auth-Token` |
| `mustChangePassword` | boolean | `true` if the admin password is still the factory default; show change-password prompt |

**Failure response (`success: false`):**

```json
{
  "success": false,
  "message": "Incorrect password"
}
```

> **Note:** Failed login attempts incur a 500 ms delay to slow brute-force attacks.

---

### POST /api/logout

Invalidates the current session token.

**Authentication required:** Yes

**Request body:** *(empty object or no body)*

**Success response:**

```json
{
  "success": true,
  "message": "Logged out"
}
```

---

### POST /api/auth/change-password

Changes the admin password. Invalidates the current session (must log in again).

**Authentication required:** Yes

**Request body:**

```json
{
  "current": "current-password",
  "new": "new-password-min-6-chars"
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Password updated. Please log in again."
}
```

**Failure responses:**

```json
{ "success": false, "message": "Current password incorrect" }
{ "success": false, "message": "New password must be at least 6 characters" }
{ "success": false, "message": "current and new fields required" }
```

> After a successful password change, the session token is cleared. The client must redirect to the login page.

---

### POST /api/auth/set-initial-password

Sets the admin password when `mustChangePassword` is `true` (factory default in use). Does not require the current password, but requires a valid session token. Can only be called once — fails if the password has already been configured.

**Authentication required:** Yes

**Request body:**

```json
{
  "password": "new-password-6-to-32-chars"
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Admin password configured"
}
```

**Failure responses:**

```json
{ "success": false, "message": "Initial password is already configured" }
{ "success": false, "message": "Password must be 6 to 32 characters" }
```

---

## Wi-Fi

### GET /api/wifi/status

Returns the current network connectivity status. Does not require authentication.

**Authentication required:** No

**Success response (Ethernet connected):**

```json
{
  "connected": true,
  "ethernet_ip": "192.168.1.42",
  "network": "Ethernet",
  "ssid": "Ethernet",
  "ip": "192.168.1.42",
  "mac": "AA:BB:CC:DD:EE:FF",
  "gateway": "192.168.1.1",
  "subnet": "255.255.255.0"
}
```

**Success response (Wi-Fi connected):**

```json
{
  "connected": true,
  "wifi_ip": "192.168.1.43",
  "network": "WiFi",
  "ssid": "MyHomeNetwork",
  "rssi": -52,
  "ip": "192.168.1.43",
  "mac": "AA:BB:CC:DD:EE:FF",
  "gateway": "192.168.1.1",
  "subnet": "255.255.255.0"
}
```

**Success response (both Ethernet and Wi-Fi active):**

Both `ethernet_ip` and `wifi_ip` are present; `network` and `ip` reflect the **preferred** (Ethernet) interface.

**Success response (not connected):**

```json
{
  "connected": false,
  "network": "None",
  "status": "Disconnected"
}
```

| Field | Type | Description |
|---|---|---|
| `connected` | boolean | `true` if at least one interface has an IP |
| `ethernet_ip` | string | Ethernet IP (only present when Ethernet has IP) |
| `wifi_ip` | string | Wi-Fi IP (only present when Wi-Fi is connected) |
| `network` | string | Active preferred interface: `"Ethernet"`, `"WiFi"`, or `"None"` |
| `ssid` | string | Network name (`"Ethernet"` for wired) |
| `rssi` | number | Signal strength in dBm (Wi-Fi only) |
| `ip` | string | Active interface IP |
| `mac` | string | MAC address of active interface |
| `gateway` | string | Default gateway IP |
| `subnet` | string | Subnet mask |

---

### GET /api/wifi/scan

Triggers a synchronous Wi-Fi network scan and returns the results. Note: this call blocks for the duration of the scan (typically 2–5 seconds).

**Authentication required:** No

**Success response:**

```json
{
  "count": 3,
  "networks": [
    { "ssid": "MyHomeNetwork", "rssi": -48, "encrypted": true },
    { "ssid": "NeighborNetwork", "rssi": -72, "encrypted": true },
    { "ssid": "OpenCoffeeShop", "rssi": -81, "encrypted": false }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `count` | number | Total networks found |
| `networks[].ssid` | string | Network name |
| `networks[].rssi` | number | Signal strength in dBm (higher = stronger) |
| `networks[].encrypted` | boolean | `true` if the network requires a password |

---

### POST /api/wifi/connect

Connects to a Wi-Fi network and saves the credentials to EEPROM.

**Authentication required:** Yes

**Request body:**

```json
{
  "ssid": "MyHomeNetwork",
  "password": "wifi-password"
}
```

Leave `password` empty or omit it for open (unencrypted) networks.

**Success response:**

```json
{
  "success": true,
  "message": "Connected",
  "ip": "192.168.1.43"
}
```

**Failure response:**

```json
{
  "success": false,
  "message": "Failed to connect"
}
```

> Connection attempt times out after 10 seconds. The credentials are only saved if the connection succeeds.

---

## Settings

### GET /api/settings

Returns the current controller settings.

**Authentication required:** Yes

**Success response:**

```json
{
  "success": true,
  "controllerName": "Home Controller",
  "logoutMinutes": 15,
  "oledBrightness": 100,
  "oledEnabled": true,
  "mustChangePassword": false,
  "firmwareVersion": "1.0.1"
}
```

| Field | Type | Description |
|---|---|---|
| `controllerName` | string | Name shown in the web interface header and OLED |
| `logoutMinutes` | number | Session inactivity timeout in minutes |
| `oledBrightness` | number | OLED contrast level (1–255) |
| `oledEnabled` | boolean | Whether the OLED display is active |
| `mustChangePassword` | boolean | `true` if the factory default password is still in use |
| `firmwareVersion` | string | Current firmware version string |

---

### POST /api/settings/save

Saves controller settings to EEPROM.

**Authentication required:** Yes

**Request body:**

```json
{
  "controllerName": "Workshop Hub",
  "logoutMinutes": 30,
  "oledBrightness": 150,
  "oledEnabled": true
}
```

| Field | Type | Constraints |
|---|---|---|
| `controllerName` | string | 1–20 characters |
| `logoutMinutes` | number | 1–1440 |
| `oledBrightness` | number | 1–255 |
| `oledEnabled` | boolean | — |

**Success response:**

```json
{
  "success": true,
  "message": "Settings saved"
}
```

**Failure response:**

```json
{
  "success": false,
  "message": "Enter a controller name and logout time from 1 to 1440 minutes"
}
```

---

### GET /api/settings/backup

Downloads the full device configuration as a JSON object suitable for re-importing via `/api/settings/restore`.

**Authentication required:** Yes

**Success response:**

```json
{
  "controllerName": "Home Controller",
  "logoutMinutes": 15,
  "oledBrightness": 100,
  "oledEnabled": true,
  "devices": [
    { "name": "Bedroom Lamp", "pin": 26, "state": false },
    { "name": "Fan",          "pin": 27, "state": true  }
  ]
}
```

> The backup does not include the admin password or Wi-Fi credentials for security reasons.

---

### POST /api/settings/restore

Restores a configuration backup. Reboots the device after a successful restore.

**Authentication required:** Yes

**Request body:** The JSON object returned by `/api/settings/backup`.

```json
{
  "controllerName": "Home Controller",
  "logoutMinutes": 15,
  "oledBrightness": 100,
  "oledEnabled": true,
  "devices": [
    { "name": "Bedroom Lamp", "pin": 26, "state": false }
  ]
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Backup restored; rebooting"
}
```

**Failure responses:**

```json
{ "success": false, "message": "Invalid backup file" }
{ "success": false, "message": "Invalid backup settings" }
```

> Pins in the backup are validated against `CFG_HIGH_VOLTAGE_PINS` and `CFG_LOW_VOLTAGE_PINS`. Any device with an unrecognised pin causes the entire restore to fail.

---

### POST /api/settings/factory-reset

Erases all settings and devices from EEPROM, then reboots. **This is irreversible.**

**Authentication required:** Yes

**Request body:**

```json
{
  "password": "current-admin-password"
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Factory reset; rebooting"
}
```

**Failure responses:**

```json
{ "success": false, "message": "Admin password required" }
{ "success": false, "message": "Incorrect admin password" }
```

> After a factory reset the device boots as if new: no Wi-Fi saved, admin password reset to MAC-derived default, OLED shows default "Home Controller" name.

---

## Firmware

### POST /api/firmware/update

Uploads a new firmware binary and flashes it via the Arduino `Update` library. The device reboots automatically on success.

**Authentication required:** Yes

**Request:**
- `Content-Type: application/octet-stream`
- Body: raw `.bin` firmware file bytes
- `X-Auth-Token: <token>` header

> The web interface uploads firmware with `XMLHttpRequest` and streams the file in chunks to display upload progress. Direct `curl` upload is also supported:
> ```bash
> curl -X POST http://192.168.1.42/api/firmware/update \
>      -H "X-Auth-Token: <token>" \
>      -H "Content-Type: application/octet-stream" \
>      --data-binary @firmware.bin
> ```

**Success response:**

```json
{
  "success": true,
  "message": "Firmware uploaded. Rebooting..."
}
```

**Failure responses:**

```json
{ "success": false, "message": "Firmware file is empty" }
{ "success": false, "message": "Not enough space for firmware update" }
{ "success": false, "message": "Firmware update failed" }
```

> The firmware validates the ESP32 image header (`0xE9` magic byte in first byte). Invalid images are rejected and the update is aborted cleanly.

---

## Devices

### GET /api/devices

Returns the list of all configured devices.

**Authentication required:** No

**Success response:**

```json
{
  "count": 2,
  "devices": [
    { "index": 0, "name": "Bedroom Lamp", "pin": 26, "state": false },
    { "index": 1, "name": "Fan",          "pin": 27, "state": true  }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `count` | number | Total number of configured devices |
| `devices[].index` | number | Zero-based device index (used in toggle/remove/rename calls) |
| `devices[].name` | string | Device label (up to 15 characters) |
| `devices[].pin` | number | GPIO output pin number |
| `devices[].state` | boolean | `true` = ON (GPIO HIGH), `false` = OFF (GPIO LOW) |

---

### GET /api/device-config

Returns the available GPIO pins and device capacity.

**Authentication required:** No

**Success response:**

```json
{
  "maxDevices": 8,
  "currentDevices": 2,
  "highVoltagePins": [26, 27, 25],
  "lowVoltagePins": [33, 32, 17, 15, 5],
  "availableHighVoltagePins": [25],
  "availableLowVoltagePins": [33, 32, 17, 15, 5]
}
```

| Field | Type | Description |
|---|---|---|
| `maxDevices` | number | Maximum configurable devices (from `deviceConfig.yaml`) |
| `currentDevices` | number | How many devices are currently configured |
| `highVoltagePins` | array | All high-voltage GPIO pins declared in config |
| `lowVoltagePins` | array | All low-voltage GPIO pins declared in config |
| `availableHighVoltagePins` | array | High-voltage pins not yet assigned to a device |
| `availableLowVoltagePins` | array | Low-voltage pins not yet assigned to a device |

---

### POST /api/devices/add

Adds a new device. The device starts in the OFF state.

**Authentication required:** Yes

**Request body:**

```json
{
  "name": "Bedroom Lamp",
  "pin": 26,
  "voltage": "high"
}
```

| Field | Type | Constraints |
|---|---|---|
| `name` | string | 1–15 characters |
| `pin` | number | Must be in `availableHighVoltagePins` or `availableLowVoltagePins` |
| `voltage` | string | `"high"` or `"low"` — must match the pin's group |

**Success response:**

```json
{
  "success": true,
  "message": "Device added",
  "index": 2
}
```

| Field | Description |
|---|---|
| `index` | Zero-based index of the newly added device |

**Failure responses:**

```json
{ "success": false, "message": "Maximum device limit reached" }
{ "success": false, "message": "name, voltage (high/low), and an available GPIO pin are required" }
```

---

### POST /api/devices/remove

Removes a device by index. The GPIO pin is set LOW before removal.

**Authentication required:** Yes

**Request body:**

```json
{
  "index": 2
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Device removed"
}
```

**Failure responses:**

```json
{ "success": false, "message": "Index out of range" }
{ "success": false, "message": "index (number) required" }
```

> After removing a device, all subsequent devices shift down by one index. Re-fetch the device list before making further index-based calls.

---

### POST /api/devices/toggle

Sets a device ON or OFF.

**Authentication required:** Yes

**Request body:**

```json
{
  "index": 0,
  "state": true
}
```

| Field | Type | Description |
|---|---|---|
| `index` | number | Zero-based device index |
| `state` | boolean | `true` = turn ON (GPIO HIGH), `false` = turn OFF (GPIO LOW) |

**Success response:**

```json
{
  "success": true,
  "index": 0,
  "state": true
}
```

**Failure responses:**

```json
{ "success": false, "message": "Index out of range" }
{ "success": false, "message": "index (number) and state (bool) required" }
```

> The new state is persisted to EEPROM and the OLED status is refreshed.

---

### POST /api/devices/rename

Renames an existing device.

**Authentication required:** Yes

**Request body:**

```json
{
  "index": 0,
  "name": "Living Room Lamp"
}
```

**Success response:**

```json
{
  "success": true,
  "message": "Device renamed",
  "index": 0,
  "name": "Living Room Lamp"
}
```

**Failure responses:**

```json
{ "success": false, "message": "Index out of range" }
{ "success": false, "message": "Name must be 1–15 characters" }
{ "success": false, "message": "index (number) and name (string) required" }
```

---

### GET /api/devices/automation

Returns the ping watchdog (automation) configuration for all devices.

**Authentication required:** Yes

**Success response:**

```json
{
  "success": true,
  "automations": [
    {
      "index": 0,
      "autoEnabled": true,
      "pingInterval": 30,
      "pingIp": "192.168.1.100",
      "cycling": false
    },
    {
      "index": 1,
      "autoEnabled": false,
      "pingInterval": 30,
      "pingIp": "",
      "cycling": false
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `index` | number | Device index |
| `autoEnabled` | boolean | Whether the ping watchdog is active for this device |
| `pingInterval` | number | Seconds between ICMP pings (5–255) |
| `pingIp` | string | IPv4 address to ping |
| `cycling` | boolean | `true` if the device is currently in a power-cycle (OFF phase) |

---

### POST /api/devices/automation/save

Saves ping watchdog settings for a single device.

**Authentication required:** Yes

**Request body:**

```json
{
  "index": 0,
  "autoEnabled": true,
  "pingInterval": 30,
  "pingIp": "192.168.1.100"
}
```

| Field | Type | Constraints |
|---|---|---|
| `index` | number | Valid device index |
| `autoEnabled` | boolean | Enable or disable the watchdog |
| `pingInterval` | number | 5–255 seconds (required when `autoEnabled: true`) |
| `pingIp` | string | Valid IPv4 address, max 15 characters (required when `autoEnabled: true`) |

**Success response:**

```json
{
  "success": true,
  "message": "Automation settings saved"
}
```

When `autoEnabled: false`:

```json
{
  "success": true,
  "message": "Automation disabled"
}
```

**Failure responses:**

```json
{ "success": false, "message": "index, autoEnabled, pingInterval, pingIp required" }
{ "success": false, "message": "Index out of range" }
{ "success": false, "message": "pingInterval must be 5–255 seconds" }
{ "success": false, "message": "pingIp must be a valid IP address (max 15 chars)" }
```

---

## System

### POST /api/reboot

Schedules a device reboot (800 ms after the HTTP response is sent).

**Authentication required:** Yes

**Request body:** *(none)*

**Success response:**

```json
{
  "success": true,
  "message": "Rebooting..."
}
```

> The device becomes unreachable for a few seconds while it reboots. The session token is cleared on reboot (RAM only). The client should redirect to `/login` after detecting the device is back online.

---

## Error Responses

### 401 Unauthorized

Returned by all protected endpoints when the `X-Auth-Token` header is missing, invalid, or the session has timed out.

```json
{
  "success": false,
  "message": "Unauthorized"
}
```

The client should redirect to `/login` and remove the stored token from `localStorage`.

### 500 Internal Server Error

Returned when the hook function returned `null` (typically an allocation failure or parse error). The response body is a plain string from the ESP-IDF HTTP server, not JSON.

### Invalid JSON

If the request body cannot be parsed, the response is:

```json
{
  "success": false,
  "message": "Invalid JSON"
}
```

---

## Quick Reference Table

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/login` | POST | No | Log in, get token |
| `/api/logout` | POST | Yes | Invalidate session |
| `/api/auth/change-password` | POST | Yes | Change admin password |
| `/api/auth/set-initial-password` | POST | Yes | Set first-time password |
| `/api/wifi/status` | GET | No | Network status |
| `/api/wifi/scan` | GET | No | Scan Wi-Fi networks |
| `/api/wifi/connect` | POST | Yes | Join a Wi-Fi network |
| `/api/settings` | GET | Yes | Read settings |
| `/api/settings/save` | POST | Yes | Save settings |
| `/api/settings/backup` | GET | Yes | Download config backup |
| `/api/settings/restore` | POST | Yes | Restore config backup |
| `/api/settings/factory-reset` | POST | Yes | Erase all + reboot |
| `/api/firmware/update` | POST | Yes | OTA firmware upload |
| `/api/devices` | GET | No | List devices |
| `/api/device-config` | GET | No | Pin availability |
| `/api/devices/add` | POST | Yes | Add device |
| `/api/devices/remove` | POST | Yes | Remove device |
| `/api/devices/toggle` | POST | Yes | Toggle ON/OFF |
| `/api/devices/rename` | POST | Yes | Rename device |
| `/api/devices/automation` | GET | Yes | Read watchdog config |
| `/api/devices/automation/save` | POST | Yes | Save watchdog config |
| `/api/reboot` | POST | Yes | Reboot device |
