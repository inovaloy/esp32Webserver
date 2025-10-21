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

htmlHandlerFunctions = []
apiHandlerFunctions  = []
uriDefinitions       = []
uriMacroList         = []

htmlHandlerFunctionsData  = []
apiHandlerFunctionsData   = []
uriHtmlDefinitionsData    = []
uriJsonDefinitionsData    = []
uriHtmlRegisterData       = []
uriJsonRegisterData       = []

linkerData  = None
autoGenInfo = None

def convertToCamelCase(data, separator="."):
    dataList = data.split(separator)
    camelString = dataList[0].lower()
    for i in range(1, len(dataList)):
        camelString += dataList[i][0].upper()+dataList[i][1:].lower()

    return camelString


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


def getHtmlHandlerFunction(handlerName, hook, arrStrName, arrStrLength):
    funcData = """
static esp_err_t """+handlerName+"""(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    webHandlerHook("""+hook+""");
    return send_large_response(req, (const char *)"""+arrStrName+""", """+arrStrLength+""");
}
"""
    return funcData


def getJsonHandlerFunction(handlerName, hook):
    funcData = """
static esp_err_t """+handlerName+"""(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *json_data = """+hook+"""();
    esp_err_t result = httpd_resp_sendstr(req, json_data);
    free(json_data);
    return result;
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
    global apiHandlerFunctions
    global uriDefinitions

    webServerData = ""

    # Generate all function and uri data
    for htmlFile in autoGenInfo:
        for uri in linkerData:
            if linkerData[uri]["rtnType"] == "HTML":
                if linkerData[uri]["fileName"] == htmlFile:
                    handlerFunc = htmlFile.replace(".html","Handler")
                    uriDefine = "uri"+uri.replace("/","_")

                    if handlerFunc not in htmlHandlerFunctions:
                        htmlHandlerFunctions.append(handlerFunc)
                        data = getHtmlHandlerFunction (
                            handlerFunc,
                            linkerData[uri]["macro"],
                            autoGenInfo[htmlFile]["arrStrName"],
                            autoGenInfo[htmlFile]["arrStrLength"]
                        )
                        htmlHandlerFunctionsData.append(data)

                    if uriDefine not in uriDefinitions:
                        uriDefinitions.append(uriDefine)
                        data = getUriData (
                            uriDefine,
                            uri,
                            handlerFunc
                        )
                        uriHtmlDefinitionsData.append(data)
                        uriHtmlRegisterData.append(f"\n        httpd_register_uri_handler(webServerHttpd, &{uriDefine});")

            if linkerData[uri]["rtnType"] == "JSON":
                uriDefine = "uri"+uri.replace("/","_")
                handlerFunc = convertToCamelCase(uri[1:]+"/Handler", "/")

                if handlerFunc not in apiHandlerFunctions:
                    apiHandlerFunctions.append(handlerFunc)
                    data = getJsonHandlerFunction (
                        handlerFunc,
                        handlerFunc+"Hook"
                    )
                    apiHandlerFunctionsData.append(data)

                if uriDefine not in uriDefinitions:
                    uriDefinitions.append(uriDefine)
                    data = getUriData (
                        uriDefine,
                        uri,
                        handlerFunc
                    )
                    uriJsonDefinitionsData.append(data)
                    uriJsonRegisterData.append(f"\n        httpd_register_uri_handler(webServerHttpd, &{uriDefine});")



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

// Helper function to send large content in chunks
static esp_err_t send_large_response(httpd_req_t *req, const char* data, size_t data_len) {
    const size_t chunk_size = 1024; // Send in 1KB chunks
    size_t remaining = data_len;

    while (remaining > 0) {
        size_t to_send = (remaining > chunk_size) ? chunk_size : remaining;
        if (httpd_resp_send_chunk(req, data, to_send) != ESP_OK) {
            return ESP_FAIL;
        }
        data += to_send;
        remaining -= to_send;
    }

    // Send empty chunk to signal end
    return httpd_resp_send_chunk(req, NULL, 0);
}
"""

    # Add handler Fuctions
    for data in htmlHandlerFunctionsData:
        webServerData += data

    # Add API Handlers Functions
    for data in apiHandlerFunctionsData:
        webServerData += data

    # Add Register Functions Start
    webServerData += """
void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase limits for larger responses
    config.max_resp_headers = 16;              // Increase max response headers
    config.stack_size       = 8192;            // Increase stack size (default is 4096)
    config.task_priority    = 5;               // Set task priority
    config.core_id          = tskNO_AFFINITY;  // Allow task to run on any core
    config.max_open_sockets = 7;               // Increase max open sockets if needed
"""
    webServerData += "\n    // HTML File Handlers and URIs"
    # Add HTML Uri definitions
    for data in uriHtmlDefinitionsData:
        webServerData += data

    webServerData += "\n    // API Handlers and URIs"
    # Add API Uri definitions
    for data in uriJsonDefinitionsData:
        webServerData += data

    # Add Register Function middle
    webServerData += """
    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        // Register static page handlers
    """

    # Add Uri register function
    for data in uriHtmlRegisterData:
        webServerData += data

    webServerData += "\n\n        // Register API handlers"
    # Add API Uri register function
    for data in uriJsonRegisterData:
        webServerData += data

    # Add Register Function end
    webServerData += """
    }
}
"""

     # Create webServer.cpp file
    createWebServerFile(webServerData)

    if os.path.exists(os.path.join(BUILD_DIR, AUTOGEN_WEB_SERVER_CPP_FILE)):
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
            if linkerData[uri]["rtnType"] != "HTML":
                continue
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
        os.makedirs(AUTOGEN_DEST_DIR, exist_ok = True)
        shutil.copy(webServerHFileName, os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_WEB_SERVER_H_FILE))


def main():
    getLinkerData()
    getAutoGenInfoData()
    updateWebServerCppFile()
    updateWebServerHFile()


if __name__ == "__main__":
    main()
