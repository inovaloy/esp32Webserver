import gzip
import os
import io
import shutil

HTML_DIR = "html"
BUILD_DIR = "_tmp"
AUTOGEN_HTML_FILE = "htmlData.h"

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
""")
    h.close()


def updateHeaderFile(zipName, zipData):
    zipDataLength = len(zipData)
    print("File:", zipName, "\tLength:", zipDataLength)
    arrStrName = convertToCamelCase(zipName)
    arrStrLength = arrStrName+"Len"

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
    htmlFiles = os.listdir(HTML_DIR)
    if not os.path.exists(BUILD_DIR):
        os.mkdir(BUILD_DIR)
    else:
        files = os.listdir(os.path.join(BUILD_DIR))
        for f in files:
            os.remove(os.path.join(BUILD_DIR, f))

    for htmlFile in htmlFiles:
        if htmlFile.endswith(".html"):
            fp = open(os.path.join(HTML_DIR, htmlFile),"rb")
            data = fp.read()
            bindata = bytearray(data)
            with gzip.open(os.path.join(BUILD_DIR, htmlFile+".gz"), "wb") as f:
                f.write(bindata)


def readZipFile():
    zipFiles = os.listdir(BUILD_DIR)

    for zipFile in zipFiles:
        if zipFile.endswith(".html.gz"):
            zip = open(os.path.join(BUILD_DIR, zipFile),'rb')
            zipData = list(zip.read())
            zip.close()

            if not os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE)):
                createHeaderFile()

            updateHeaderFile(zipFile, zipData)

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE)):
        shutil.copy(os.path.join(BUILD_DIR, AUTOGEN_HTML_FILE), AUTOGEN_HTML_FILE)


def main():
    createZipFiles()
    readZipFile()


if __name__ == '__main__':
    main()
