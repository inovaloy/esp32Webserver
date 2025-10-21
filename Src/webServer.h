#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "AutoGen/autoGenWebServer.h"

void webHandlerHook(webServerMacro hook);

// Hooks for API handlers
char* apiTestHandlerHook();
char* apiServerHandlerHook();
#endif
