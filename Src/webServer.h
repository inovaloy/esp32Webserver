#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "AutoGen/autoGenWebServer.h"

void webHandlerHook(webServerMacro hook);

// Hooks for API handlers
char* apiLoginHandlerHook(httpd_req_t *req);
char* apiRegisterHandlerHook(httpd_req_t *req);

#endif // WEB_SERVER_H
