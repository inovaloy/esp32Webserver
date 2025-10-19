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
    return send_large_response(req, (const char *)"""+arrStrName+""", """+arrStrLength+""");
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
    for data in handlerFunctionsData:
        webServerData += data

    # Add Register Functions Start
    webServerData += """
void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase limits for larger responses
    config.max_resp_headers = 16;     // Increase max response headers
    config.stack_size = 8192;         // Increase stack size (default is 4096)
    config.task_priority = 5;         // Set task priority
    config.core_id = tskNO_AFFINITY;  // Allow task to run on any core
    config.max_open_sockets = 7;      // Increase max open sockets if needed
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
