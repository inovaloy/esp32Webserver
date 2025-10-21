#include "esp_http_server.h"
#include "Arduino.h"


// Helper function to send large content in chunks
esp_err_t sendLargeResponse(httpd_req_t *req, const char* data, size_t dataLen) {
    const size_t chunkSize = 1024; // Send in 1KB chunks
    size_t remaining = dataLen;

    while (remaining > 0) {
        size_t toSend = (remaining > chunkSize) ? chunkSize : remaining;
        if (httpd_resp_send_chunk(req, data, toSend) != ESP_OK) {
            return ESP_FAIL;
        }
        data += toSend;
        remaining -= toSend;
    }

    // Send empty chunk to signal end
    return httpd_resp_send_chunk(req, NULL, 0);
}


char* getContentFromReq(httpd_req_t *req) {
    char* buf = nullptr;
    size_t buf_len = req->content_len;

    if (buf_len > 0) {
        buf = (char*)malloc(buf_len + 1);
        if (buf == nullptr) {
            Serial.println("Failed to allocate memory for request content");
            return nullptr;
        }

        int ret = httpd_req_recv(req, buf, buf_len);
        if (ret <= 0) {
            Serial.println("Failed to receive request content");
            free(buf);
            return nullptr;
        }
        buf[buf_len] = '\0'; // Null-terminate the string
    }
    return buf;
}
