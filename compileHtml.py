import gzip
import os
import io

def str2hex(decimal):
    hexadecimal = "0x"+hex(decimal)[2:].zfill(2).upper()
    return hexadecimal

fp = open("html/index.html","rb")
data = fp.read()
bindata = bytearray(data)
with gzip.open("index.html.gz", "wb") as f:
    f.write(bindata)

zip = open("index.html.gz",'rb')
zipData = list(zip.read())
zipDataLength = len(zipData)
print("len:", zipDataLength)
zip.close()

os.remove("index.html.gz")

h = io.open("htmlData.h", "w", newline='\n')
h.write("""/*
 * File: index.html.gz, Size: """+str(zipDataLength)+"""
 * This is a autogen file
 */
#define indexHtmlGzLen """+str(zipDataLength)+"""

const uint8_t indexHtmlGz[] = {
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


