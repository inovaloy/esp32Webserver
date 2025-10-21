#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// WebSocket frame types
typedef enum {
    WS_FRAME_TEXT = 0x01,
    WS_FRAME_BINARY = 0x02,
    WS_FRAME_CLOSE = 0x08,
    WS_FRAME_PING = 0x09,
    WS_FRAME_PONG = 0x0A
} ws_frame_type_t;

// WebSocket message structure
typedef struct {
    ws_frame_type_t type;
    char* payload;
    size_t len;
} ws_message_t;

// Function declarations
esp_err_t websocket_handler(httpd_req_t *req);
void websocket_send_to_all(const char* message);
void websocket_broadcast_sensor_data();

#ifdef __cplusplus
}
#endif

#endif // WEBSOCKET_HANDLER_H