# API Usage Examples

This guide shows practical, copy-paste examples for controlling the ESP32 Device Hub programmatically using Python scripts and `curl` commands. No deep knowledge of HTTP or networking is needed — just follow the examples and substitute your own device IP and password.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Authentication — Getting a Token](#2-authentication--getting-a-token)
3. [Listing and Reading Devices](#3-listing-and-reading-devices)
4. [Toggling a Device On or Off](#4-toggling-a-device-on-or-off)
5. [Turning All Devices Off](#5-turning-all-devices-off)
6. [Adding and Removing a Device](#6-adding-and-removing-a-device)
7. [Reading and Saving Settings](#7-reading-and-saving-settings)
8. [Backing Up the Configuration](#8-backing-up-the-configuration)
9. [Checking Network Status](#9-checking-network-status)
10. [Rebooting the Hub](#10-rebooting-the-hub)
11. [A Reusable Python Helper Class](#11-a-reusable-python-helper-class)
12. [Scheduling Tasks with cron / Task Scheduler](#12-scheduling-tasks-with-cron--task-scheduler)
13. [curl Quick-Reference](#13-curl-quick-reference)

---

## 1. Prerequisites

### Python

Python 3.7 or newer. The examples use only the **`requests`** library:

```bash
pip install requests
```

### curl

`curl` is pre-installed on macOS and most Linux distributions. On Windows, it is included with Git Bash and Windows 10+.

### Hub IP Address

Replace `192.168.1.42` in every example with your actual hub IP. You can find it on the OLED display or in your router's device list.

---

## 2. Authentication — Getting a Token

Every API call (except reading device state and Wi-Fi status) requires a session token. Get one by logging in first.

### Python

```python
import requests

HUB = "http://192.168.1.42"
PASSWORD = "your-admin-password"

resp = requests.post(f"{HUB}/api/login", json={"password": PASSWORD})
data = resp.json()

if data["success"]:
    TOKEN = data["token"]
    print("Logged in. Token:", TOKEN)
else:
    print("Login failed:", data["message"])
```

### curl

```bash
curl -s -X POST http://192.168.1.42/api/login \
  -H "Content-Type: application/json" \
  -d '{"password": "your-admin-password"}'
```

**Response:**

```json
{
  "success": true,
  "token": "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7",
  "mustChangePassword": false
}
```

Save the `token` value — you will include it in the `X-Auth-Token` header of every subsequent request.

> **Token lifetime:** The token expires after the configured inactivity timeout (default 15 minutes). If you get a `401 Unauthorized` response, log in again to get a fresh token.

---

## 3. Listing and Reading Devices

Get all devices and their current ON/OFF states — **no authentication required**.

### Python

```python
import requests

HUB = "http://192.168.1.42"

resp = requests.get(f"{HUB}/api/devices")
data = resp.json()

print(f"Total devices: {data['count']}")
for device in data["devices"]:
    state = "ON" if device["state"] else "OFF"
    print(f"  [{device['index']}] {device['name']} (GPIO {device['pin']}) — {state}")
```

**Example output:**

```
Total devices: 3
  [0] Bedroom Lamp (GPIO 26) — ON
  [1] Desk Fan (GPIO 27) — OFF
  [2] Router (GPIO 25) — ON
```

### curl

```bash
curl -s http://192.168.1.42/api/devices
```

---

## 4. Toggling a Device On or Off

Toggle a specific device by its index number. Index 0 is the first device in the list, index 1 is the second, and so on.

### Python — Turn device ON

```python
import requests

HUB = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

def toggle_device(index, state):
    resp = requests.post(
        f"{HUB}/api/devices/toggle",
        headers={"X-Auth-Token": TOKEN},
        json={"index": index, "state": state}
    )
    result = resp.json()
    if result["success"]:
        print(f"Device {index} is now {'ON' if state else 'OFF'}")
    else:
        print("Error:", result["message"])

toggle_device(0, True)   # Turn device 0 ON
toggle_device(1, False)  # Turn device 1 OFF
```

### Python — Toggle by device name

```python
import requests

HUB = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

def find_device_by_name(name):
    """Returns the device index, or None if not found."""
    resp = requests.get(f"{HUB}/api/devices")
    devices = resp.json().get("devices", [])
    for d in devices:
        if d["name"].lower() == name.lower():
            return d["index"]
    return None

def toggle_by_name(name, state):
    idx = find_device_by_name(name)
    if idx is None:
        print(f"Device '{name}' not found")
        return
    resp = requests.post(
        f"{HUB}/api/devices/toggle",
        headers={"X-Auth-Token": TOKEN},
        json={"index": idx, "state": state}
    )
    result = resp.json()
    print(f"'{name}': {'ON' if state else 'OFF'}" if result["success"] else result["message"])

toggle_by_name("Bedroom Lamp", True)
toggle_by_name("Desk Fan", False)
```

### curl — Turn device 0 ON

```bash
curl -s -X POST http://192.168.1.42/api/devices/toggle \
  -H "Content-Type: application/json" \
  -H "X-Auth-Token: a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7" \
  -d '{"index": 0, "state": true}'
```

---

## 5. Turning All Devices Off

Useful for a bedtime or "leaving home" script.

### Python

```python
import requests

HUB = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

# Get current device list
devices = requests.get(f"{HUB}/api/devices").json().get("devices", [])

for device in devices:
    if device["state"]:  # only toggle devices that are currently ON
        requests.post(
            f"{HUB}/api/devices/toggle",
            headers={"X-Auth-Token": TOKEN},
            json={"index": device["index"], "state": False}
        )
        print(f"Turned OFF: {device['name']}")

print("Done — all devices are OFF")
```

### Complete login + all-off script

```python
import requests

HUB      = "http://192.168.1.42"
PASSWORD = "your-admin-password"

# Step 1: Log in
login_resp = requests.post(f"{HUB}/api/login", json={"password": PASSWORD})
login_data = login_resp.json()
if not login_data["success"]:
    print("Login failed:", login_data["message"])
    exit(1)

TOKEN = login_data["token"]
HEADERS = {"X-Auth-Token": TOKEN}

# Step 2: Get all devices
devices = requests.get(f"{HUB}/api/devices").json().get("devices", [])

# Step 3: Turn each OFF
for device in devices:
    requests.post(
        f"{HUB}/api/devices/toggle",
        headers=HEADERS,
        json={"index": device["index"], "state": False}
    )
    print(f"OFF: {device['name']}")

# Step 4: Log out
requests.post(f"{HUB}/api/logout", headers=HEADERS)
print("Logged out")
```

---

## 6. Adding and Removing a Device

### Python — Add a device

```python
import requests

HUB    = "http://192.168.1.42"
TOKEN  = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

resp = requests.post(
    f"{HUB}/api/devices/add",
    headers={"X-Auth-Token": TOKEN},
    json={
        "name": "Garden Lights",
        "pin": 25,
        "voltage": "high"   # "high" or "low"
    }
)
result = resp.json()
if result["success"]:
    print(f"Device added at index {result['index']}")
else:
    print("Failed:", result["message"])
```

### Python — Remove a device by name

```python
import requests

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

def remove_device_by_name(name):
    devices = requests.get(f"{HUB}/api/devices").json().get("devices", [])
    for d in devices:
        if d["name"].lower() == name.lower():
            resp = requests.post(
                f"{HUB}/api/devices/remove",
                headers={"X-Auth-Token": TOKEN},
                json={"index": d["index"]}
            )
            result = resp.json()
            print(f"Removed '{name}'" if result["success"] else result["message"])
            return
    print(f"Device '{name}' not found")

remove_device_by_name("Garden Lights")
```

### curl — Add a device

```bash
curl -s -X POST http://192.168.1.42/api/devices/add \
  -H "Content-Type: application/json" \
  -H "X-Auth-Token: a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7" \
  -d '{"name": "Garden Lights", "pin": 25, "voltage": "high"}'
```

---

## 7. Reading and Saving Settings

### Python — Read settings

```python
import requests

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

resp = requests.get(f"{HUB}/api/settings", headers={"X-Auth-Token": TOKEN})
s = resp.json()

print(f"Controller name:  {s['controllerName']}")
print(f"Auto logout:      {s['logoutMinutes']} minutes")
print(f"OLED brightness:  {s['oledBrightness']}")
print(f"OLED enabled:     {s['oledEnabled']}")
print(f"Firmware version: {s['firmwareVersion']}")
```

### Python — Update controller name

```python
import requests

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

# First, fetch current settings so we only change what we need
current = requests.get(f"{HUB}/api/settings", headers={"X-Auth-Token": TOKEN}).json()

resp = requests.post(
    f"{HUB}/api/settings/save",
    headers={"X-Auth-Token": TOKEN},
    json={
        "controllerName": "Workshop Hub",          # ← change this
        "logoutMinutes":  current["logoutMinutes"],
        "oledBrightness": current["oledBrightness"],
        "oledEnabled":    current["oledEnabled"]
    }
)
print(resp.json()["message"])
```

---

## 8. Backing Up the Configuration

### Python — Save backup to a file

```python
import requests
import json
from datetime import datetime

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

resp = requests.get(f"{HUB}/api/settings/backup", headers={"X-Auth-Token": TOKEN})
backup = resp.json()

filename = f"hub-backup-{datetime.now().strftime('%Y%m%d-%H%M%S')}.json"
with open(filename, "w") as f:
    json.dump(backup, f, indent=2)

print(f"Backup saved to {filename}")
```

### Python — Restore from a backup file

```python
import requests
import json

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

with open("hub-backup-20240101-120000.json") as f:
    backup = json.load(f)

resp = requests.post(
    f"{HUB}/api/settings/restore",
    headers={"X-Auth-Token": TOKEN},
    json=backup
)
print(resp.json()["message"])
# Hub will reboot after a successful restore
```

### curl — Backup to a file

```bash
curl -s http://192.168.1.42/api/settings/backup \
  -H "X-Auth-Token: a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7" \
  -o hub-backup.json
```

---

## 9. Checking Network Status

### Python

```python
import requests

HUB = "http://192.168.1.42"

resp = requests.get(f"{HUB}/api/wifi/status")
net = resp.json()

if net["connected"]:
    print(f"Network:  {net['network']}")
    print(f"IP:       {net['ip']}")
    print(f"Gateway:  {net.get('gateway', 'N/A')}")
    if "rssi" in net:
        print(f"Signal:   {net['rssi']} dBm")
else:
    print("Hub is not connected to a network")
```

### curl

```bash
curl -s http://192.168.1.42/api/wifi/status
```

---

## 10. Rebooting the Hub

```python
import requests
import time

HUB   = "http://192.168.1.42"
TOKEN = "a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"

resp = requests.post(f"{HUB}/api/reboot", headers={"X-Auth-Token": TOKEN})
print(resp.json()["message"])   # "Rebooting..."

# Wait for the hub to come back online
print("Waiting for hub to reboot", end="", flush=True)
time.sleep(5)
for _ in range(15):
    try:
        r = requests.get(f"{HUB}/api/wifi/status", timeout=2)
        if r.status_code == 200:
            print("\nHub is back online!")
            break
    except requests.exceptions.ConnectionError:
        pass
    print(".", end="", flush=True)
    time.sleep(1)
```

### curl

```bash
curl -s -X POST http://192.168.1.42/api/reboot \
  -H "X-Auth-Token: a3f2e1c9b8d7e6f5a4b3c2d1e0f9a8b7"
```

---

## 11. A Reusable Python Helper Class

Copy this class into your own scripts to avoid repeating login/auth boilerplate.

```python
import requests

class DeviceHub:
    """Minimal client for the ESP32 Device Hub API."""

    def __init__(self, host: str):
        self.host = host.rstrip("/")
        self._token = None

    def login(self, password: str) -> bool:
        resp = requests.post(f"{self.host}/api/login", json={"password": password})
        data = resp.json()
        if data.get("success"):
            self._token = data["token"]
            return True
        raise RuntimeError(f"Login failed: {data.get('message', 'unknown error')}")

    def logout(self):
        self._post("/api/logout")
        self._token = None

    def _headers(self):
        if not self._token:
            raise RuntimeError("Not logged in. Call login() first.")
        return {"X-Auth-Token": self._token, "Content-Type": "application/json"}

    def _get(self, path):
        return requests.get(f"{self.host}{path}", headers=self._headers()).json()

    def _post(self, path, body=None):
        return requests.post(f"{self.host}{path}", headers=self._headers(), json=body).json()

    # ── Devices ───────────────────────────────────────────────────────────
    def get_devices(self):
        return requests.get(f"{self.host}/api/devices").json().get("devices", [])

    def turn_on(self, index: int):
        return self._post("/api/devices/toggle", {"index": index, "state": True})

    def turn_off(self, index: int):
        return self._post("/api/devices/toggle", {"index": index, "state": False})

    def turn_on_by_name(self, name: str):
        return self._toggle_by_name(name, True)

    def turn_off_by_name(self, name: str):
        return self._toggle_by_name(name, False)

    def _toggle_by_name(self, name: str, state: bool):
        devices = self.get_devices()
        for d in devices:
            if d["name"].lower() == name.lower():
                return self._post("/api/devices/toggle", {"index": d["index"], "state": state})
        raise ValueError(f"Device '{name}' not found")

    def all_off(self):
        for d in self.get_devices():
            if d["state"]:
                self.turn_off(d["index"])

    def add_device(self, name: str, pin: int, voltage: str = "low"):
        return self._post("/api/devices/add", {"name": name, "pin": pin, "voltage": voltage})

    def remove_device(self, index: int):
        return self._post("/api/devices/remove", {"index": index})

    def rename_device(self, index: int, new_name: str):
        return self._post("/api/devices/rename", {"index": index, "name": new_name})

    # ── Settings ──────────────────────────────────────────────────────────
    def get_settings(self):
        return self._get("/api/settings")

    def backup(self):
        return self._get("/api/settings/backup")

    def reboot(self):
        return self._post("/api/reboot")

    def network_status(self):
        return requests.get(f"{self.host}/api/wifi/status").json()
```

### Usage examples

```python
hub = DeviceHub("http://192.168.1.42")
hub.login("your-admin-password")

# Print all devices
for d in hub.get_devices():
    print(d["name"], "—", "ON" if d["state"] else "OFF")

# Control by name
hub.turn_on_by_name("Bedroom Lamp")
hub.turn_off_by_name("Desk Fan")

# Turn everything off
hub.all_off()

# Read settings
s = hub.get_settings()
print("Controller:", s["controllerName"])

# Backup
import json
with open("backup.json", "w") as f:
    json.dump(hub.backup(), f, indent=2)

hub.logout()
```

---

## 12. Scheduling Tasks with cron / Task Scheduler

### Linux / macOS — cron

Turn all devices off at 11 PM every day:

1. Open crontab: `crontab -e`
2. Add a line (adjust path and IP):

```cron
0 23 * * * /usr/bin/python3 /home/user/scripts/all_off.py >> /home/user/scripts/hub.log 2>&1
```

`all_off.py`:
```python
from hub_client import DeviceHub   # put the helper class in hub_client.py
hub = DeviceHub("http://192.168.1.42")
hub.login("your-admin-password")
hub.all_off()
hub.logout()
```

### Windows — Task Scheduler

1. Open Task Scheduler → Create Basic Task
2. Set trigger: Daily at 11:00 PM
3. Action: Start a Program
4. Program: `C:\Python311\python.exe`
5. Arguments: `C:\Scripts\all_off.py`

---

## 13. curl Quick-Reference

| Action | curl command |
|---|---|
| Login | `curl -s -X POST http://HUB/api/login -H "Content-Type: application/json" -d '{"password":"PASS"}'` |
| List devices | `curl -s http://HUB/api/devices` |
| Turn device 0 ON | `curl -s -X POST http://HUB/api/devices/toggle -H "Content-Type: application/json" -H "X-Auth-Token: TOKEN" -d '{"index":0,"state":true}'` |
| Turn device 0 OFF | `curl -s -X POST http://HUB/api/devices/toggle -H "Content-Type: application/json" -H "X-Auth-Token: TOKEN" -d '{"index":0,"state":false}'` |
| Network status | `curl -s http://HUB/api/wifi/status` |
| Read settings | `curl -s http://HUB/api/settings -H "X-Auth-Token: TOKEN"` |
| Backup config | `curl -s http://HUB/api/settings/backup -H "X-Auth-Token: TOKEN" -o backup.json` |
| Reboot | `curl -s -X POST http://HUB/api/reboot -H "X-Auth-Token: TOKEN"` |
| Logout | `curl -s -X POST http://HUB/api/logout -H "X-Auth-Token: TOKEN"` |

Replace `HUB` with your device IP (e.g. `192.168.1.42`) and `TOKEN` with the token from the login response.

> **Tip:** On Windows PowerShell, use double quotes around the `-d` JSON and escape inner quotes with `\"`. On macOS/Linux bash, single quotes around the JSON body are simpler.
