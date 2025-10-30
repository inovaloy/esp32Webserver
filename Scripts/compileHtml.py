#!/usr/bin/env python3
"""
HTML Compilation Script for ESP32 Webserver
Compiles HTML files to compressed char arrays
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


InfoData          = {}
LinkerData        = []


def minifyHtml(htmlContent):
    """
    Minify HTML by removing unnecessary whitespace while preserving content
    inside tags where whitespace is important (pre, textarea, script, style).
    Also minifies CSS and JavaScript content.
    """
    # Preserve and minify CSS content inside <style> tags
    def minifyStyleContent(match):
        openingTag = match.group(1)
        cssContent = match.group(2)
        closingTag = match.group(3)
        minifiedCss = minifyCss(cssContent)
        return openingTag + minifiedCss + closingTag

    # Preserve and minify JavaScript content inside <script> tags
    def minifyScriptContent(match):
        openingTag = match.group(1)
        jsContent = match.group(2)
        closingTag = match.group(3)
        minifiedJs = minifyJavaScript(jsContent)
        return openingTag + minifiedJs + closingTag

    # Process <style> tags
    stylePattern = r'(<style[^>]*>)(.*?)(<\/style>)'
    htmlContent = re.sub(stylePattern, minifyStyleContent, htmlContent, flags = re.DOTALL | re.IGNORECASE)

    # Process <script> tags (only those without src attribute)
    scriptPattern = r'(<script(?![^>]*\ssrc\s*=)[^>]*>)(.*?)(<\/script>)'
    htmlContent = re.sub(scriptPattern, minifyScriptContent, htmlContent, flags = re.DOTALL | re.IGNORECASE)

    # Preserve content inside specific tags where whitespace matters (excluding style and script as they're handled above)
    preserveTags = ['pre', 'textarea']
    preservedContent = {}
    placeholderCounter = 0

    # Extract and replace content that should be preserved
    for tag in preserveTags:
        pattern = rf'(<{tag}[^>]*>)(.*?)(</{tag}>)'
        matches = re.finditer(pattern, htmlContent, re.DOTALL | re.IGNORECASE)
        for match in matches:
            placeholder = f"__PRESERVE_{placeholderCounter}__"
            preservedContent[placeholder] = match.group(0)
            htmlContent = htmlContent.replace(match.group(0), placeholder)
            placeholderCounter += 1

    # Remove HTML comments (but keep conditional comments for IE)
    htmlContent = re.sub(r'<!--(?!\[if\s)(?!.*\[endif\]).*?-->', '', htmlContent, flags = re.DOTALL)

    # Remove multiple whitespace characters (spaces, tabs, newlines) and replace with single space
    htmlContent = re.sub(r'\s+', ' ', htmlContent)

    # Remove spaces around HTML tags (but not inside attributes or content)
    htmlContent = re.sub(r'>\s+<', '><', htmlContent)  # Between tags
    htmlContent = re.sub(r'>\s+', '>', htmlContent)    # After opening tags
    htmlContent = re.sub(r'\s+<', '<', htmlContent)    # Before closing tags

    # Remove spaces around equal signs in attributes
    htmlContent = re.sub(r'\s*=\s*', '=', htmlContent)

    # Remove leading and trailing whitespace
    htmlContent = htmlContent.strip()

    # Restore preserved content
    for placeholder, content in preservedContent.items():
        htmlContent = htmlContent.replace(placeholder, content)

    return htmlContent


def readLinkerDataFile():
    global LinkerData
    linkerData = readLinkerData(LINKER_DATA_FILE)
    print(f"Loading HTML file mappings from: {LINKER_DATA_FILE}")

    for route, config in linkerData.items():
        returnType = config.get("rtnType", "")
        if returnType != "HTML":
            continue
        fileName = config["fileName"]
        LinkerData.append(fileName)

    print(f"Found {len(LinkerData)} HTML files to process")


def createHeaderFile():
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_H), "w", newline = '\n')
    h.write("""/*
* This is an autogen file; Do not change manually
* Contains compressed HTML files for ESP32 Webserver
*/

#ifndef AUTOGEN_HTML_H
#define AUTOGEN_HTML_H

#include <stdint.h>
""")
    h.close()


def updateHeaderFile(zipName, zipData, zipDataLength, arrStrName, arrStrLength):
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_H), "a", newline = '\n')
    h.write("""
// File: """+str(zipName)+""", Size: """+str(zipDataLength)+"""
#define """+arrStrLength+""" """+str(zipDataLength)+"""

const uint8_t """+arrStrName+"""[] = {
""")
    index = 0
    htmlData = "    "
    for a in zipData:
        htmlData += str2hex(a)+", "
        index += 1
        if index == 16:
            htmlData = htmlData[:-1]
            htmlData += "\n    "
            index = 0

    if index == 0:
        htmlData = htmlData[:-6]
    else:
        htmlData = htmlData[:-2]
    h.write(htmlData)
    h.write("""
};
""")
    h.close()


def createZipFiles(enableCompression = True):
    global InfoData
    global LinkerData
    htmlFiles = os.listdir(HTML_DIR)
    os.makedirs(BUILD_DIR, exist_ok = True)

    # Create a subdirectory for compressed HTML files
    compressedHtmlDir = os.path.join(BUILD_DIR, "compressed_html")
    os.makedirs(compressedHtmlDir, exist_ok = True)

    for htmlFile in htmlFiles:
        if htmlFile not in LinkerData:
            print("Skipping file:", htmlFile)
            continue
        if htmlFile.endswith(".html"):
            # Read original HTML file
            with open(os.path.join(HTML_DIR, htmlFile), "r", encoding = 'utf-8') as fp:
                originalHtml = fp.read()

            # Apply minification based on enableCompression flag
            if enableCompression:
                minifiedHtml = minifyHtml(originalHtml)
                statusText = "Minified"
            else:
                minifiedHtml = originalHtml
                statusText = "Original"

            # Save compressed HTML file for reference
            compressedHtmlPath = os.path.join(compressedHtmlDir, htmlFile)
            with open(compressedHtmlPath, "w", encoding = 'utf-8') as fp:
                fp.write(minifiedHtml)

            # Convert HTML to binary data
            bindata = bytearray(minifiedHtml.encode('utf-8'))

            # Create gzip file
            zipFileName = htmlFile+".gz"
            InfoData[htmlFile] = {}
            InfoData[htmlFile]["zipFileName"]        = zipFileName
            InfoData[htmlFile]["originalSize"]       = len(originalHtml.encode('utf-8'))
            InfoData[htmlFile]["minifiedSize"]       = len(minifiedHtml.encode('utf-8'))
            InfoData[htmlFile]["compressionEnabled"] = enableCompression

            zipFilePath = os.path.join(BUILD_DIR, zipFileName)
            with gzip.open(zipFilePath, "wb") as f:
                f.write(bindata)

            # Print compression statistics
            originalSize = InfoData[htmlFile]["originalSize"]
            minifiedSize = InfoData[htmlFile]["minifiedSize"]
            compressionRatio = ((originalSize - minifiedSize) / originalSize) * 100 if originalSize > 0 else 0

            if enableCompression:
                print(f"File: {htmlFile:<20} Original: {originalSize:>6} bytes, {statusText}: {minifiedSize:>6} bytes, Saved: {compressionRatio:>5.1f}%")
            else:
                print(f"File: {htmlFile:<20} Size: {originalSize:>6} bytes (compression disabled)")


def readZipFile():
    global InfoData
    zipFiles = os.listdir(BUILD_DIR)
    createHeaderFile()

    for zipFile in zipFiles:
        if zipFile.endswith(".html.gz"):
            zip = open(os.path.join(BUILD_DIR, zipFile),'rb')
            zipData = list(zip.read())
            zip.close()

            zipDataLength = len(zipData)
            print(f"Compressed file: \t{zipFile:<30} Length: {zipDataLength}")
            arrStrName = convertToCamelCase(zipFile)
            arrStrLength = arrStrName+"Len"

            for key in InfoData:
                if InfoData[key]["zipFileName"] == zipFile:
                    InfoData[key]["zipDataLength"] = zipDataLength
                    InfoData[key]["arrStrName"] = arrStrName
                    InfoData[key]["arrStrLength"] = arrStrLength
                    break

            updateHeaderFile(zipFile, zipData, zipDataLength, arrStrName, arrStrLength)

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_H)):
        h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_H), "a", newline = '\n')
        h.write("""
#endif // AUTOGEN_HTML_H
""")
        h.close()

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_H)):
        os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)

        shutil.copy(
            os.path.join(BUILD_DIR, AUTOGEN_HTML_H),
            os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_HTML_H)
        )

    with open(os.path.join(BUILD_DIR, AUTOGEN_HTML_INFO_FILE), 'w') as autoGenInfoFile:
        autoGenInfoFile.write(json.dumps(InfoData, indent=4))


def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description = 'Compile HTML files to header file with optional compression')
    parser.add_argument('-nc', '--no-compress', action = 'store_true',
                       help = 'Disable HTML/CSS/JavaScript minification (compression enabled by default)')

    # Parse arguments
    args = parser.parse_args()

    # Determine compression setting (enabled by default, disabled with --no-compress)
    enableCompression = not args.no_compress

    if enableCompression:
        print("HTML/CSS/JavaScript compression: ENABLED")
    else:
        print("HTML/CSS/JavaScript compression: DISABLED")
    print("-" * 60)

    readLinkerDataFile()
    createZipFiles(enableCompression)
    readZipFile()


if __name__ == '__main__':
    main()
