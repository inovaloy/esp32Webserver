#!/usr/bin/env python3
"""
Enhanced Web Server Generator for ESP32
Generates web server code that handles both HTML pages and static assets (CSS, JS, images)
"""

import os
import io
import shutil
import subprocess

from common import *
from utility import *


def getGitHash():
    """Return short git hash, or 'dev' if not in a git repo"""
    try:
        return subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return 'dev'


def createWebServerFiles():
    """Generate enhanced web server files with asset support"""
    linkerData = readLinkerData(LINKER_DATA_FILE)

    # Validate the linker data structure
    validateLinkerData(linkerData)

    print(f"Successfully loaded linker data from: {LINKER_DATA_FILE}")
    print(f"Found {len(linkerData)} routes to process")

    # Separate HTML pages and assets
    htmlPages = []
    assets = []
    apiEndpoints = []

    for route, config in linkerData.items():
        if config["rtnType"] == "HTML":
            print(f"Found HTML page: {route} -> {config['fileName']}")
            htmlPages.append({
                'route': route,
                'fileName': config["fileName"],
                'macro': config["macro"],
                'method': config["reqType"]
            })
        elif config["rtnType"] == "ASSET":
            print(f"Found Asset: {route} -> {config['fileName']}")
            assets.append({
                'route': route,
                'fileName': config["fileName"],
                'contentType': config["contentType"],
                'macro': config["macro"],
                'method': config["reqType"]
            })
        elif config["rtnType"] == "JSON":
            print(f"Found API Endpoint: {route}")
            apiEndpoints.append({
                'route': route,
                'method': config["reqType"]
            })

    # Generate header file
    generateHeaderFile(htmlPages, assets, apiEndpoints)

    # Generate implementation file
    generateImplementationFile(htmlPages, assets, apiEndpoints)

    # Copy to destination
    os.makedirs(AUTOGEN_DEST_DIR, exist_ok=True)
    shutil.copy(
        os.path.join(BUILD_DIR, AUTOGEN_WEBSERVER_H),
        os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_WEBSERVER_H)
    )
    shutil.copy(
        os.path.join(BUILD_DIR, AUTOGEN_WEBSERVER_CPP),
        os.path.join(AUTOGEN_DEST_DIR, AUTOGEN_WEBSERVER_CPP)
    )

    print(f"Generated web server files with {len(htmlPages)} HTML pages, {len(assets)} assets, and {len(apiEndpoints)} API endpoints")


def generateHeaderFile(htmlPages, assets, apiEndpoints):
    """Generate header file"""
    header_path = os.path.join(BUILD_DIR, AUTOGEN_WEBSERVER_H)

    with io.open(header_path, "w", newline='\n') as h:
        h.write("""/*
* This is an autogen file; Do not change manually
* Enhanced Web Server for ESP32 with asset support
*/

#ifndef AUTOGEN_WEBSERVER_H
#define AUTOGEN_WEBSERVER_H

#include "esp_http_server.h"

// Web server handle
extern httpd_handle_t webServerHttpd;

// Main functions
void startWebServer();
void stopWebServer();

// HTML page macros
typedef enum {
""")

        # Add HTML page macros
        for page in htmlPages:
            h.write(f"    {page['macro']},\n")

        h.write("""} webServerMacro;

// Hook functions (to be implemented by user)
void webHandlerHook(webServerMacro hook);
""")

        # Add API handler declarations
        for endpoint in apiEndpoints:
            functionName = getFunctionNameFromRoute(endpoint['route'])
            h.write(f"char* {functionName}HandlerHook(httpd_req_t *req);\n")

        h.write("""
#endif // AUTOGEN_WEBSERVER_H
""")


def generateImplementationFile(htmlPages, assets, apiEndpoints):
    """Generate implementation file"""
    cpp_path = os.path.join(BUILD_DIR, AUTOGEN_WEBSERVER_CPP)

    with io.open(cpp_path, "w", newline='\n') as cpp:
        cpp.write("""/*
* This is an autogen file; Do not change manually
* Enhanced Web Server implementation for ESP32
*/

#include "Arduino.h"
#include "esp_http_server.h"

#include "AutoGen/autoGenHtmlData.h"
#include "AutoGen/autoGenWebServer.h"
""")

        if assets:
            cpp.write('#include "AutoGen/autoGenAssets.h"\n')

        cpp.write("""
#include "webServer.h"
#include "webServerHelper.h"


httpd_handle_t webServerHttpd = NULL;

""")

        # Generate HTML page handlers
        for page in htmlPages:
            generateHtmlHandler(cpp, page)

        # Generate asset handlers
        gitHash = getGitHash()
        for asset in assets:
            generateAssetHandler(cpp, asset, gitHash)

        # Generate API handlers
        for endpoint in apiEndpoints:
            generateApiHandler(cpp, endpoint)

        # Generate startWebServer function
        generateStartWebServerFunction(cpp, htmlPages, assets, apiEndpoints)

        # Generate stopWebServer function
        cpp.write("""
void stopWebServer() {
    if (webServerHttpd != NULL) {
        httpd_stop(webServerHttpd);
        webServerHttpd = NULL;
        Serial.println("Web server stopped");
    }
}
""")


def generateHtmlHandler(cpp, page):
    """Generate handler for HTML page"""
    handlerName = f"{convertToCamelCase(page['fileName'])}Handler"
    arrayName = convertToCamelCase(page['fileName'] + '.gz')
    arrayLen = arrayName + "Len"

    cpp.write(f"""
static esp_err_t {handlerName}(httpd_req_t *req){{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    webHandlerHook({page['macro']});
    return sendLargeResponse(req, (const char *){arrayName}, {arrayLen});
}}
""")


def generateAssetHandler(cpp, asset, gitHash='dev'):
    """Generate handler for static asset"""
    handlerName = f"{convertToCamelCase(asset['fileName'])}Handler"
    arrayName = convertToCamelCase(asset['fileName'])
    arrayLen = arrayName + "Len"

    cpp.write(f"""
static esp_err_t {handlerName}(httpd_req_t *req){{
    httpd_resp_set_type(req, "{asset['contentType']}");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    httpd_resp_set_hdr(req, "ETag", "\\"{gitHash}\\"");
    return sendLargeResponse(req, (const char *){arrayName}, {arrayLen});
}}
""")


def generateApiHandler(cpp, endpoint):
    """Generate handler for API endpoint"""
    functionName = getFunctionNameFromRoute(endpoint['route'])
    handlerName = f"{functionName}Handler"

    cpp.write(f"""
static esp_err_t {handlerName}(httpd_req_t *req) {{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    char *jsonData = {functionName}HandlerHook(req);
    if (jsonData != NULL) {{
        esp_err_t result = httpd_resp_sendstr(req, jsonData);
        free(jsonData);
        return result;
    }} else {{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }}
}}
""")


def generateStartWebServerFunction(cpp, htmlPages, assets, apiEndpoints):
    """Generate the main startWebServer function"""
    cpp.write("""
void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase limits for larger responses and more handlers
    config.max_resp_headers = 16;
    config.stack_size       = 12288;           // Increased stack size for asset handling
    config.task_priority    = 5;
    config.core_id          = tskNO_AFFINITY;
    config.max_open_sockets = 10;              // Increased for concurrent requests
""")

    # Calculate total handlers needed
    total_handlers = len(htmlPages) + len(assets) + len(apiEndpoints)
    cpp.write(f"    config.max_uri_handlers = {total_handlers + 5};              // Total handlers + buffer\n\n")

    # Generate URI structures for HTML pages
    for page in htmlPages:
        handlerName = f"{convertToCamelCase(page['fileName'])}Handler"
        uriName = f"uri_{convertToCamelCase(page['route'].replace('/', '_'))}"

        cpp.write(f"""    // HTML Page: {page['route']}
    httpd_uri_t {uriName} = {{
        .uri       = "{page['route']}",
        .method    = HTTP_{page['method']},
        .handler   = {handlerName},
        .user_ctx  = NULL
    }};
""")
    cpp.write("\n// *********************************** //\n\n")

    # Generate URI structures for assets
    for asset in assets:
        handlerName = f"{convertToCamelCase(asset['fileName'])}Handler"
        uriName = f"uri_{convertToCamelCase(asset['route'].replace('/', '_').replace('.', '_'))}"

        cpp.write(f"""    // Asset: {asset['route']}
    httpd_uri_t {uriName} = {{
        .uri       = "{asset['route']}",
        .method    = HTTP_{asset['method']},
        .handler   = {handlerName},
        .user_ctx  = NULL
    }};
""")
    cpp.write("\n// *********************************** //\n\n")

    # Generate URI structures for API endpoints
    for endpoint in apiEndpoints:
        functionName = getFunctionNameFromRoute(endpoint['route'])
        handlerName = f"{functionName}Handler"
        uriName = f"uri_{convertToCamelCase(endpoint['route'].replace('/', '_').replace('.', '_'))}"

        cpp.write(f"""    // API Endpoint: {endpoint['route']}
    httpd_uri_t {uriName} = {{
        .uri       = "{endpoint['route']}",
        .method    = HTTP_{endpoint['method']},
        .handler   = {handlerName},
        .user_ctx  = NULL
    }};
""")

    # Server startup and registration
    cpp.write("""
    if (httpd_start(&webServerHttpd, &config) == ESP_OK) {
        Serial.println("Enhanced web server started successfully!");
        esp_err_t returnCode;
""")

    # Register HTML page handlers
    cpp.write("\n        // Register HTML page handlers\n")
    for page in htmlPages:
        uriName = f"uri_{convertToCamelCase(page['route'].replace('/', '_'))}"
        cpp.write(f"""        returnCode = httpd_register_uri_handler(webServerHttpd, &{uriName});
        if (returnCode != ESP_OK) {{
            Serial.println("Failed to register URI handler for {page['route']}. ERROR: 0x" + String(returnCode, HEX));
        }}
""")

    # Register asset handlers
    if assets:
        cpp.write("\n        // Register asset handlers\n")
        for asset in assets:
            uriName = f"uri_{convertToCamelCase(asset['route'].replace('/', '_').replace('.', '_'))}"
            cpp.write(f"""        returnCode = httpd_register_uri_handler(webServerHttpd, &{uriName});
        if (returnCode != ESP_OK) {{
            Serial.println("Failed to register URI handler for {asset['route']}. ERROR: 0x" + String(returnCode, HEX));
        }}
""")

    # Register API handlers
    if apiEndpoints:
        cpp.write("\n        // Register API handlers\n")
        for endpoint in apiEndpoints:
            uriName = f"uri_{convertToCamelCase(endpoint['route'].replace('/', '_').replace('.', '_'))}"
            cpp.write(f"""        returnCode = httpd_register_uri_handler(webServerHttpd, &{uriName});
        if (returnCode != ESP_OK) {{
            Serial.println("Failed to register URI handler for {endpoint['route']}. ERROR: 0x" + String(returnCode, HEX));
        }}
""")

    cpp.write("""    } else {
        Serial.println("Failed to start enhanced web server!");
    }
}
""")


def getFunctionNameFromRoute(route):
    """Convert API route to function name"""
    # Remove leading slash and convert to camelCase
    name = route.lstrip('/')
    # Replace slashes with underscores, then convert to camelCase
    name = name.replace('/', '_')
    return convertToCamelCase(name, '_')


def main():
    """Main function"""
    # Ensure build directory exists
    os.makedirs(BUILD_DIR, exist_ok=True)

    createWebServerFiles()

    print("Enhanced web server generation completed!")
    print(f"Files generated in: {AUTOGEN_DEST_DIR}")

if __name__ == '__main__':
    main()
