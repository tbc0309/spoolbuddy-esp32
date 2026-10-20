#include "memory_esp.h"
#include "bridge.h"
#include "realtime.h"
#include "ui_live.h"
#include "weight_math.h"
#include "optional_inputs.h"
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "api.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pn5180.h"
#include "scale.h"
#include "sdkconfig.h"
#include "ui.h"
#include "settings.h"
#include "board.h"
#include "update.h"
#include "settings_store_esp.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>

typedef struct {
    char path[96];
    cJSON *body;
} event_t;
typedef struct {
    int spool;
    size_t length;
    uint8_t data[888];
} write_t;
static EventGroupHandle_t wifi_events;
static atomic_bool wifi_reconfiguring;
static QueueHandle_t outgoing, writes, weights;
static SemaphoreHandle_t state_mutex;
static nvs_handle_t storage;
static struct {
    bool nfc, scale, have_raw, stable;
    int32_t raw, tare;
    double factor;
    int64_t write_until;
    char uid[21];
} state;
static bool tare_pending;
static write_t issued_write;

static bool emit(const char *path, cJSON *p) {
    event_t event = {.body = p};
    snprintf(event.path, sizeof(event.path), "%s", path);
    if (xQueueSend(outgoing, &event, 0) != pdTRUE) {
        ESP_LOGW("events", "Queue full, dropping %s", path);
        cJSON_Delete(p);
        return false;
    }
    return true;
}
static bool emit_device(const char *suffix, cJSON *p) {
    char path[96];
    api_device_path(path, sizeof(path), suffix);
    return emit(path, p);
}
static void publish_weight(cJSON *p) {
    event_t next = {.body = p}, old = {0};
    snprintf(next.path, sizeof(next.path), "/scale/reading");
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    xQueueReceive(weights, &old, 0);
    bool sent = xQueueSend(weights, &next, 0) == pdTRUE;
    xSemaphoreGive(state_mutex);
    cJSON_Delete(old.body);
    if (!sent)
        cJSON_Delete(p);
}
static void publish_override(float grams, void *context) {
    event_t *event = context;
    if (event->body && !strcmp(event->path, "/scale/reading")) {
        cJSON_Delete(event->body);
        event->body = NULL;
    }
    cJSON *p = api_payload();
    cJSON_AddNumberToObject(p, "weight_grams", grams);
    cJSON_AddBoolToObject(p, "stable", true);
    publish_weight(p);
}
static void calibration(int32_t tare, double factor) {
    if (!isfinite(factor) || fabs(factor) < 1e-9)
        return;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    bool changed = tare != state.tare || factor != state.factor;
    state.tare = tare;
    state.factor = factor;
    xSemaphoreGive(state_mutex);
    if (changed) {
        nvs_set_i32(storage, "tare", tare);
        nvs_set_blob(storage, "factor", &factor, sizeof(factor));
        nvs_commit(storage);
    }
}
static void do_tare(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    bool ready = state.scale && state.have_raw;
    int32_t raw = state.raw;
    double factor = state.factor;
    xSemaphoreGive(state_mutex);
    if (!ready) {
        ui_status("port.platform.TareFailedScaleNotReady");
        return;
    }
    calibration(raw, factor);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "tare_offset", raw);
    tare_pending = emit_device("calibration/set-tare", p);
    ui_status("port.platform.ScaleZeroed");
}
static void uid_hex(const sb_tag_t *tag, char uid[21]) {
    const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < tag->uid_len; i++) {
        uid[i * 2] = hex[tag->uid[i] >> 4];
        uid[i * 2 + 1] = hex[tag->uid[i] & 15];
    }
    uid[tag->uid_len * 2] = 0;
}
static void sensors(void *arg) {
    (void)arg;
    bool nfc_ok = false, scale_ok = false;
#if CONFIG_SB_NFC
    esp_err_t e = pn_init();
    nfc_ok = e == ESP_OK;
    ESP_LOGI("nfc", "PN5180: %s", esp_err_to_name(e));
#endif
#if CONFIG_SB_SCALE
    esp_err_t se = scale_init();
    scale_ok = se == ESP_OK;
    ESP_LOGI("scale", "NAU7802: %s", esp_err_to_name(se));
#endif
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.nfc = nfc_ok;
    state.scale = scale_ok;
    xSemaphoreGive(state_mutex);
    xEventGroupSetBits(wifi_events, BIT1);
    char current[21] = "";
    int misses = 0;
    int64_t next_scan = 0, last_weight = 0, last_valid = 0;
    float samples[10] = {0};
    size_t sample_count = 0, sample_pos = 0;
    float last_wake = 0;
    int32_t previous_tare = 0;
    double previous_factor = 0;
    write_t pending = {0};
    for (;;) {
        int64_t now = esp_timer_get_time();
        if (xQueueReceive(writes, &pending, 0) == pdTRUE && pending.length) {
            ui_status("port.platform.PlaceAnNtagStickerToWrite");
            ui_wake();
        }
        if (scale_ok) {
            int32_t raw;
            esp_err_t err = scale_read(&raw);
            if (err == ESP_OK) {
                last_valid = now;
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.raw = raw;
                state.have_raw = true;
                int32_t tare = state.tare;
                double factor = state.factor;
                xSemaphoreGive(state_mutex);
                float grams = sb_weight_from_adc(raw, tare, factor);
                if (tare != previous_tare || factor != previous_factor) {
                    sample_count = 0;
                    sample_pos = 0;
                    previous_tare = tare;
                    previous_factor = factor;
                }
                samples[sample_pos++ % 10] = grams;
                if (sample_count < 10)
                    sample_count++;
                float lo = samples[0], hi = lo;
                for (size_t i = 1; i < sample_count; i++) {
                    lo = fminf(lo, samples[i]);
                    hi = fmaxf(hi, samples[i]);
                }
                bool stable = sample_count == 10 && hi - lo < 2;
                bool calibrated = factor != 1.0;
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.stable = stable;
                state.scale = true;
                xSemaphoreGive(state_mutex);
                ui_raw(raw, true);
                ui_weight(grams, stable, calibrated);
                if (stable && fabsf(grams - last_wake) > 50) {
                    ui_wake();
                    last_wake = grams;
                }
                if (now - last_weight >= 1000000 && !ui_manual_active()) {
                    cJSON *p = api_payload();
                    cJSON_AddNumberToObject(p, "weight_grams", grams);
                    cJSON_AddBoolToObject(p, "stable", stable);
                    cJSON_AddNumberToObject(p, "raw_adc", raw);
                    publish_weight(p);
                    last_weight = now;
                }
            } else if (err != ESP_ERR_NOT_FOUND || (last_valid && now - last_valid > 3000000)) {
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.scale = false;
                state.have_raw = false;
                xSemaphoreGive(state_mutex);
                ui_raw(0, false);
                ui_weight(0, false, false);
            }
        }
        if (nfc_ok && now >= next_scan) {
            sb_tag_t tag;
            esp_err_t err = pn_select(&tag);
            next_scan = now + 300000;
            if (err == ESP_OK) {
                misses = 0;
                char uid[21];
                uid_hex(&tag, uid);
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                snprintf(state.uid, sizeof(state.uid), "%s", uid);
                xSemaphoreGive(state_mutex);
                if (strcmp(uid, current)) {
                    if (current[0]) {
                        cJSON *p = api_payload();
                        cJSON_AddStringToObject(p, "tag_uid", current);
                        emit("/nfc/tag-removed", p);
                    }
                    snprintf(current, sizeof(current), "%s", uid);
                    char tray[33];
                    esp_err_t read = pn_read_tray(&tag, tray);
                    cJSON *p = api_payload();
                    cJSON_AddStringToObject(p, "tag_uid", uid);
                    cJSON_AddNumberToObject(p, "sak", tag.sak);
                    cJSON_AddStringToObject(p, "tag_type",
                                            (tag.sak == 8 || tag.sak == 0x18) ? "mifare_classic"
                                            : tag.sak == 0                    ? "ntag"
                                                                              : "unknown");
                    if (read == ESP_OK)
                        cJSON_AddStringToObject(p, "tray_uuid", tray);
                    emit("/nfc/tag-scanned", p);
                    ui_tag(uid, tag.sak == 8 ? "Bambu / MIFARE Classic" : "NFC sticker detected");
                    if (read == ESP_OK)
                        ui_tag_uuid(uid, tray);
                    ui_wake();
                }
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                int64_t write_until = state.write_until;
                xSemaphoreGive(state_mutex);
                if (pending.length && now < write_until &&
                    (xEventGroupGetBits(wifi_events) & BIT0)) {
                    // Reading a Classic tag can leave Crypto1 enabled. Always reselect
                    // for a write.
                    sb_tag_t selected;
                    esp_err_t result = pn_select(&selected);
                    if (result == ESP_OK && (selected.uid_len != tag.uid_len ||
                                             memcmp(selected.uid, tag.uid, tag.uid_len)))
                        result = ESP_ERR_INVALID_RESPONSE;
                    if (result == ESP_OK)
                        result = pn_write_ndef(&selected, pending.data, pending.length);
                    cJSON *p = api_payload();
                    cJSON_AddNumberToObject(p, "spool_id", pending.spool);
                    cJSON_AddStringToObject(p, "tag_uid", uid);
                    cJSON_AddBoolToObject(p, "success", result == ESP_OK);
                    cJSON_AddStringToObject(p, "message",
                                            result == ESP_OK ? "Written and readback verified"
                                                             : esp_err_to_name(result));
                    emit("/nfc/write-result", p);
                    ui_status(result == ESP_OK ? "port.platform.TagWriteVerified" : "port.platform.TagWriteFailed");
                    pending.length = 0;
                }
            } else if (++misses >= 3 && current[0]) {
                cJSON *p = api_payload();
                cJSON_AddStringToObject(p, "tag_uid", current);
                emit("/nfc/tag-removed", p);
                current[0] = 0;
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.uid[0] = 0;
                xSemaphoreGive(state_mutex);
                ui_tag("", "Ready for the next spool");
            }
            if (err == ESP_ERR_TIMEOUT) {
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.nfc = false;
                xSemaphoreGive(state_mutex);
            } else if (err == ESP_OK) {
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                state.nfc = true;
                xSemaphoreGive(state_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        if(!wifi_reconfiguring)esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW("network", "Station disconnected, reason=%u", event ? event->reason : 0);
        xEventGroupClearBits(wifi_events, BIT0);
        if(!wifi_reconfiguring)esp_wifi_connect();
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(wifi_events, BIT0);
}
static void wifi_start(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    esp_sntp_config_t clock_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&clock_cfg));
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, sb_settings.ssid, strlen(sb_settings.ssid));
    memcpy(cfg.sta.password, sb_settings.password, strlen(sb_settings.password));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Keep the hosted radio awake for this mains-powered interactive display. */
    esp_err_t power_save = esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI("network", "Wi-Fi modem power saving disabled: %s", esp_err_to_name(power_save));
}
static int hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static void command_result(const char *cmd, bool success, const char *message) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "command", cmd);
    cJSON_AddBoolToObject(p, "success", success);
    cJSON_AddStringToObject(p, "message", message);
    emit_device("system/command-result", p);
}
static void response(cJSON *r) {
    cJSON *t = cJSON_GetObjectItemCaseSensitive(r, "tare_offset"),
          *f = cJSON_GetObjectItemCaseSensitive(r, "calibration_factor");
    if (cJSON_IsNumber(t) && cJSON_IsNumber(f) && !tare_pending)
        calibration(t->valueint, f->valuedouble);
    cJSON *b = cJSON_GetObjectItemCaseSensitive(r, "display_brightness"),
          *s = cJSON_GetObjectItemCaseSensitive(r, "display_blank_timeout");
    if (cJSON_IsNumber(b) && cJSON_IsNumber(s))
        ui_display_settings(b->valueint, s->valueint);
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(r, "pending_command");
    if (!cJSON_IsString(cmd) || strcmp(cmd->valuestring, "write_tag")) {
        issued_write.length = 0;
        write_t cancel = {0};
        xQueueOverwrite(writes, &cancel);
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        state.write_until = 0;
        xSemaphoreGive(state_mutex);
    }
    if (!cJSON_IsString(cmd))
        return;
    if (!strcmp(cmd->valuestring, "run_nfc_diag") || !strcmp(cmd->valuestring, "run_scale_diag") ||
        !strcmp(cmd->valuestring, "run_read_tag_diag")) {
        bool scale_diag = !strcmp(cmd->valuestring, "run_scale_diag");
        bool tag_diag = !strcmp(cmd->valuestring, "run_read_tag_diag");
        char output[240];
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        bool success = scale_diag ? state.scale : tag_diag ? state.uid[0] != 0 : state.nfc;
        snprintf(output, sizeof(output),
                 "ESP32 driver snapshot\nPN5180 ready: %s\nTag UID: %s\n"
                 "NAU7802 ready: %s\nRaw ADC: %ld\nTare: %ld\nFactor: %.6g\n"
                 "Snapshot only; no exhaustive RF or wiring test.",
                 state.nfc ? "yes" : "no", state.uid[0] ? state.uid : "none",
                 state.scale ? "yes" : "no", (long)state.raw, (long)state.tare, state.factor);
        xSemaphoreGive(state_mutex);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "diagnostic",
                                scale_diag ? "scale"
                                : tag_diag ? "read_tag"
                                           : "nfc");
        cJSON_AddBoolToObject(p, "success", success);
        cJSON_AddStringToObject(p, "output", output);
        cJSON_AddNumberToObject(p, "exit_code", success ? 0 : 1);
        char path[96];
        snprintf(path, sizeof(path), "/diagnostics/%s/result", api_device_id());
        emit(path, p);
        return;
    }
    if (!strcmp(cmd->valuestring, "tare")) {
        do_tare();
        return;
    }
    if (!strcmp(cmd->valuestring, "write_tag")) {
        cJSON *payload = cJSON_GetObjectItemCaseSensitive(r, "pending_write_payload");
        cJSON *hex = cJSON_GetObjectItemCaseSensitive(payload, "ndef_data_hex"),
              *spool = cJSON_GetObjectItemCaseSensitive(payload, "spool_id");
        write_t w = {0};
        bool valid = cJSON_IsString(hex) && cJSON_IsNumber(spool) && spool->valueint > 0;
        size_t n = valid ? strlen(hex->valuestring) : 0;
        valid = valid && n > 0 && !(n % 2) && n <= sizeof(w.data) * 2;
        for (size_t i = 0; valid && i < n / 2; i++) {
            int a = hex_digit(hex->valuestring[2 * i]), b = hex_digit(hex->valuestring[2 * i + 1]);
            if (a < 0 || b < 0)
                valid = false;
            else
                w.data[i] = a * 16 + b;
        }
        if (valid) {
            w.length = n / 2;
            w.spool = spool->valueint;
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            state.write_until = esp_timer_get_time() + 10000000;
            xSemaphoreGive(state_mutex);
            if (w.spool != issued_write.spool || w.length != issued_write.length ||
                memcmp(w.data, issued_write.data, w.length)) {
                issued_write = w;
                xQueueOverwrite(writes, &w);
            }
        } else
            command_result("write_tag", false, "Invalid NDEF payload");
    } else if (!strcmp(cmd->valuestring, "reboot") || !strcmp(cmd->valuestring, "restart_daemon")) {
        char path[96];
        api_device_path(path, sizeof(path), "system/command-result");
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "command", cmd->valuestring);
        cJSON_AddBoolToObject(p, "success", true);
        cJSON_AddStringToObject(p, "message", "Restarting device");
        cJSON *ack = api_post(path, p);
        cJSON_Delete(ack);
        cJSON_Delete(p);
        esp_restart();
    } else
        command_result(cmd->valuestring, false,
                       "This Pi-specific command is unavailable on this device");
}
void app_main(void) {
    sb_memory_init();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("spoolbuddy", NVS_READWRITE, &storage));
    snprintf(sb_settings.ssid, sizeof(sb_settings.ssid), "%s", CONFIG_SB_WIFI_SSID);
    snprintf(sb_settings.password, sizeof(sb_settings.password), "%s", CONFIG_SB_WIFI_PASSWORD);
    snprintf(sb_settings.backend_url, sizeof(sb_settings.backend_url), "%s", CONFIG_SB_BACKEND_URL);
    snprintf(sb_settings.api_key, sizeof(sb_settings.api_key), "%s", CONFIG_SB_API_KEY);
    bool config_error = false;
    esp_err_t card = sb_board_sd_mount();
    char settings_error[160];
    bool loaded=sb_settings_store_load(card==ESP_OK,settings_error,sizeof(settings_error));
    config_error=!loaded && !sb_settings.ssid[0];
    if(settings_error[0])ESP_LOGW("settings","%s",settings_error);
    if(card!=ESP_OK)ESP_LOGW("settings","TF unavailable; onboard configuration selected when present");
    setenv("TZ", sb_settings.timezone, 1);
    tzset();
    state.factor = 1.0;
    nvs_get_i32(storage, "tare", &state.tare);
    size_t n = sizeof(state.factor);
    nvs_get_blob(storage, "factor", &state.factor, &n);
    if (!isfinite(state.factor) || fabs(state.factor) < 1e-9)
        state.factor = 1.0;
    outgoing = xQueueCreate(40, sizeof(event_t));
    weights = xQueueCreate(1, sizeof(event_t));
    writes = xQueueCreate(1, sizeof(write_t));
    state_mutex = xSemaphoreCreateMutex();
    wifi_events = xEventGroupCreate();
    assert(outgoing && weights && writes && state_mutex && wifi_events);
    api_init();
    ui_init();
    sb_update_init();
    bridge_init(api_device_id());
    ui_status("port.platform.ConnectingWiFi");
    BaseType_t started = xTaskCreate(sensors, "sensors", 8192, NULL, 4, NULL);
    assert(started == pdPASS);
    bool configured = strlen(sb_settings.ssid) > 0;
    if (!configured)
        ui_status(config_error ? "port.platform.SdConfigurationInvalid"
                               : "port.platform.SetWiFiAndBambuddyInMenuconfig");
    else
        wifi_start();
    bool registered = false;
    int64_t next_heartbeat = 0, next_retry = 0;
    int64_t config_deadline=esp_timer_get_time()+120000000;
    event_t event = {0};
    for (;;) {
        sb_optional_inputs_poll(esp_timer_get_time(), publish_override, &event);
        if (ui_take_tare())
            do_tare();
        bool online = (xEventGroupGetBits(wifi_events) & BIT0) != 0;
        int64_t now = esp_timer_get_time();
        if(now>config_deadline && sb_settings_store_rollback()) {
            sb_realtime_poll(false,api_request);
            registered=false;
            wifi_reconfiguring=true;
            xEventGroupClearBits(wifi_events,BIT0);
            esp_wifi_stop();
            wifi_config_t config={0};
            memcpy(config.sta.ssid,sb_settings.ssid,strlen(sb_settings.ssid));
            memcpy(config.sta.password,sb_settings.password,strlen(sb_settings.password));
            esp_wifi_set_config(WIFI_IF_STA,&config);
            setenv("TZ",sb_settings.timezone,1);tzset();ui_language(sb_settings.chinese);
            wifi_reconfiguring=false;esp_wifi_start();
            online=false;next_retry=0;
        }
        ui_network(online);
        bridge_online(online && registered);
        sb_realtime_poll(online && registered,api_request);
        if (!online) {
            if (configured)
                ui_status("port.platform.WiFiDisconnectedReconnecting");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (now < next_retry) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        bool nfc = state.nfc, scale = state.scale;
        int32_t tare = state.tare;
        double factor = state.factor;
        xSemaphoreGive(state_mutex);
        if (!(xEventGroupGetBits(wifi_events) & BIT1)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!registered) {
            cJSON *r = api_register(nfc, scale, tare, factor);
            if (r) {
                registered = true;
                if(sb_settings_store_confirm())sb_update_confirm_boot();
                response(r);
                cJSON_Delete(r);
                ui_status("spoolbuddy.status.systemReady");
            } else {
                ui_status("port.platform.BambuddyUnavailableRetrying");
                next_retry = now + 5000000;
            }
            continue;
        }
        if (now >= next_heartbeat) {
            cJSON *r = api_heartbeat(nfc, scale);
            next_heartbeat = now + 5000000;
            if (r) {
                if(sb_settings_store_confirm())sb_update_confirm_boot();
                response(r);
                cJSON_Delete(r);
                ui_status("spoolbuddy.status.systemReady");
            } else {
                ui_status("port.platform.BackendDisconnectedRetrying");
                registered = false;
                bridge_online(false);
                next_retry = now + 3000000;
                continue;
            }
        }
        bridge_poll(api_request, false);
        if (!event.body && xQueueReceive(outgoing, &event, 0) != pdTRUE)
            xQueueReceive(weights, &event, 0);
        if (event.body) {
            cJSON *r = api_post(event.path, event.body);
            if (r) {
                if (!strcmp(event.path, "/nfc/tag-scanned")) {
                    cJSON *id = cJSON_GetObjectItemCaseSensitive(r, "spool_id");
                    cJSON *uid = cJSON_GetObjectItemCaseSensitive(event.body, "tag_uid");
                    if (cJSON_IsNumber(id) && cJSON_IsString(uid)) {
                        char path[96];
                        snprintf(path, sizeof(path), "%s/%d", bridge_inventory(), id->valueint);
                        cJSON *spool = api_get(path);
                        cJSON *material = cJSON_GetObjectItemCaseSensitive(spool, "material"),
                              *brand = cJSON_GetObjectItemCaseSensitive(spool, "brand"),
                              *label_weight =
                                  cJSON_GetObjectItemCaseSensitive(spool, "label_weight"),
                              *used = cJSON_GetObjectItemCaseSensitive(spool, "weight_used");
                        char text[240];
                        double remaining =
                            cJSON_IsNumber(label_weight) && cJSON_IsNumber(used)
                                ? fmax(0, label_weight->valuedouble - used->valuedouble)
                                : 0;
                        snprintf(text, sizeof(text), "#%d  %s %s\nRemaining: %.1f g", id->valueint,
                                 cJSON_IsString(brand) ? brand->valuestring : "",
                                 cJSON_IsString(material) ? material->valuestring : "", remaining);
                        if (!cJSON_IsObject(spool))
                            snprintf(text, sizeof(text), "Spool details unavailable");
                        cJSON *core = cJSON_GetObjectItemCaseSensitive(spool, "core_weight");
                        if (cJSON_IsObject(spool))
                            ui_spool(uid->valuestring, id->valueint,
                                     cJSON_IsString(brand) ? brand->valuestring : "",
                                     cJSON_IsString(material) ? material->valuestring : "",
                                     remaining, cJSON_IsNumber(core) ? core->valuedouble : 250);
                        else
                            ui_details(uid->valuestring, text);
                        cJSON_Delete(spool);
                    } else if (cJSON_IsString(uid))
                        ui_details(uid->valuestring, "Unknown spool: link it in Bambuddy");
                }
                if (strstr(event.path, "calibration/set-tare"))
                    tare_pending = false;
                if (!strcmp(event.path, "/scale/update-spool-weight"))
                    ui_status("port.platform.WeightSavedToInventory");
                cJSON_Delete(r);
                cJSON_Delete(event.body);
                event.body = NULL;
            } else {
                int status = api_last_status();
                if (status >= 400 && status < 500 && status != 408 && status != 429) {
                    ESP_LOGE("events", "Rejected %s: HTTP %d", event.path, status);
                    ui_status("port.platform.ApiRejectedEventCheckKeyPermissions");
                    if (strstr(event.path, "calibration/set-tare"))
                        tare_pending = false;
                    cJSON_Delete(event.body);
                    event.body = NULL;
                    if (status == 404)
                        registered = false;
                } else
                    next_retry = now + 3000000;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
