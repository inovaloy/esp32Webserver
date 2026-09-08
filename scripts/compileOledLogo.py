#!/usr/bin/env python3
"""
OLED Logo Compilation Script
Converts blank_white_logo.png to a 1-bit C bitmap header for the SSD1306 OLED.

Output: Src/AutoGen/autoGenOledLogo.h
  - bootLogo[]    : PROGMEM byte array (MSB-first, row-major)
  - BOOT_LOGO_W   : bitmap width  in pixels
  - BOOT_LOGO_H   : bitmap height in pixels

Usage:
    python3 Scripts/compileOledLogo.py [--size WxH] [--threshold T] [--input path/to/logo.png]
"""

import os
import io
import argparse
import shutil

from common import AUTOGEN_DEST_DIR, BUILD_DIR

# Source image (relative to repo root)
DEFAULT_SOURCE = "Assets/oledLogo.png"

# Output dimensions — fits on the left of a 128×64 OLED with text on the right
DEFAULT_WIDTH  = 48
DEFAULT_HEIGHT = 48

# Pixels darker than this threshold are treated as SET (white on OLED)
DEFAULT_THRESHOLD = 128

AUTOGEN_OLED_LOGO_H = "autoGenOledLogo.h"


def convertToBitmap(imagePath, width, height, threshold):
    """Load image, resize, threshold, return list-of-rows of bits."""
    try:
        from PIL import Image
    except ImportError:
        raise ImportError(
            "Pillow is required: pip install pillow"
        )

    img = Image.open(imagePath).convert("L")
    img = img.resize((width, height), Image.LANCZOS)
    pixels = img.load()

    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            # Source is black-on-white; dark pixel → set bit (drawn WHITE on OLED)
            row.append(1 if pixels[x, y] < threshold else 0)
        rows.append(row)
    return rows


def generateHeader(rows, width, height, sourceName):
    """Return the full C header file as a string."""
    bytes_per_row = (width + 7) // 8

    lines = []
    lines.append("/*")
    lines.append(" * autoGenOledLogo.h  —  DO NOT EDIT MANUALLY")
    lines.append(f" * Generated from: {sourceName}")
    lines.append(f" * Bitmap size: {width}x{height} pixels, {bytes_per_row} bytes/row")
    lines.append(" * Black pixels in the source image are set (drawn WHITE on OLED).")
    lines.append(" * Regenerate with: make autogen")
    lines.append(" */")
    lines.append("")
    lines.append("#ifndef AUTOGEN_OLED_LOGO_H")
    lines.append("#define AUTOGEN_OLED_LOGO_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <pgmspace.h>")
    lines.append("")
    lines.append(f"#define BOOT_LOGO_W {width}")
    lines.append(f"#define BOOT_LOGO_H {height}")
    lines.append("")
    lines.append("static const uint8_t PROGMEM bootLogo[] = {")

    row_strs = []
    for row_bits in rows:
        byte_strs = []
        for b in range(bytes_per_row):
            val = 0
            for bit in range(8):
                x = b * 8 + bit
                px = row_bits[x] if x < width else 0
                val = (val << 1) | px
            byte_strs.append(f"0x{val:02X}")
        row_strs.append("  " + ", ".join(byte_strs))

    lines.append(",\n".join(row_strs))
    lines.append("};")
    lines.append("")
    lines.append("#endif // AUTOGEN_OLED_LOGO_H")
    lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Convert a PNG logo to a PROGMEM OLED bitmap header")
    parser.add_argument("--input",     default=DEFAULT_SOURCE,
                        help=f"Source PNG (default: {DEFAULT_SOURCE})")
    parser.add_argument("--size",      default=f"{DEFAULT_WIDTH}x{DEFAULT_HEIGHT}",
                        help=f"Output size WxH (default: {DEFAULT_WIDTH}x{DEFAULT_HEIGHT})")
    parser.add_argument("--threshold", type=int, default=DEFAULT_THRESHOLD,
                        help=f"Darkness threshold 0-255 (default: {DEFAULT_THRESHOLD})")
    args = parser.parse_args()

    w, h = (int(v) for v in args.size.lower().split("x"))
    if w % 8 != 0:
        raise ValueError(f"Width must be a multiple of 8 (got {w})")

    print(f"OLED logo: {args.input} → {w}x{h}, threshold={args.threshold}")

    rows = convertToBitmap(args.input, w, h, args.threshold)
    header = generateHeader(rows, w, h, os.path.basename(args.input))

    os.makedirs(BUILD_DIR, exist_ok=True)
    build_path = os.path.join(BUILD_DIR, AUTOGEN_OLED_LOGO_H)
    with io.open(build_path, "w", newline="\n") as f:
        f.write(header)

    os.makedirs(AUTOGEN_DEST_DIR, exist_ok=True)
    dest_path = os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_OLED_LOGO_H)
    shutil.copy(build_path, dest_path)

    print(f"  Written: {dest_path}  ({w}x{h}, {w//8} bytes/row × {h} rows = {w//8*h} bytes)")


if __name__ == "__main__":
    main()
