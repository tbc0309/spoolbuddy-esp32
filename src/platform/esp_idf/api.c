// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "api.h"
#include "response_limit.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "system_stats_esp.h"
#include "board.h"
#include "update.h"
#include "sdkconfig.h"
#include "settings.h"
#include "clock.h"
#include <sys/time.h>
#include <time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char device_id[32];
static int last_status;
int api_last_status(void) { return last_status; }
typedef struct {
    size_t used, capacity;
    bool overflow;
    char data[];
} response_t;
static esp_err_t receive(esp_http_client_event_t *event) {
    response_t *r = event->user_data;
    if(event->event_id==HTTP_EVENT_ON_HEADER && event->header_key &&
       !strcasecmp(event->header_key,"Date") && time(NULL)<1700000000) {
        int64_t epoch=sb_http_date_epoch(event->header_value);
        if(epoch>1700000000) {struct timeval now={.tv_sec=epoch};settimeofday(&now,NULL);}
    }
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        if (event->data_len < 0 || r->used + (size_t)event->data_len > r->capacity) {
            r->overflow = true;
            return ESP_FAIL;
        }
        memcpy(r->data + r->used, event->data, event->data_len);
        r->used += event->data_len;
        r->data[r->used] = 0;
    }
    return ESP_OK;
}
void api_init(void) {
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_BASE));
    snprintf(device_id, sizeof(device_id), "%s-%02x%02x%02x%02x%02x%02x", sb_board_device_prefix(),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
const char *api_device_id(void) { return device_id; }
void api_device_path(char *out, size_t n, const char *suffix) {
    snprintf(out, n, "/devices/%s/%s", device_id, suffix);
}
cJSON *api_payload(void) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "device_id", device_id);
    return p;
}
static cJSON *request_method(const char *method, const char *path, const cJSON *p, bool errors) {
    last_status = 0;
    char url[2048];
    const char *base = sb_settings.backend_url;
    size_t n = strlen(base);
    while (n && base[n - 1] == '/')
        n--;
    if (snprintf(url, sizeof(url), "%.*s/api/v1%s", (int)n, base, path) >= (int)sizeof(url))
        return NULL;
    size_t limit = sb_response_limit(path);
    response_t *r = sb_memory_calloc(1, sizeof(*r) + limit + 1);
    if (r)
        r->capacity = limit;
    char *body = p ? cJSON_PrintUnformatted(p) : NULL;
    if (!r || (p && !body)) {
        free(r);
        free(body);
        return NULL;
    }
    esp_http_client_config_t cfg = {.url = url,
                                    .timeout_ms = 3000,
                                    .event_handler = receive,
                                    .user_data = r,
                                    .crt_bundle_attach = esp_crt_bundle_attach,
                                    .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(r);
        free(body);
        return NULL;
    }
    esp_http_client_method_t verb = !strcmp(method, "PUT")      ? HTTP_METHOD_PUT
                                    : !strcmp(method, "PATCH")  ? HTTP_METHOD_PATCH
                                    : !strcmp(method, "DELETE") ? HTTP_METHOD_DELETE
                                    : !strcmp(method, "POST")   ? HTTP_METHOD_POST
                                                                : HTTP_METHOD_GET;
    esp_http_client_set_method(client, verb);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (strlen(sb_settings.api_key))
        esp_http_client_set_header(client, "X-API-Key", sb_settings.api_key);
    if (body)
        esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    last_status = err == ESP_OK ? status : 0;
    cJSON *result = NULL;
    if (err == ESP_OK && (errors || (status >= 200 && status < 300)) && !r->overflow)
        result = r->used ? cJSON_Parse(r->data) : cJSON_CreateObject();
    else
        ESP_LOGW("api", "%s: %s, HTTP %d", path, esp_err_to_name(err), status);
    esp_http_client_cleanup(client);
    free(body);
    free(r);
    return result;
}
static cJSON *request(const char *path, const cJSON *p) {
    return request_method(p ? "POST" : "GET", path, p, false);
}
cJSON *api_request(const char *method, const char *path, const cJSON *p, int *status) {
    cJSON *r = request_method(method, path, p, true);
    *status = last_status;
    return r;
}
cJSON *api_post(const char *path, const cJSON *p) {
    char full[160];
    snprintf(full, sizeof(full), "/spoolbuddy%s", path);
    return request(full, p);
}
cJSON *api_get(const char *path) { return request(path, NULL); }
static void network_fields(cJSON *p) {
    char ip[46] = "0.0.0.0";
    esp_netif_ip_info_t info;
    esp_netif_t *net = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (net && esp_netif_get_ip_info(net, &info) == ESP_OK)
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
    cJSON_AddStringToObject(p, "ip_address", ip);
    cJSON_AddStringToObject(p, "firmware_version", SB_FIRMWARE_VERSION);
    cJSON_AddStringToObject(p, "nfc_reader_type", "PN5180");
    cJSON_AddStringToObject(p, "nfc_connection", "SPI");
    cJSON_AddStringToObject(p, "backend_url", sb_settings.backend_url);
    cJSON *stats = sb_system_stats_collect();
    if (stats) {
        cJSON_AddItemToObject(p, "system_stats", stats);
    }
}
cJSON *api_register(bool nfc, bool scale, int tare, double factor) {
    cJSON *p = api_payload();
    network_fields(p);
    cJSON_AddStringToObject(p, "hostname", device_id);
    cJSON_AddBoolToObject(p, "has_nfc", nfc);
    cJSON_AddBoolToObject(p, "has_scale", scale);
    cJSON_AddBoolToObject(p, "has_backlight", true);
    cJSON_AddNumberToObject(p, "tare_offset", tare);
    cJSON_AddNumberToObject(p, "calibration_factor", factor);
    cJSON *r = api_post("/devices/register", p);
    cJSON_Delete(p);
    return r;
}
cJSON *api_heartbeat(bool nfc, bool scale) {
    cJSON *p = cJSON_CreateObject();
    network_fields(p);
    cJSON_AddBoolToObject(p, "nfc_ok", nfc);
    cJSON_AddBoolToObject(p, "scale_ok", scale);
    cJSON_AddNumberToObject(p, "uptime_s", esp_timer_get_time() / 1000000);
    char path[96];
    api_device_path(path, sizeof(path), "heartbeat");
    cJSON *r = api_post(path, p);
    cJSON_Delete(p);
    return r;
}

cJSON *api_post_absolute(const char *path, const cJSON *body) { return request(path, body); }
