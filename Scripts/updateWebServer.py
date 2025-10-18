import os
import json
import io
import shutil

HTML_DIR                    = "html"
BUILD_DIR                   = ".temp/AutoGen"
LINKER_FILE                 = "linkerData.json"
AUTOGEN_HTML_FILE           = "AutoGenHtmlData.h"
AUTOGEN_INFO_FILE           = "autoGenInfo.json"
AUTOGEN_WEB_SERVER_CPP_FILE = "autoGenWebServer.cpp"
AUTOGEN_WEB_SERVER_H_FILE   = "autoGenWebServer.h"
AUTOGEN_DEST_DIR            = "Src/AutoGen"

handlerFunctions = []
uriDefinations   = []
uriMacroList     = []

handlerFunctionsData = []
uriDefinationsData   = []
uriRegisterData      = []

linkerData  = None
autoGenInfo = None


def getLinkerData():
    global linkerData
    with open(os.path.join(HTML_DIR, LINKER_FILE), 'r') as file:
        linkerData = json.loads(file.read())


def getAutoGenInfoData():
    global autoGenInfo
    with open(os.path.join(BUILD_DIR, AUTOGEN_INFO_FILE), 'r') as file:
        autoGenInfo = json.loads(file.read())


def createWebServerFile(data):
    webServerFileName = os.path.join(BUILD_DIR, AUTOGEN_WEB_SERVER_CPP_FILE)
    webServerFile = io.open(webServerFileName, "w", newline='\n')
    webServerFile.write(data)
    webServerFile.close()


def getHanderFunction(handlerName, hook, arrStrName, arrStrLength):
    funcData = """
static esp_err_t """+handlerName+"""(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandelerHook("""+hook+""");
    return httpd_resp_send(req, (const char *)"""+arrStrName+""", """+arrStrLength+""");
}
"""
    return funcData


def getUriData(uriDefine, uri, handlerName):
    uriData="""
        httpd_uri_t """+uriDefine+""" = {
        .uri       = \""""+uri+"""\",
        .method    = HTTP_GET,
        .handler   = """+handlerName+""",
        .user_ctx  = NULL
    };
"""
    return uriData


def updateWebServerCppFile():
    global linkerData
    global autoGenInfo

    webServerData = ""

    # Genarate all function and uri data
    for htmlFile in autoGenInfo:
        for uri in linkerData:
            if linkerData[uri]["fileName"] == htmlFile:
                handlerFunc = htmlFile.replace(".html","Handler")
                uriDefine = "uri"+uri.replace("/","_")


                if handlerFunc not in handlerFunctions:
                    handlerFunctions.append(handlerFunc)

                    data = getHanderFunction (
                        handlerFunc,
                        linkerData[uri]["macro"],
                        autoGenInfo[htmlFile]["arrStrName"],
                        autoGenInfo[htmlFile]["arrStrLength"]
                    )
                    handlerFunctionsData.append(data)

                if uriDefine not in uriDefinations:
                    uriDefinations.append(uriDefine)

                    data = getUriData (
                        uriDefine,
                        uri,
                        handlerFunc
                    )
                    uriDefinationsData.append(data)

                uriRegisterData.append("""
        httpd_register_uri_handler(webServerHttpd, &"""+uriDefine+""");""")


    # Add header file
    webServerData += """/*
* This is a autogen file; Do not change manually
*/

#include "esp_http_server.h"
#include "AutoGen/autoGenHtmlData.h"
#include "Arduino.h"
#include "webServer.h"
#include "AutoGen/autoGenWebServer.h"

httpd_handle_t webServerHttpd = NULL;
"""

    # Add handler Fuctions
    for data in handlerFunctionsData:
        webServerData += data

    # Add Register Functions Start
    webServerData += """
void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
"""

    # Add Uri definations
    for data in uriDefinationsData:
        webServerData += data

    # Add Register Function middle
    webServerData += """
    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {"""

    # Add Uri register function
    for data in uriRegisterData:
        webServerData += data

    # Add Register Function end
    webServerData += """
    }
}
"""

     # Create webServer.cpp file
    createWebServerFile(webServerData)

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_WEB_SERVER_CPP_FILE)):
        if not os.path.exists(AUTOGEN_DEST_DIR):
            os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)
        shutil.copy(
            os.path.join(BUILD_DIR, AUTOGEN_WEB_SERVER_CPP_FILE),
            os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_WEB_SERVER_CPP_FILE)
        )


def updateWebServerHFile():
    global linkerData
    global autoGenInfo

    webServerHFileData = """
#ifndef AUTOGEN_WEBSERVER_H
#define AUTOGEN_WEBSERVER_H

typedef enum _
{
"""

    for htmlFile in autoGenInfo:
        for uri in linkerData:
            if linkerData[uri]["fileName"] == htmlFile:
                if linkerData[uri]["macro"] not in uriMacroList:
                    uriMacroList.append(linkerData[uri]["macro"])
                    webServerHFileData += "    "+linkerData[uri]["macro"]+",\n"

    webServerHFileData = webServerHFileData[:-2]
    webServerHFileData += """
} webServerMacro;

void startWebServer();

#endif
"""

    webServerHFileName = os.path.join(BUILD_DIR, AUTOGEN_WEB_SERVER_H_FILE)
    webServerFile = io.open(webServerHFileName, "w", newline='\n')
    webServerFile.write(webServerHFileData)
    webServerFile.close()

    if os.path.exists(webServerHFileName):
        if not os.path.exists(AUTOGEN_DEST_DIR):
            os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)
        shutil.copy(webServerHFileName, os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_WEB_SERVER_H_FILE))


def main():
    getLinkerData()
    getAutoGenInfoData()
    updateWebServerCppFile()
    updateWebServerHFile()


if __name__ == "__main__":
    main()
