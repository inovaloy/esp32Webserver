import gzip
import os
import io
import shutil
import json
import re
import argparse

HTML_DIR          = "html"
LINKER_DATA_FILE  = "linkerData.json"
BUILD_DIR         = ".temp/AutoGen"
AUTOGEN_HTML_FILE = "autoGenHtmlData.h"
AUTOGEN_INFO_FILE = "autoGenInfo.json"
AUTOGEN_DEST_DIR  = "Src/AutoGen"
InfoData          = {}
LinkerData        = []

def str2hex(decimal):
    hexadecimal = "0x"+hex(decimal)[2:].zfill(2).upper()
    return hexadecimal


def minifyCss(cssContent):
    """
    Minify CSS by removing unnecessary whitespace and comments.
    """
    # Remove CSS comments
    cssContent = re.sub(r'/\*.*?\*/', '', cssContent, flags = re.DOTALL)

    # Remove unnecessary whitespace around CSS syntax
    cssContent = re.sub(r'\s*([{}:;,])\s*', r'\1', cssContent)

    # Remove multiple whitespace and newlines
    cssContent = re.sub(r'\s+', ' ', cssContent)

    # Remove leading and trailing whitespace
    cssContent = cssContent.strip()

    return cssContent


def minifyJavaScript(jsContent):
    """
    Minify JavaScript by removing unnecessary whitespace and comments.
    Uses a safer approach to preserve functionality.
    """
    # Remove single-line comments (but be careful with // in strings and URLs)
    jsContent = re.sub(r'(?<!:)//(?!/).*?(?=\n|$)', '', jsContent)

    # Remove multi-line comments
    jsContent = re.sub(r'/\*.*?\*/', '', jsContent, flags = re.DOTALL)

    # Remove leading and trailing whitespace from each line
    lines = jsContent.split('\n')
    lines = [line.strip() for line in lines if line.strip()]

    # Join lines with single space
    jsContent = ' '.join(lines)

    # Remove excessive whitespace (replace multiple spaces with single space)
    jsContent = re.sub(r'\s+', ' ', jsContent)

    # Remove spaces around specific punctuation where it's safe
    jsContent = re.sub(r'\s*([{}();,])\s*', r'\1', jsContent)
    jsContent = re.sub(r'\s*:\s*', r':', jsContent)  # Object property colons
    jsContent = re.sub(r'\s*=\s*', r'=', jsContent)  # Assignment operators

    # Remove spaces around brackets
    jsContent = re.sub(r'\s*(\[)\s*', r'\1', jsContent)
    jsContent = re.sub(r'\s*(\])\s*', r'\1', jsContent)

    # Clean up any remaining multiple spaces
    jsContent = re.sub(r'\s+', ' ', jsContent)

    # Remove leading and trailing whitespace
    jsContent = jsContent.strip()

    return jsContent


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


def convertToCamelCase(data, separator = "."):
    dataList = data.split(separator)
    camelString = dataList[0].lower()
    for i in range(1, len(dataList)):
        camelString += dataList[i][0].upper() + dataList[i][1:].lower()

    return camelString


def readLinkerDataFile():
    global LinkerData
    with open(os.path.join(HTML_DIR, LINKER_DATA_FILE), 'r') as file:
        jsonData = json.loads(file.read())
        for item in jsonData:
            returnType = jsonData[item]["rtnType"]
            if returnType != "HTML":
                continue
            fileName = jsonData[item]["fileName"]
            LinkerData.append(fileName)


def createHeaderFile():
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE), "w", newline = '\n')
    h.write("""/*
* This is a autogen file; Do not change manually
*/

#include <stdint.h>
""")
    h.close()


def updateHeaderFile(zipName, zipData, zipDataLength, arrStrName, arrStrLength):
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE), "a", newline = '\n')
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
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(BUILD_DIR)

    # Create a subdirectory for compressed HTML files
    compressedHtmlDir = os.path.join(BUILD_DIR, "compressed_html")
    os.makedirs(compressedHtmlDir)

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

    for zipFile in zipFiles:
        if zipFile.endswith(".html.gz"):
            zip = open(os.path.join(BUILD_DIR, zipFile),'rb')
            zipData = list(zip.read())
            zip.close()

            if not os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE)):
                createHeaderFile()

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

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE)):
        os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)

        shutil.copy(
            os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE),
            os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_HTML_FILE)
        )

    with open(os.path.join(BUILD_DIR, AUTOGEN_INFO_FILE), 'w') as autoGenInfoFile:
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
