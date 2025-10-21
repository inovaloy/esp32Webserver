#!/usr/bin/env python3
"""
Copy HTML files to SD card directory structure
This script prepares HTML files for SD card deployment
"""

import os
import shutil
import json

HTML_DIR = "html"
SD_OUTPUT_DIR = "sd_card_files/html"
LINKER_DATA_FILE = "linkerData.json"

def main():
    # Read linker data to find HTML files
    linker_path = os.path.join(HTML_DIR, LINKER_DATA_FILE)
    if not os.path.exists(linker_path):
        print(f"Error: {linker_path} not found")
        return

    with open(linker_path, 'r') as f:
        linker_data = json.load(f)

    # Get list of HTML files to copy
    html_files = []
    for item in linker_data:
        if linker_data[item]["rtnType"] == "HTML":
            html_files.append(linker_data[item]["fileName"])

    # Create output directory
    os.makedirs(SD_OUTPUT_DIR, exist_ok=True)

    # Copy HTML files
    copied_count = 0
    for html_file in html_files:
        src_path = os.path.join(HTML_DIR, html_file)
        dst_path = os.path.join(SD_OUTPUT_DIR, html_file)

        if os.path.exists(src_path):
            shutil.copy2(src_path, dst_path)
            file_size = os.path.getsize(dst_path)
            print(f"Copied: {html_file} ({file_size} bytes)")
            copied_count += 1
        else:
            print(f"Warning: {src_path} not found")

    print(f"\nTotal files copied: {copied_count}")
    print(f"Output directory: {SD_OUTPUT_DIR}")
    print("\nNext steps:")
    print("1. Copy the contents of 'sd_card_files/html' folder to your SD card")
    print("2. Create a folder named 'html' on the SD card root")
    print("3. Place all HTML files inside the 'html' folder on SD card")
    print("4. Insert SD card into ESP32")
    print("5. Use startWebServerWithSD() in your sketch")

if __name__ == "__main__":
    main()
