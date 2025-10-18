import gzip
import os
import io
import shutil
import json

HTML_DIR          = "html"
BUILD_DIR         = ".temp/AutoGen"
AUTOGEN_HTML_FILE = "autoGenHtmlData.h"
AUTOGEN_INFO_FILE = "autoGenInfo.json"
AUTOGEN_DEST_DIR  = "Src/AutoGen"
InfoData = {}

def str2hex(decimal):
    hexadecimal = "0x"+hex(decimal)[2:].zfill(2).upper()
    return hexadecimal


def convertToCamelCase(data, separator="."):
    dataList = data.split(separator)
    camelString = dataList[0].lower()
    for i in range(1, len(dataList)):
        camelString += dataList[i][0].upper()+dataList[i][1:].lower()

    return camelString


def createHeaderFile():
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE), "w", newline='\n')
    h.write("""/*
* This is a autogen file; Do not change manually
*/

#include <stdint.h>
""")
    h.close()


def updateHeaderFile(zipName, zipData, zipDataLength, arrStrName, arrStrLength):
    h = io.open(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE), "a", newline='\n')
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


def createZipFiles():
    global InfoData
    htmlFiles = os.listdir(HTML_DIR)
    if not os.path.exists(BUILD_DIR):
        os.makedirs(BUILD_DIR, exist_ok = True)
    else:
        files = os.listdir(os.path.join(BUILD_DIR))
        for f in files:
            os.remove(os.path.join(BUILD_DIR, f))

    for htmlFile in htmlFiles:
        if htmlFile.endswith(".html"):
            fp = open(os.path.join(HTML_DIR, htmlFile),"rb")
            data = fp.read()
            bindata = bytearray(data)
            zipFileName = htmlFile+".gz"
            InfoData[htmlFile] = {}
            InfoData[htmlFile]["zipFileName"] = zipFileName
            zipFilePath = os.path.join(BUILD_DIR, zipFileName)
            with gzip.open(zipFilePath, "wb") as f:
                f.write(bindata)


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
            print("File:", zipFile, "\tLength:", zipDataLength)
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
        if not os.path.exists(AUTOGEN_DEST_DIR):
            os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)

        shutil.copy(
            os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE),
            os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_HTML_FILE)
        )

    with open(os.path.join(BUILD_DIR, AUTOGEN_INFO_FILE), 'w') as autoGenInfoFile:
        autoGenInfoFile.write(json.dumps(InfoData, indent=4))

def main():
    createZipFiles()
    readZipFile()


if __name__ == '__main__':
    main()
