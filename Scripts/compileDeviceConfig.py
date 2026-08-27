#!/usr/bin/env python3
"""
Device Config Compiler for ESP32 Webserver
Reads WebApp/deviceConfig.yaml and generates Src/AutoGen/autoGenDeviceConfig.h
"""

import os
import io
import shutil
import yaml

from common import *


DEVICE_CONFIG_FILE = os.path.join(WEB_APP_DIR, "deviceConfig.yaml")


def compileDeviceConfig():
    with open(DEVICE_CONFIG_FILE, 'r') as f:
        data = yaml.safe_load(f)

    maxDevices = int(data.get("maxDevices", 16))
    pins       = data.get("gpioPins", [])

    # Clamp: can never exceed firmware hard limit of 16
    if maxDevices > 16:
        print(f"Warning: maxDevices {maxDevices} > 16, clamping to 16")
        maxDevices = 16

    # Only take the first maxDevices pins from the list
    pins = [int(p) for p in pins[:maxDevices]]
    pinCount = len(pins)

    print(f"maxDevices : {maxDevices}")
    print(f"gpioPins   : {pins} ({pinCount} entries)")

    os.makedirs(BUILD_DIR, exist_ok=True)

    header_path = os.path.join(BUILD_DIR, "autoGenDeviceConfig.h")
    with io.open(header_path, "w", newline='\n') as h:
        h.write(f"""/*
 * This is an autogen file; Do not change manually.
 * Generated from WebApp/deviceConfig.yaml
 */

#ifndef AUTOGEN_DEVICE_CONFIG_H
#define AUTOGEN_DEVICE_CONFIG_H

#include <stdint.h>

// Maximum number of devices the user is allowed to add
#define CFG_MAX_DEVICES {maxDevices}

// Allowed GPIO pins (in order) — length CFG_PIN_COUNT
#define CFG_PIN_COUNT {pinCount}
static const uint8_t CFG_ALLOWED_PINS[CFG_PIN_COUNT] = {{
    {', '.join(str(p) for p in pins)}
}};

#endif // AUTOGEN_DEVICE_CONFIG_H
""")

    # Copy to AutoGen destination
    os.makedirs(AUTOGEN_DEST_DIR, exist_ok=True)
    shutil.copy(header_path, os.path.join(AUTOGEN_DEST_DIR, "autoGenDeviceConfig.h"))
    print(f"Generated: {os.path.join(AUTOGEN_DEST_DIR, 'autoGenDeviceConfig.h')}")


def main():
    print("Compiling device config...")
    print("-" * 60)
    compileDeviceConfig()
    print("-" * 60)
    print("Device config compilation complete!")


if __name__ == '__main__':
    main()
