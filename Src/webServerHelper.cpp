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
    size_t buf_len = req->content_len;
    if (buf_len == 0) return nullptr;

    char* buf = (char*)malloc(buf_len + 1);
    if (buf == nullptr) {
        Serial.println("[HTTP] malloc failed for request body");
        return nullptr;
    }

    // httpd_req_recv may return fewer bytes than requested (TCP segmentation).
    // Loop until the full body is received.
    size_t received = 0;
    while (received < buf_len) {
        int ret = httpd_req_recv(req, buf + received, buf_len - received);
        if (ret <= 0) {
            Serial.printf("[HTTP] recv failed: ret=%d received=%d/%d\n", ret, received, buf_len);
            free(buf);
            return nullptr;
        }
        received += ret;
    }
    buf[buf_len] = '\0';
    return buf;
}
