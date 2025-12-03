#!/usr/bin/env python3
"""
Asset Compilation Script for ESP32 Webserver
Compiles CSS, JavaScript, and Image files to compressed char arrays
"""

import gzip
import os
import io
import shutil
import json
import re
import argparse

from common import *
from utility import *


InfoData = {}
LinkerData = []


def readLinkerDataFile():
    """Read linker data file to get asset mappings"""
    global LinkerData

    if os.path.exists(LINKER_DATA_FILE):
        linkerData = readLinkerData(LINKER_DATA_FILE)
        print(f"Loading asset mappings from: {LINKER_DATA_FILE}")

        for item, config in linkerData.items():
            returnType = config.get("rtnType", "")
            if returnType == "ASSET":
                fileName = config["fileName"]
                LinkerData.append({
                    'route': item,
                    'fileName': fileName,
                    'contentType': config.get("contentType", ""),
                    'macro': config.get("macro", ""),
                    'isCached': config.get("isCached", True)
                })

        print(f"Found {len(LinkerData)} assets to process")
    else:
        print(f"Error: Linker data file not found: {LINKER_DATA_FILE}")
        raise FileNotFoundError(f"Required file not found: {LINKER_DATA_FILE}")


def createHeaderFile():
    """Create the header file for assets"""
    header_path = os.path.join(BUILD_DIR, AUTOGEN_ASSETS_H)
    with io.open(header_path, "w", newline='\n') as h:
        h.write("""/*
* This is an autogen file; Do not change manually
* Contains compressed CSS, JavaScript, and Image assets for ESP32 Webserver
*/

#ifndef AUTOGEN_ASSETS_H
#define AUTOGEN_ASSETS_H

#include <stdint.h>

""")


def updateHeaderFile(originalName, compressedName, compressedData, dataLength, arrStrName, arrStrLength, contentType):
    """Add asset data to header file"""
    header_path = os.path.join(BUILD_DIR, AUTOGEN_ASSETS_H)
    with io.open(header_path, "a", newline='\n') as h:
        h.write(f"""
// File: {originalName}, Compressed: {compressedName}, Size: {dataLength}, Type: {contentType}
#define {arrStrLength} {dataLength}
#define {arrStrName.upper()}_CONTENT_TYPE "{contentType}"

const uint8_t {arrStrName}[] = {{
""")

        # Write hex data
        index = 0
        hexData = "    "
        for byte in compressedData:
            hexData += str2hex(byte) + ", "
            index += 1
            if index == 16:
                hexData = hexData[:-1] + "\n    "
                index = 0

        if index == 0:
            hexData = hexData[:-6]
        else:
            hexData = hexData[:-2]

        h.write(hexData)
        h.write("""
};
""")


def closeHeaderFile():
    """Close the header file"""
    header_path = os.path.join(BUILD_DIR, AUTOGEN_ASSETS_H)
    with io.open(header_path, "a", newline='\n') as h:
        h.write("""
#endif // AUTOGEN_ASSETS_H
""")


def processTextAsset(filePath, enableCompression=True):
    """Process CSS and JS files"""
    with open(filePath, 'r', encoding='utf-8') as f:
        content = f.read()

    originalSize = len(content.encode('utf-8'))

    if enableCompression:
        ext = os.path.splitext(filePath)[1].lower()
        if ext == '.css':
            content = minifyCss(content)
        elif ext == '.js':
            content = minifyJavaScript(content)

    minifiedSize = len(content.encode('utf-8'))
    return content, originalSize, minifiedSize


def processBinaryAsset(filePath):
    """Process binary files (images, fonts)"""
    with open(filePath, 'rb') as f:
        content = f.read()
    return content, len(content), len(content)


def compressAssets(enableCompression=True):
    """Main function to compress all assets"""
    global InfoData

    os.makedirs(BUILD_DIR, exist_ok=True)

    createHeaderFile()

    # Process assets referenced in linker data
    for asset in LinkerData:
        assetPath = os.path.join(ASSETS_DIR, asset['fileName'])
        if os.path.exists(assetPath):
            processAsset(assetPath, asset['fileName'], asset, enableCompression)
        else:
            print(f"Warning: Asset file not found: {assetPath}")

    closeHeaderFile()

    # Copy to destination
    os.makedirs(AUTOGEN_DEST_DIR, exist_ok=True)
    shutil.copy(
        os.path.join(BUILD_DIR, AUTOGEN_ASSETS_H),
        os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_ASSETS_H)
    )

    # Save info data
    with open(os.path.join(BUILD_DIR, AUTOGEN_ASSET_INFO_FILE), 'w') as f:
        f.write(json.dumps(InfoData, indent=4))


def processAsset(filePath, fileName, assetInfo, enableCompression=True):
    """Process individual asset file"""
    global InfoData

    ext = os.path.splitext(fileName)[1].lower()
    contentType = SUPPORTED_ASSET_EXTENSIONS.get(ext, 'application/octet-stream')

    print(f"Processing: {fileName}")

    os.makedirs(os.path.dirname(os.path.join(BUILD_DIR, fileName)), exist_ok=True)

    # Determine if it's a text or binary file
    if ext in TEXT_ASSET_EXTENSIONS:
        content, originalSize, processedSize = processTextAsset(filePath, enableCompression)
        with open(os.path.join(BUILD_DIR, fileName), 'w', encoding='utf-8') as tempFile:
            tempFile.write(content)
        binaryData = content.encode('utf-8')
    else:
        binaryData, originalSize, processedSize = processBinaryAsset(filePath)
        with open(os.path.join(BUILD_DIR, fileName), 'wb') as tempFile:
            tempFile.write(binaryData)

    # Compress with gzip
    compressedData = gzip.compress(binaryData)
    compressedSize = len(compressedData)

    # Generate variable names
    arrStrName = convertToCamelCase(fileName)
    arrStrLength = arrStrName + "Len"
    compressedName = fileName + ".gz"

    # Store info
    InfoData[fileName] = {
        'originalSize': originalSize,
        'processedSize': processedSize,
        'compressedSize': compressedSize,
        'contentType': contentType,
        'arrStrName': arrStrName,
        'arrStrLength': arrStrLength,
        'compressionEnabled': enableCompression,
        'route': assetInfo['route'] if assetInfo else f"/assets/{fileName}",
        'macro': assetInfo['macro'] if assetInfo else arrStrName.upper(),
        'cache': assetInfo["isCached"]
    }

    # Add to header file
    updateHeaderFile(fileName, compressedName, compressedData, compressedSize,
                    arrStrName, arrStrLength, contentType)

    # Print statistics
    compressionRatio = ((originalSize - compressedSize) / originalSize) * 100 if originalSize > 0 else 0
    print(f"  Original: {originalSize:>6} bytes")
    if enableCompression and ext in ['.css', '.js']:
        print(f"  Minified: {processedSize:>6} bytes")
    print(f"  GZipped:  {compressedSize:>6} bytes (saved {compressionRatio:>5.1f}%)")


def main():
    parser = argparse.ArgumentParser(description='Compile assets to compressed header files')
    parser.add_argument('-nc', '--no-compress', action='store_true',
                       help='Disable CSS/JavaScript minification')

    args = parser.parse_args()
    enableCompression = not args.no_compress

    if enableCompression:
        print("Asset compression: ENABLED")
    else:
        print("Asset compression: DISABLED")
    print("-" * 60)

    readLinkerDataFile()
    compressAssets(enableCompression)

    print("-" * 60)
    print("Asset compilation completed!")
    print(f"Generated files in: {AUTOGEN_DEST_DIR}")

if __name__ == '__main__':
    main()
