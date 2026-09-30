# User Guide

Welcome to the **ESP32 Device Hub**. This guide is written for everyday users — no programming or electronics knowledge required. It walks you through everything you need to control your devices from a browser.

---

## Table of Contents

1. [What Can This Device Do?](#1-what-can-this-device-do)
2. [Opening the Web Interface](#2-opening-the-web-interface)
3. [First Boot — Setting Up Wi-Fi](#3-first-boot--setting-up-wi-fi)
4. [Logging In](#4-logging-in)
5. [Setting Your Admin Password (First Login)](#5-setting-your-admin-password-first-login)
6. [The Dashboard — Controlling Your Devices](#6-the-dashboard--controlling-your-devices)
7. [Adding a Device](#7-adding-a-device)
8. [Renaming a Device](#8-renaming-a-device)
9. [Removing a Device](#9-removing-a-device)
10. [The Settings Page](#10-the-settings-page)
11. [Changing the Admin Password](#11-changing-the-admin-password)
   - [API Tokens for Scripts and Automation](#api-tokens-for-scripts-and-automation)
12. [Logging Out](#12-logging-out)
13. [Rebooting the Device](#13-rebooting-the-device)
14. [The OLED Display](#14-the-oled-display)
15. [Frequently Asked Questions](#15-frequently-asked-questions)

---

## 1. What Can This Device Do?

The ESP32 Device Hub lets you:

- **Turn things ON and OFF** — lamps, fans, relays, or any device connected to a GPIO output — from any browser on your local network
- **Name your devices** so you always know which switch does what
- **Watch device status** on a small OLED display mounted on the hub itself
- **Connect to your home Wi-Fi** or use a wired Ethernet cable
- **Set automatic power-cycling** — if a device stops responding to a network ping, the hub restarts it automatically

You control everything through a simple web page. No app, no cloud, no subscription.

---

## 2. Opening the Web Interface

### You Need to Know the IP Address

Open a browser and type the hub's IP address into the address bar, for example:

```
http://192.168.1.42
```

**How to find the IP address:**

| Situation | Where to look |
|---|---|
| OLED display present | The IP address is shown on the first info page of the OLED |
| First boot, no Wi-Fi saved | Use `http://192.168.4.1` (the hub creates its own Wi-Fi hotspot) |
| Connected to your router | Check your router's device list for "esp32-device-hub" |

> The web interface works in any modern browser — Chrome, Firefox, Safari, Edge — on a phone, tablet, or computer.

---

## 3. First Boot — Setting Up Wi-Fi

When the hub starts for the very first time (or after a factory reset), it has no saved Wi-Fi credentials. It creates its own temporary Wi-Fi hotspot (Access Point) so you can configure it.

### Step 1 — Connect to the hub's hotspot

On your phone or laptop, open the Wi-Fi settings and look for a network named:

```
ESP32-XXXX
```

(where `XXXX` is a short code unique to your device, shown on the OLED)

The password is also shown on the OLED screen. It looks like `A1B2C3D4`.

> If you have an OLED display, you will see a QR code on the left side. You can scan it with your phone camera to connect automatically without typing the password.

### Step 2 — Open the setup page

Once connected to `ESP32-XXXX`, open your browser and go to:

```
http://192.168.4.1
```

You will land on the login page.

### Step 3 — Log in with the default password

The default admin password is the same short code shown on the OLED (e.g., `A1B2C3D4`). Type it in and press **Login**.

### Step 4 — Connect to your home Wi-Fi

After logging in:

1. Click **Settings** (bottom navigation or the Settings option in the top-right menu)
2. Select **Network** from the left sidebar
3. Click **Scan for Networks** — wait a few seconds while the hub scans
4. Click your home Wi-Fi network name in the list that appears
5. Enter your Wi-Fi password in the Password field
6. Click **Connect**

The hub will connect and show you its new IP address. **Write this address down** — you will use it to open the interface from now on.

> After connecting to Wi-Fi, you can disconnect from the `ESP32-XXXX` hotspot and rejoin your normal Wi-Fi. Then navigate to the new IP address.

---

## 4. Logging In

Go to the hub's IP address in your browser. You will see the login page.

1. Type the admin password in the **Password** field
2. Click **Login** (or press **Enter**)

If the password is correct, you are taken to the Dashboard.

**Forgot your password?** See [factory-reset.md](factory-reset.md) — a factory reset will erase the password and reset it to the device's MAC-derived default (shown on the OLED).

### Session Timeout

For security, the hub automatically logs you out after a period of inactivity (default: 15 minutes). You can change this in **Settings → Security → Auto Logout**. If you find yourself being logged out too quickly, increase the timeout.

---

## 5. Setting Your Admin Password (First Login)

The first time you log in after a factory reset (or when the device is brand new), you will see a pop-up asking you to **Set Admin Password**.

1. Type a new password — it must be **at least 6 characters** long (and no more than 32)
2. Click **Save Password**

You will not be asked again until the next factory reset. Keep this password safe — it is the only way to log in to the hub.

---

## 6. The Dashboard — Controlling Your Devices

After logging in, you land on the **Dashboard**. This is where all your connected devices appear as cards.

### Reading a Device Card

Each card shows:

```
┌──────────────────────────────┐
│  Bedroom Lamp         ●  ON  │  ← toggle switch
│  GPIO 26                     │
│  ⚡ Auto                ⋮    │  ← 3-dot menu
└──────────────────────────────┘
```

| Element | Meaning |
|---|---|
| **Name** | The label you gave the device |
| **GPIO 26** | Which output pin it's connected to |
| **Toggle switch** | Slide it to turn the device ON (coloured) or OFF (grey) |
| **⚡ Auto** badge | Ping watchdog is active for this device |
| **⟳ Cycling** badge | The hub is currently power-cycling this device |
| **⋮ menu** | Rename, configure watchdog, or remove the device |

### Turning a Device On or Off

Click the toggle switch on a device card. The card background changes colour when the device is ON. The change is applied immediately — the GPIO output changes within milliseconds.

### Device Grouping

Devices are grouped into two sections:

- **High Voltage Devices** — typically relay-switched loads (lamps, appliances)
- **Low Voltage Devices** — signal-level outputs (fans, LED strips, indicators)

The grouping is determined by which GPIO pin the device uses, as configured by the installer.

---

## 7. Adding a Device

> You can only add a device if there is a physical connection (wire, relay, etc.) between the hub's GPIO pin and the device you want to control. Adding a device in the software does not create the physical connection — your installer should have wired this up.

1. On the Dashboard, click **+ Add Device**
2. Fill in the form:
   - **Device Name** — a short, descriptive label (max 15 characters), e.g. "Desk Fan" or "Porch Light"
   - **Voltage** — choose **High Voltage** for mains-switched loads (relays) or **Low Voltage** for signal outputs
   - **GPIO Pin** — select the pin that is wired to this device; only available (unassigned) pins are shown
3. *(Optional)* Enable **Ping Watchdog** — see [device-management.md](device-management.md) for details
4. Click **Add Device**

The new device appears on the Dashboard immediately, starting in the **OFF** state.

> If the **+ Add Device** button shows "limit reached", the maximum number of devices has been configured. Contact your installer or see [device-management.md](device-management.md) for device limit information.

---

## 8. Renaming a Device

1. Find the device card on the Dashboard
2. Click the **⋮** (three-dot) button on the card
3. Click **Rename**
4. Type the new name (max 15 characters)
5. Press **Save** or hit **Enter**

The name updates instantly on the card and on the OLED display.

---

## 9. Removing a Device

1. Find the device card on the Dashboard
2. Click the **⋮** (three-dot) button on the card
3. Click **✕ Remove Device**
4. Confirm in the pop-up

> Removing a device turns the physical GPIO pin OFF before removal. The pin becomes available for a new device.

---

## 10. The Settings Page

Click **Settings** from the bottom nav bar or the avatar menu (top-right). The settings page has several sections:

| Section | What You Can Do |
|---|---|
| **General** | Change the controller name (shown in the header and on the OLED) |
| **Display** | Adjust OLED brightness (1–255) or turn the display on/off |
| **Appearance** | Switch between light/dark mode; choose an accent colour |
| **Security** | Change auto-logout timeout; change admin password; manage API tokens for scripts |
| **Network** | View current connection info; scan and connect to Wi-Fi |
| **Backup & Restore** | Download or upload your device configuration as a JSON file |
| **Firmware** | Upload a new firmware `.bin` file (OTA update) |
| **System** | Factory reset the device |

### Appearance Settings

The **Appearance** settings (theme and accent colour) are saved in your browser only — they are not sent to the hub. Each browser/device has its own appearance preference.

### Saving Settings

Most sections have their own **Save** button. After saving, a green confirmation banner appears at the top of the page.

---

## 11. Changing the Admin Password

1. Go to **Settings → Security**
2. Click **Change Admin Password**
3. Enter your **current password**
4. Enter a **new password** (at least 6 characters)
5. Click **Save**

After saving, the current session is ended and you are taken back to the login page. Log in with the new password.

> There is no "forgot password" feature — keep your password safe. If you are locked out, a [factory reset](factory-reset.md) will restore the default password.

---

### API Tokens for Scripts and Automation

The **Security** section also has a **Local API access** panel. This is for advanced users who want to control the hub from a Python script, Home Assistant, or any other tool without going through the browser login flow every time.

An API token is a long password (64 characters) that you generate once, copy, and store in your script. It works just like being logged in — but it never expires unless you revoke it.

**To generate a token:**
1. Go to **Settings → Security**
2. Scroll down to **Local API access**
3. Type a name for the token (e.g., `Home Assistant` or `Night script`) — max 24 characters
4. Click **Generate API Token**
5. A box appears with the token value — **copy it now**. It will never be shown again.

**To revoke a token:**
Each saved token appears in a list with its name and a **Revoke** button. Clicking Revoke deletes it permanently.

You can have up to **5 tokens** active at the same time.

> For usage examples see [api-examples.md](api-examples.md).

---

## 12. Logging Out

Open the **Admin** dropdown in the top-right corner (the "A" button) and click **Logout**. You are returned to the login page and the session token is cleared.

The hub also logs you out automatically after the configured inactivity period.

---

## 13. Rebooting the Device

To restart the hub:

1. Open the **Admin** dropdown (top-right "A" button)
2. Click **Reboot Device**
3. Confirm in the pop-up by clicking **Reboot**

The page shows a countdown while the hub restarts (~8 seconds), then redirects to the login page.

A reboot does **not** erase any settings or device configuration. All devices retain their saved ON/OFF states.

---

## 14. The OLED Display

If your hub has a small OLED screen, it shows live status information. The display cycles through pages automatically every 4 seconds, or you can press the page button to advance manually.

### Normal Mode Pages

| Page | What It Shows |
|---|---|
| **Info** | Controller name · Ethernet IP · Wi-Fi IP · Firmware version |
| **Summary** | Total devices · High-voltage count · Low-voltage count · ON/OFF count |
| **Device list** | Device names and their current ON/OFF states (4 per page) |

### AP Mode (no Wi-Fi connected)

When the hub is in hotspot mode, the OLED shows:

- **QR code** (default) — scan to connect to the hub's hotspot automatically
- **Info page** — shows the IP address and the default admin password (press the page button to toggle between QR and info)

### Buttons on the Hub

| Button | Location | Action |
|---|---|---|
| **Page button** | GPIO 4 | Short press: advance to next OLED page |
| **Reset button** | GPIO 16 | Hold 10 seconds: factory reset (erases everything) |

---

## 15. Frequently Asked Questions

**Q: I can't find the hub on my network after it connected to Wi-Fi.**
Check your router's connected devices list for a device named `esp32-device-hub`. Alternatively, check the OLED display — the IP address is shown on the first page.

**Q: The hub shows "AP Mode" on the OLED even though I already set up Wi-Fi.**
The hub could not connect to the saved Wi-Fi. This can happen if the Wi-Fi password changed or the router is unreachable. Connect to the hub's hotspot and go to **Settings → Network** to reconfigure Wi-Fi.

**Q: I forgot my admin password.**
Perform a factory reset. The password will be reset to the MAC-derived default shown on the OLED. See [factory-reset.md](factory-reset.md).

**Q: A device shows "⟳ Cycling" on the card — what does that mean?**
The ping watchdog detected that the monitored IP address became unreachable, so the hub is power-cycling the device (turning it off for 5 seconds, then back on). This is normal automatic behaviour. If it keeps happening, the monitored device may be permanently offline or the IP address is wrong — check the watchdog settings via the ⋮ menu → **Ping Watchdog**.

**Q: How do I back up my device configuration?**
Go to **Settings → Backup & Restore → Backup Configuration**. A `controller-config.json` file is downloaded to your device. Keep it somewhere safe.

**Q: Can multiple people control the hub at once?**
Only one user can be logged in at a time. If another person logs in, the previous session is effectively overwritten. Device toggling by one session will be reflected for anyone viewing the dashboard (the status auto-refreshes every 5 seconds).

**Q: Does the hub need internet access?**
No. The hub runs entirely on your local network. There is no cloud component and no outgoing internet traffic.

**Q: What happens to the devices if the hub loses power?**
The device ON/OFF states are saved in EEPROM. When the hub powers back on, it restores all GPIO outputs to their last saved state and reconnects to Wi-Fi automatically.
