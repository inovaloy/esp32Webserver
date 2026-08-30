#!/usr/bin/env python3
"""
Device Config Compiler for ESP32 Webserver
Reads webApp/deviceConfig.yaml and generates src/autoGen/autoGenDeviceConfig.h
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
    storageBackend = str(data.get("storageBackend", "internal")).lower()
    if storageBackend not in ("internal", "external"):
        raise ValueError("storageBackend must be 'internal' or 'external'")
    highVoltagePins = data.get("highVoltageGpioPins", [])
    lowVoltagePins  = data.get("lowVoltageGpioPins", [])

    # Clamp: can never exceed firmware hard limit of 16
    if maxDevices > 16:
        print(f"Warning: maxDevices {maxDevices} > 16, clamping to 16")
        maxDevices = 16

    highVoltagePins = [int(p) for p in highVoltagePins]
    lowVoltagePins  = [int(p) for p in lowVoltagePins]
    pins = highVoltagePins + lowVoltagePins
    if len(pins) > maxDevices:
        print(f"Warning: configured GPIO pins exceed maxDevices {maxDevices}; truncating")
        pins = pins[:maxDevices]
        highVoltagePins = pins[:len(highVoltagePins)]
        lowVoltagePins = pins[len(highVoltagePins):]
    highVoltagePinCount = len(highVoltagePins)
    lowVoltagePinCount = len(lowVoltagePins)
    pinCount = len(pins)

    print(f"maxDevices : {maxDevices}")
    print(f"high voltage GPIOs: {highVoltagePins} ({highVoltagePinCount} entries)")
    print(f"low voltage GPIOs : {lowVoltagePins} ({lowVoltagePinCount} entries)")

    os.makedirs(BUILD_DIR, exist_ok=True)

    header_path = os.path.join(BUILD_DIR, "autoGenDeviceConfig.h")
    with io.open(header_path, "w", newline='\n') as h:
        h.write(f"""/*
 * This is an autogen file; Do not change manually.
 * Generated from webApp/deviceConfig.yaml
 */

#ifndef AUTOGEN_DEVICE_CONFIG_H
#define AUTOGEN_DEVICE_CONFIG_H

#include <stdint.h>

// Maximum number of devices the user is allowed to add
#define CFG_MAX_DEVICES {maxDevices}

#define CFG_STORAGE_EXTERNAL {1 if storageBackend == 'external' else 0}
#define CFG_EXTERNAL_EEPROM_ADDRESS 0x50
#define CFG_EXTERNAL_EEPROM_SIZE 8192

// GPIO pins grouped by voltage category
#define CFG_HIGH_VOLTAGE_PIN_COUNT {highVoltagePinCount}
static const uint8_t CFG_HIGH_VOLTAGE_PINS[CFG_HIGH_VOLTAGE_PIN_COUNT] = {{
    {', '.join(str(p) for p in highVoltagePins)}
}};

#define CFG_LOW_VOLTAGE_PIN_COUNT {lowVoltagePinCount}
static const uint8_t CFG_LOW_VOLTAGE_PINS[CFG_LOW_VOLTAGE_PIN_COUNT] = {{
    {', '.join(str(p) for p in lowVoltagePins)}
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
