# Factory Reset

A factory reset erases all saved settings on the ESP32 Device Hub and restores it to a clean out-of-the-box state. This page explains when to use it, how to perform it (two methods), and exactly what gets erased.

---

## Table of Contents

1. [When Should You Factory Reset?](#1-when-should-you-factory-reset)
2. [What Gets Erased](#2-what-gets-erased)
3. [What Is NOT Erased](#3-what-is-not-erased)
4. [Method 1 — Factory Reset via the Web Interface](#4-method-1--factory-reset-via-the-web-interface)
5. [Method 2 — Factory Reset via the Physical Button](#5-method-2--factory-reset-via-the-physical-button)
6. [What Happens After a Factory Reset](#6-what-happens-after-a-factory-reset)
7. [Before You Reset — Backup Your Configuration](#7-before-you-reset--backup-your-configuration)
8. [Recovering After a Reset](#8-recovering-after-a-reset)

---

## 1. When Should You Factory Reset?

| Situation | Recommended action |
|---|---|
| Forgotten admin password and locked out | Factory reset (physical button) |
| Giving the hub to someone else | Factory reset to remove your Wi-Fi credentials and password |
| Hub is behaving unexpectedly and rebooting doesn't help | Factory reset as a last resort |
| Moving to a new home / new Wi-Fi network | Just re-configure Wi-Fi in Settings → Network (no reset needed) |
| Want to start device list from scratch | Remove devices individually, or use backup/restore instead |

> A factory reset is **irreversible**. There is no undo. If you have important configuration, [back it up first](#7-before-you-reset--backup-your-configuration).

---

## 2. What Gets Erased

After a factory reset, all of the following are permanently removed:

| Data | Erased? |
|---|---|
| Admin password | ✓ Reset to MAC-derived default |
| Wi-Fi SSID and password | ✓ Erased |
| All configured devices (names, pins, states) | ✓ Erased |
| Controller name | ✓ Reset to "Home Controller" |
| Session timeout setting | ✓ Reset to 15 minutes |
| OLED brightness and enabled state | ✓ Reset to defaults |

The entire EEPROM storage area (534 bytes) is zeroed out, then default values are written.

---

## 3. What Is NOT Erased

| Data | Preserved? |
|---|---|
| Firmware code | ✓ Unchanged |
| GPIO pin configuration (what pins are available) | ✓ Unchanged (baked into firmware) |
| OLED display hardware | ✓ Unchanged |
| Wi-Fi radio / Ethernet hardware | ✓ Unchanged |
| Appearance preferences (theme, accent colour) | ✓ Stored in browser `localStorage` — not on the hub |

---

## 4. Method 1 — Factory Reset via the Web Interface

This method requires you to be logged in. Use it when you want to reset the hub intentionally and still have access.

### Steps

1. Log in to the web interface
2. Go to **Settings** (bottom nav or the Admin menu)
3. Click **System** in the settings sidebar
4. Click **Factory Reset**
5. A confirmation dialog appears asking for your **admin password**

   ```
   ┌──────────────────────────────────┐
   │  Confirm Factory Reset           │
   │                                  │
   │  Enter the admin password to     │
   │  erase all settings and devices. │
   │                                  │
   │  [__________________]            │
   │  Admin Password                  │
   │                                  │
   │  [ Reset ]  [ Cancel ]           │
   └──────────────────────────────────┘
   ```

6. Type your admin password and click **Reset**
7. The hub erases all settings and **reboots automatically**
8. Your browser is redirected to the login page after a short countdown

> **Why is the password required?** This prevents accidental or malicious resets — someone who is merely logged in cannot reset the hub without knowing the password.

---

## 5. Method 2 — Factory Reset via the Physical Button

Use this method if you are **locked out** (forgot the password) or cannot access the web interface for any reason.

### Locating the Button

The factory reset button is on **GPIO 16** of the ESP32. On most hub enclosures, it is a small momentary push-button labelled "RESET" or "RST" (distinct from the ESP32's main reset button, which reboots without erasing).

If you are not sure which button is which, check the hardware documentation or ask your installer.

### How to Perform the Reset

1. Locate the factory reset button on the hub enclosure
2. Press and **hold** the button
3. Keep holding — the hub needs you to hold for a full **10 seconds**
4. The serial monitor (if connected) will print:

   ```
   Factory reset button pressed; hold for 10 seconds.
   Factory reset button held for 10 seconds; resetting.
   ```

5. Release the button — the hub erases all settings and reboots immediately

> **Why 10 seconds?** The long hold time is intentional — it prevents accidental resets from brief button presses (e.g., bumping the enclosure).

### What to Expect During the Hold

- The hub continues to operate normally while you hold the button
- There is no beep or visual countdown (if you have serial access, messages appear at the start and end of the hold)
- The OLED display does not change during the hold

---

## 6. What Happens After a Factory Reset

After the hub reboots following a factory reset:

### 1. Network — AP Mode

Because Wi-Fi credentials were erased, the hub cannot connect to your network. It starts in **Access Point (AP) mode**:

- Creates a hotspot named `ESP32-XXXX` (XXXX = unique device code)
- Password: MAC-derived 8-character hex code (shown on OLED)
- IP address: `192.168.4.1`

The OLED shows the QR code page.

### 2. Admin Password — Back to Default

The admin password is reset to the same 8-character MAC-derived hex code as the AP password. It is shown on the OLED info page (press the page button to see it).

Example: if the OLED shows `A1B2C3D4`, both the AP password and the admin password are `A1B2C3D4`.

### 3. Devices — All Cleared

The device list is empty. All GPIO pins that were previously configured as outputs are now unconfigured.

### 4. Settings — Defaults Restored

| Setting | After Reset |
|---|---|
| Controller name | `Home Controller` |
| Auto logout | 15 minutes |
| OLED brightness | 100 |
| OLED enabled | On |

### 5. Re-setup Required

You will need to:
1. Connect to the `ESP32-XXXX` hotspot
2. Log in with the default password shown on the OLED
3. Set a new admin password when prompted
4. Go to **Settings → Network** and reconnect to your Wi-Fi
5. Re-add your devices on the Dashboard

If you made a backup before the reset, you can restore it at step 4 via **Settings → Backup & Restore → Restore Configuration** (after connecting to Wi-Fi).

---

## 7. Before You Reset — Backup Your Configuration

If you have access to the web interface, always save a backup before resetting:

1. Go to **Settings → Backup & Restore**
2. Click **Backup Configuration**
3. Save the downloaded `controller-config.json` file somewhere safe

The backup captures your device list, controller name, and display settings. Wi-Fi credentials and the admin password are not included (by design).

After the reset and re-setup, you can restore the backup to instantly re-add all your devices without typing them in one by one.

---

## 8. Recovering After a Reset

### Step-by-step Recovery

```
1. Connect to ESP32-XXXX hotspot
        ↓
2. Open http://192.168.4.1
        ↓
3. Log in with default password (shown on OLED)
        ↓
4. Set new admin password when prompted
        ↓
5. Go to Settings → Network → scan → connect to home Wi-Fi
        ↓
6. Hub reboots / connects — note the new IP from OLED
        ↓
7. Open http://<new-ip> and log in
        ↓
8. (Optional) Settings → Backup & Restore → Restore Configuration
        ↓
9. Re-add devices if no backup available
```

### If You Don't Have a Backup

You will need to re-add each device manually on the Dashboard. You will need to know:
- Which GPIO pin each physical device is connected to (check your wiring or ask your installer)
- The device name you want to use

The [device-management.md](device-management.md) guide walks through adding devices step by step.
