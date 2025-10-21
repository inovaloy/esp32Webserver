#ifndef WEB_SERVER_SD_H
#define WEB_SERVER_SD_H

#include "esp_http_server.h"

// Define whether to use SD card for HTML files
// Set to true to read from SD card, false to use compiled HTML
extern bool useSDCardForHTML;

// Initialize and start web server with SD card support
void startWebServerWithSD(bool enableSD = true, int sdCardPin = 5);

#endif // WEB_SERVER_SD_H
