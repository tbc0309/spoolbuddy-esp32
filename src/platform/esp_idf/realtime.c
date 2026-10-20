// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "realtime.h"
#include "realtime_url.h"
#include "settings.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static esp_websocket_client_handle_t socket_client;
static atomic_bool finished, forbidden;
static int64_t retry_at;
static bool denied;
static char *message;
static size_t message_used;
static void ws_event(void *argument, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)argument;
    (void)base;
    if (event_id == WEBSOCKET_EVENT_DISCONNECTED || event_id == WEBSOCKET_EVENT_ERROR) {
        const esp_websocket_event_data_t *details = event_data;
        if (details && (details->error_handle.esp_ws_handshake_status_code == 401 ||
                        details->error_handle.esp_ws_handshake_status_code == 403 ||
                        details->close_status_code == 4401))
            atomic_store(&forbidden, true);
        atomic_store(&finished, true);
        return;
    }
    if (event_id != WEBSOCKET_EVENT_DATA)
        return;
    esp_websocket_event_data_t *data = event_data;
    if (data->op_code == 8) {
        if (data->data_len >= 2 && (unsigned char)data->data_ptr[0] == 0x11 &&
            (unsigned char)data->data_ptr[1] == 0x31)
            atomic_store(&forbidden, true);
        return;
    }
    if (data->op_code != 1 && data->op_code != 0)
        return;
    if (!message)
        message = sb_memory_alloc(65537);
    if (!message || data->data_len < 0 || message_used + (size_t)data->data_len > 65536) {
        atomic_store(&finished, true);
        return;
    }
    if (data->payload_offset == 0 && data->op_code == 1)
        message_used = 0;
    memcpy(message + message_used, data->data_ptr, (size_t)data->data_len);
    message_used += (size_t)data->data_len;
    if (data->fin && data->payload_offset + data->data_len == data->payload_len) {
        message[message_used] = 0;
        cJSON *event = cJSON_ParseWithOpts(message, NULL, true);
        if (event)
            bridge_event(event);
        cJSON_Delete(event);
        message_used = 0;
    }
}
static void stop_socket(void) {
    if (socket_client) {
        esp_websocket_client_stop(socket_client);
        esp_websocket_client_destroy(socket_client);
        socket_client = NULL;
    }
    free(message);
    message = NULL;
    message_used = 0;
}
void sb_realtime_poll(bool connected, sb_transport_fn transport) {
    if (!connected) {
        stop_socket();
        return;
    }
    if (socket_client && !atomic_load(&finished))
        return;
    if (socket_client) {
        stop_socket();
        retry_at = esp_timer_get_time() + 3000000;
    }
    if (atomic_load(&forbidden))
        denied = true;
    if (denied || esp_timer_get_time() < retry_at)
        return;
    retry_at = esp_timer_get_time() + 5000000;
    int code = 0;
    cJSON *reply = transport("POST", "/auth/ws-token", NULL, &code);
    if (code == 401 || code == 403) {
        denied = true;
        cJSON_Delete(reply);
        return;
    }
    if (code < 200 || code >= 300) {
        cJSON_Delete(reply);
        return;
    }
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(reply, "token");
    char url[2048];
    bool valid = sb_realtime_url(url, sizeof(url), sb_settings.backend_url,
                                 cJSON_IsString(token) ? token->valuestring : "");
    cJSON_Delete(reply);
    if (!valid)
        return;
    // The SDK can print its URI on connect errors; this URI contains a short-lived credential.
    esp_log_level_set("websocket_client", ESP_LOG_NONE);
    esp_websocket_client_config_t config = {.uri = url,
                                            .disable_auto_reconnect = true,
                                            .network_timeout_ms = 3000,
                                            .buffer_size = 4096,
                                            .crt_bundle_attach = esp_crt_bundle_attach};
    socket_client = esp_websocket_client_init(&config);
    if (!socket_client)
        return;
    atomic_store(&finished, false);
    esp_websocket_register_events(socket_client, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    if (esp_websocket_client_start(socket_client) != ESP_OK) {
        stop_socket();
        return;
    }
}
