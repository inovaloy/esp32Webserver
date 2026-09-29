# Network Setup

This guide explains how to connect the ESP32 Device Hub to your network: via Wi-Fi, wired Ethernet, or the built-in Access Point (AP) hotspot that appears when nothing else is available.

---

## Table of Contents

1. [How the Hub Connects to a Network](#1-how-the-hub-connects-to-a-network)
2. [Access Point (AP) Mode — First-time Setup](#2-access-point-ap-mode--first-time-setup)
3. [Connecting to Wi-Fi](#3-connecting-to-wi-fi)
4. [Using Wired Ethernet (W5500)](#4-using-wired-ethernet-w5500)
5. [Viewing Current Network Status](#5-viewing-current-network-status)
6. [Changing Your Wi-Fi Network](#6-changing-your-wi-fi-network)
7. [Ethernet Direct Connection (No Router)](#7-ethernet-direct-connection-no-router)
8. [Network Priority and Automatic Failover](#8-network-priority-and-automatic-failover)
9. [Finding the Hub's IP Address](#9-finding-the-hubs-ip-address)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. How the Hub Connects to a Network

Every time the hub boots, it tries to connect in this order:

```
Boot
 │
 ├─ 1. Try Ethernet (W5500 module)
 │       ✓ Cable connected + DHCP works  → use Ethernet IP
 │       ✓ Cable connected, no DHCP      → use static 192.168.4.1
 │       ✗ No cable / not detected       → skip
 │
 ├─ 2. Try saved Wi-Fi credentials
 │       ✓ Connects within 20 s          → use Wi-Fi IP
 │       ✗ No saved SSID / timeout       → skip
 │
 └─ 3. Start Access Point (AP) mode
         Always succeeds — creates a hotspot named ESP32-XXXX
```

If Ethernet succeeds, it is preferred over Wi-Fi. Both can be active at the same time — Ethernet is used as the primary network, and Wi-Fi serves as automatic failover if the Ethernet cable is unplugged.

---

## 2. Access Point (AP) Mode — First-time Setup

AP mode is the hub's fallback. It creates its own Wi-Fi hotspot so you can reach the web interface even without a router.

### Identifying AP Mode

On the **OLED display**, you will see a QR code on the left side and the network credentials on the right.

In your phone or laptop's Wi-Fi settings, a network named `ESP32-XXXX` will appear (where XXXX is a 4-character code unique to your device).

### Connecting to the Hub's Hotspot

**Option A — Scan the QR code**
On a phone, open the camera app and point it at the OLED QR code. Your phone should offer to connect automatically.

**Option B — Connect manually**
1. Open Wi-Fi settings on your phone or laptop
2. Select the network `ESP32-XXXX`
3. Enter the password shown on the OLED (e.g., `A1B2C3D4`)

### Opening the Web Interface in AP Mode

Once connected to the hotspot, open your browser and go to:

```
http://192.168.4.1
```

This is the hub's fixed IP address when in AP mode.

> **Note:** While connected to the hub's hotspot, you will not have internet access on that device. This is normal — the hotspot is only for configuring the hub.

### Switching OLED Pages in AP Mode

The OLED has two AP mode pages. Press the **page button** on the hub to toggle between:
- **QR code page** — for easy mobile connection
- **Info page** — shows the IP address `192.168.4.1` and the default admin password in text

---

## 3. Connecting to Wi-Fi

### From the Settings Page (recommended)

1. Log in to the web interface (see [user-guide.md](user-guide.md) for login help)
2. Go to **Settings** → **Network**
3. The current network status is shown at the top

4. Click **Scan for Networks**
   - The hub scans for nearby Wi-Fi networks (takes 2–5 seconds)
   - A list of found networks appears, showing network name and signal strength

5. Click your home network in the list — its name fills the SSID field automatically

6. Enter your Wi-Fi password in the **Password** field
   - Click the eye icon to show/hide the password
   - Leave the password field empty for open (unprotected) networks

7. Click **Connect**
   - The hub attempts to connect (up to 10 seconds)
   - A success or failure message appears

8. If successful, the hub shows the assigned IP address (e.g., `192.168.1.42`)

### After Connecting

- The hub saves the SSID and password to EEPROM
- On every subsequent reboot it connects to this Wi-Fi automatically
- You can now disconnect from the `ESP32-XXXX` hotspot and use your normal Wi-Fi
- Navigate to the new IP address to access the hub

### Signal Strength Guide

When scanning, networks are shown with their signal strength in dBm:

| dBm range | Quality | Notes |
|---|---|---|
| Above −50 | Excellent | Very close to router |
| −50 to −65 | Good | Reliable connection |
| −65 to −75 | Fair | May have occasional drops |
| Below −75 | Poor | Consider moving hub closer to router |

---

## 4. Using Wired Ethernet (W5500)

If your hub has a W5500 Ethernet module installed and a network cable plugged in, Ethernet is used automatically — no configuration required.

### What Happens at Boot

1. The hub detects the W5500 module and activates the Ethernet driver
2. It checks for a physical link (cable connected) — waits up to 5 seconds
3. It requests an IP address from your router via DHCP — waits up to 5 seconds
4. If DHCP succeeds, the Ethernet IP is shown on the OLED info page
5. If DHCP times out but the cable is connected, it falls back to `192.168.4.1` (see [below](#7-ethernet-direct-connection-no-router))

### Ethernet is Always Preferred

When both Ethernet and Wi-Fi are available, Ethernet takes priority. The OLED shows the Ethernet IP. Wi-Fi remains connected in the background for failover.

### Hot-Plugging

You can plug and unplug the Ethernet cable while the hub is running:

- **Plug in** → hub detects the link, attempts DHCP, promotes Ethernet to primary
- **Unplug** → hub detects link loss within 1 second, switches to saved Wi-Fi (or AP mode if Wi-Fi is unavailable)

---

## 5. Viewing Current Network Status

### From the Header

The Wi-Fi/network badge in the header shows the active connection at a glance:

| Badge text | Meaning |
|---|---|
| `Ethernet: 192.168.1.42` | Connected via Ethernet DHCP |
| `WiFi: 192.168.1.43` | Connected via Wi-Fi |
| `Ethernet: 192.168.1.42 · WiFi: 192.168.1.43` | Both active |
| `Not connected` | Neither Ethernet nor Wi-Fi has an IP |

### From Settings → Network

The **Network** settings section shows a full status card:

```
✓ Ethernet connected         Active connection
  SSID        Ethernet
  IP address  192.168.1.42
  MAC address AA:BB:CC:DD:EE:FF
  Gateway     192.168.1.1
  Subnet      255.255.255.0
```

For Wi-Fi it also shows signal strength (RSSI in dBm).

### From the OLED

The OLED **Info page** (page 0) shows:
- `ETH: 192.168.1.42` (or `ETH: unavailable`)
- `WiFi: 192.168.1.43` (or `WiFi: unavailable`)

---

## 6. Changing Your Wi-Fi Network

To connect to a different Wi-Fi network:

1. Go to **Settings → Network**
2. Click **Scan for Networks**
3. Click the new network in the list
4. Enter the new password
5. Click **Connect**

If the connection succeeds, the new SSID and password overwrite the previously saved ones. The next reboot will connect to the new network automatically.

> You do not need to reboot the hub after changing Wi-Fi — the new connection becomes active immediately.

---

## 7. Ethernet Direct Connection (No Router)

If you plug an Ethernet cable directly from the hub to a laptop (no router in between), there is no DHCP server to assign an IP automatically. The hub handles this gracefully:

After DHCP times out, the hub assigns itself:

```
IP:      192.168.4.1
Subnet:  255.255.255.0
```

**To connect from your laptop:**

1. Open your laptop's network settings
2. Find the Ethernet adapter and assign it a **static IP** in the same subnet:
   - IP Address: `192.168.4.2`
   - Subnet Mask: `255.255.255.0`
   - Gateway: *(leave blank)*
3. Open your browser and go to `http://192.168.4.1`

> The same `192.168.4.1` address is used for both the direct Ethernet fallback and AP mode. If Ethernet is connected, it takes priority.

---

## 8. Network Priority and Automatic Failover

The hub manages multiple network interfaces automatically with no user action required:

| Priority | Interface | When Used |
|---|---|---|
| 1 (highest) | Ethernet (W5500) | Cable connected, DHCP or static fallback |
| 2 | Wi-Fi Station | Saved SSID found and connected |
| 3 (lowest) | Access Point | No Ethernet, no saved Wi-Fi / connection fails |

### Failover Scenarios

**Ethernet cable unplugged → Wi-Fi takeover**
The hub detects the link loss within 1 second and switches to Wi-Fi. If Wi-Fi is already connected (it stays connected in the background), the takeover is instant with no reboot.

**Wi-Fi drops → reconnect attempt**
Every 30 seconds, if Wi-Fi is not connected, the hub retries with the saved credentials.

**Both drop → AP mode**
If both Ethernet and Wi-Fi are unavailable, the hub starts AP mode so you can always reach it.

**Ethernet reappears while on Wi-Fi → Ethernet takeover**
If you plug in an Ethernet cable and DHCP succeeds, Ethernet is promoted to primary. Wi-Fi stays connected but is no longer the preferred interface.

---

## 9. Finding the Hub's IP Address

| Method | How |
|---|---|
| **OLED display** | Look at Page 0 — shows ETH and WiFi IPs |
| **Router device list** | Look for `esp32-device-hub` in your router's connected devices |
| **Serial monitor** | Connect USB and open serial at 115200 baud — the IP is printed on boot |
| **AP mode** | If no other IP is available, the hub is at `192.168.4.1` |
| **mDNS / Bonjour** | The hub advertises as `esp32-device-hub.local` (supported on most systems without extra software) |

---

## 10. Troubleshooting

### Hub stuck in AP mode even though Wi-Fi credentials were saved

- The password may have changed on your router — go to **Settings → Network** and reconnect
- Your router may be out of range — check signal strength after scanning
- Check that the router is not blocking the device (MAC filter, client limit)

### Hub connects to Wi-Fi but the IP keeps changing

Your router is assigning a new IP each time (dynamic DHCP). To prevent this, set a **DHCP reservation** on your router using the hub's MAC address (shown in **Settings → Network** under "MAC address").

### Ethernet detected but DHCP keeps timing out

- Check that your router/switch has DHCP enabled
- Try a different Ethernet cable
- If connecting directly to a laptop, configure a static IP on your laptop as described in [Section 7](#7-ethernet-direct-connection-no-router)

### "Not connected" in the header even though I can reach the web interface

You are accessing the hub in AP mode (`192.168.4.1`). The "Not connected" status means the hub itself is not connected to the internet or your home LAN — which is expected in AP mode. Go to **Settings → Network** to connect to your home Wi-Fi.

### Wi-Fi connects but then disconnects repeatedly

- Weak signal — move the hub closer to the router or use Ethernet
- IP address conflict — set a DHCP reservation on the router for this device
- The router may be kicking off devices after idle time — check the router's wireless settings
