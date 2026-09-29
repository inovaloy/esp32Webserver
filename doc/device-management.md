# Device Management

This guide explains everything you can do with devices in the ESP32 Device Hub: adding, renaming, toggling, removing, and setting up the automatic ping watchdog.

---

## Table of Contents

1. [What Is a "Device"?](#1-what-is-a-device)
2. [Viewing Your Devices](#2-viewing-your-devices)
3. [Adding a Device](#3-adding-a-device)
4. [Turning a Device On or Off](#4-turning-a-device-on-or-off)
5. [Renaming a Device](#5-renaming-a-device)
6. [Removing a Device](#6-removing-a-device)
7. [Ping Watchdog — Automatic Power Cycling](#7-ping-watchdog--automatic-power-cycling)
8. [Device Limits](#8-device-limits)
9. [Backup and Restore Your Device List](#9-backup-and-restore-your-device-list)
10. [Tips and Common Mistakes](#10-tips-and-common-mistakes)

---

## 1. What Is a "Device"?

In the context of this hub, a **device** is anything physically connected to one of the hub's GPIO output pins — a relay module, an LED strip driver, a fan controller, etc.

Each device you add in the web interface is mapped to one specific GPIO pin. When you toggle the device ON in the browser, that pin goes HIGH (3.3V). When you toggle it OFF, the pin goes LOW (0V).

Devices are divided into two categories based on the pin they use:

| Category | Typical use | Controlled by |
|---|---|---|
| **High Voltage** | Relay modules switching mains loads (lamps, appliances) | Pins 25, 26, 27 (default) |
| **Low Voltage** | Signal outputs (fans, LED drivers, indicators) | Pins 5, 15, 17, 32, 33 (default) |

> The exact pin numbers depend on how your hub was configured. Only the pins your installer declared are available.

---

## 2. Viewing Your Devices

Log in and you land on the **Dashboard**. All configured devices are shown as cards, grouped by voltage category.

Each card shows:

```
┌──────────────────────────────────┐
│  Bedroom Lamp           [  ON  ] │
│  GPIO 26  ⚡ Auto         ⋮      │
└──────────────────────────────────┘
```

| Element | Description |
|---|---|
| Device name | What you called it |
| GPIO pin number | Which hardware pin it uses |
| Toggle switch | Click to turn on/off |
| **⚡ Auto** badge | Ping watchdog is active |
| **⟳ Cycling** badge | Hub is currently power-cycling this device |
| **⋮** (three-dot menu) | Opens rename / watchdog / remove options |

The dashboard **auto-refreshes every 5 seconds** — device states updated by other sessions or automation will appear automatically.

---

## 3. Adding a Device

### Before you start

Make sure there is a physical wire from the GPIO pin to the device you want to control. The software cannot create the electrical connection — that must be set up in hardware first.

### Steps

1. On the Dashboard, click **+ Add Device**

2. Fill in the **Add New Device** form:

   **Device Name** *(required)*
   A short, friendly label — max 15 characters. Examples: `Porch Light`, `Desk Fan`, `Router`.

   **Voltage** *(required)*
   - Choose **High Voltage** if this device is a relay switching a mains-voltage load
   - Choose **Low Voltage** for signal-level outputs

   **GPIO Pin** *(required)*
   Only unassigned pins for the selected voltage type are shown. If the dropdown says "No pins available", all pins in that category are already in use.

3. *(Optional)* **Enable Ping Watchdog** — if you want the hub to automatically restart this device when a network target goes offline, tick this box and fill in the [watchdog settings](#7-ping-watchdog--automatic-power-cycling).

4. Click **Add Device**

The device is added immediately and appears as a new card in the OFF state.

### What if the button says "limit reached"?

The hub has a maximum number of devices it can manage (default: 8). If you've reached the limit, you must remove an existing device before adding a new one. See [Device Limits](#8-device-limits).

---

## 4. Turning a Device On or Off

Click the **toggle switch** on a device card.

- The card background changes to the accent colour when the device is **ON**
- The card is grey/neutral when the device is **OFF**

The GPIO output changes immediately — within milliseconds of you clicking.

**The state is saved automatically.** If the hub loses power and reboots, every device returns to its last saved ON/OFF state.

### What if the toggle doesn't respond?

- Check that you are still logged in (try refreshing the page)
- Check the network status badge in the header — the hub needs to be reachable
- If the device is physically not responding, check the relay/hardware wiring

---

## 5. Renaming a Device

1. Find the device card
2. Click **⋮** → **Rename**
3. The current name is pre-filled in the text box — edit it
4. Press **Save** or hit **Enter**

The new name appears instantly on the card and on the OLED display.

**Rules:**
- Names must be 1–15 characters long
- All printable characters are allowed

---

## 6. Removing a Device

1. Find the device card
2. Click **⋮** → **✕ Remove Device**
3. A confirmation pop-up appears — click **OK** to confirm

When removed:
- The GPIO pin is set to **LOW (OFF)** first
- The device is deleted from the hub's memory
- The GPIO pin becomes available for a new device

> **Note:** After removing a device, device indices shift. If you are using the [API](api-examples.md) to control devices programmatically, re-fetch the device list after any removal to get current indices.

---

## 7. Ping Watchdog — Automatic Power Cycling

The **Ping Watchdog** is an automation feature that monitors a device's associated network endpoint. If that endpoint stops responding to a network ping (ICMP echo), the hub automatically power-cycles the device — turning it OFF for 5 seconds, then back ON.

### Typical use case

You have a Wi-Fi router connected to the hub on GPIO 26. You configure the watchdog to ping `192.168.1.1` (the router's own IP) every 30 seconds. If the router hangs and stops responding, the hub cuts power, waits 5 seconds, and restores power — a remote reboot without any manual intervention.

### When does the watchdog trigger?

- The device must be **ON** — the watchdog does nothing while the device is off
- The target IP must fail to respond to a ping **within 2 seconds**

### Setting up the Ping Watchdog

#### Option A: When adding a new device

In the **Add New Device** form, tick **Enable Ping Watchdog** before clicking Add Device:

1. Tick the **Enable Ping Watchdog** checkbox
2. Fill in **Target IP Address** — the IPv4 address to ping (e.g. `192.168.1.1`)
3. Fill in **Check Interval** — how many seconds between pings (5–255 seconds)
4. Click **Add Device**

The watchdog is saved together with the new device.

#### Option B: On an existing device

1. Find the device card on the Dashboard
2. Click **⋮** → **⚡ Ping Watchdog**
3. Toggle **Enable ping watchdog** ON
4. Enter the **Target IP Address**
5. Enter the **Check Interval** in seconds
6. Click **Save**

### Disabling the Ping Watchdog

1. Click **⋮** → **⚡ Ping Watchdog** on the device card
2. Toggle **Enable ping watchdog** OFF
3. Click **Save**

### Understanding the Cycling State

When the watchdog triggers a power cycle, the card shows:

```
┌──────────────────────────────────┐
│  Router                 [  OFF ] │
│  GPIO 26  ⟳ Cycling       ⋮      │
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│  ← animated bar
└──────────────────────────────────┘
```

- The **⟳ Cycling** badge appears
- An animated progress bar runs across the bottom of the card
- After **5 seconds** the device turns back ON and the badge disappears

During a cycle, the watchdog pauses — it does not ping again until the cycle is complete.

### Watchdog settings reference

| Setting | Allowed values | Notes |
|---|---|---|
| Target IP | Any IPv4 address | Must be reachable from the hub over Wi-Fi or Ethernet |
| Check interval | 5–255 seconds | Lower = faster response; higher = less network traffic |

---

## 8. Device Limits

| Limit | Default | Maximum |
|---|---|---|
| Maximum devices | 8 | 16 |

The limit is set at firmware build time in `webApp/deviceConfig.yaml` by the person who installed and flashed the hub. It cannot be changed through the web interface.

The **+ Add Device** button shows how many slots are used:

- Available: `+ Add Device`
- Full: `+ Add Device (limit reached)` (greyed out)

If you need more device slots, ask your installer to change `maxDevices` in `deviceConfig.yaml` and reflash the firmware.

---

## 9. Backup and Restore Your Device List

You can save a copy of all your device names, pins, and states as a JSON file, and restore from it later (useful after a factory reset or when setting up an identical second hub).

### Creating a Backup

1. Go to **Settings → Backup & Restore**
2. Click **Backup Configuration**
3. A file named `controller-config.json` is downloaded to your browser's downloads folder

The backup includes: controller name, logout timeout, OLED settings, and all device names/pins/states.

> The backup does **not** include your admin password or Wi-Fi credentials — those are not exported for security reasons.

### Restoring a Backup

1. Go to **Settings → Backup & Restore**
2. Click **Restore Configuration** (or click the restore label to open the file picker)
3. Select your `controller-config.json` file
4. Confirm the prompt — the hub will restore all settings and **reboot automatically**

After the reboot, all your devices will be back with the same names and states as when you made the backup.

> **Warning:** Restoring will overwrite the current device list entirely. Any devices added since the backup was made will be removed.

---

## 10. Tips and Common Mistakes

**Device toggles in the app but nothing happens physically**
Check the wiring. The hub sets the GPIO voltage correctly, but if the relay module or driver board is not connected or powered, nothing will switch.

**"No pins available" in the Add Device form**
All pins of that voltage type are already assigned. Either remove a device that is no longer needed to free up a pin, or ask your installer to add more pins in the hardware configuration.

**Device keeps cycling (⟳ Cycling appears repeatedly)**
The ping watchdog is triggering repeatedly because the target IP remains unreachable. This usually means:
- The device is permanently offline or the IP address has changed
- The target IP is wrong — it should be the IP of the device being power-cycled (or a device that goes down with it), not the router/gateway unless the router is what you're monitoring
- Disable the watchdog while you investigate: **⋮ → ⚡ Ping Watchdog → toggle off → Save**

**Device state shows as OFF after a reboot, but I left it ON**
State is saved to EEPROM before the reboot. If the hub rebooted unexpectedly (power cut), the last committed state is restored. If a device was toggled and the hub crashed before saving, the previous saved state is used.

**I added a device with the wrong pin**
Remove the device (⋮ → Remove Device) and add it again with the correct pin.
