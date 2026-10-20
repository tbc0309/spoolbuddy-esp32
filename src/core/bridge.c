// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared application transport and bounded command queue. Never retry a write implicitly.
#include "bridge.h"
#include "ui_platform.h"
#include "ui_original.h"
#include "ui.h"
#include "settings.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
typedef struct {
    char method[8], path[1536], model_key[32];
    cJSON *body;
} command_t;
static command_t commands[8];
static unsigned head, count;
static cJSON *models, *lookup_targets;
static char device[51];
static bool online, refresh = true, spoolman;
static int printer;
static uint32_t next_poll, diagnostic_until, diagnostic_next;
static char diagnostic_path[192];
static int catalog_stage = -1, catalog_printer, catalog_diameter;
static bool catalogs_requested;
static bool event_refresh;
static uint32_t event_next;
void bridge_event(const cJSON *event) {
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(event, "type");
    if (!cJSON_IsString(type))
        return;
    sb_display_lock();
    const char *name = type->valuestring;
    const cJSON *device_id = cJSON_GetObjectItemCaseSensitive(event, "device_id");
    if (!strncmp(name, "spoolbuddy_", 10) &&
        (!cJSON_IsString(device_id) || strcmp(device_id->valuestring, device))) {
        sb_display_unlock();
        return;
    }
    if (!strcmp(name, "printer_status")) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(event, "printer_id");
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(event, "data");
        if (cJSON_IsNumber(id) && cJSON_IsObject(data)) {
            char key[32];
            snprintf(key, sizeof(key), "printer_%d", id->valueint);
            cJSON_DeleteItemFromObjectCaseSensitive(models, key);
            cJSON_AddItemToObject(models, key, cJSON_Duplicate(data, true));
            if (id->valueint == printer) {
                cJSON_DeleteItemFromObjectCaseSensitive(models, "printer_status");
                cJSON_AddItemToObject(models, "printer_status", cJSON_Duplicate(data, true));
            }
            original_model_changed();
        }
    } else if (!strcmp(name, "spoolbuddy_tag_written") ||
               !strcmp(name, "spoolbuddy_tag_write_failed") ||
               !strcmp(name, "spoolbuddy_tag_link_failed")) {
        original_feedback(!strcmp(name, "spoolbuddy_tag_written"),
                          !strcmp(name, "spoolbuddy_tag_written") ? 200 : 409, event);
        event_refresh = true;
    } else if (strstr(name, "spool") || strstr(name, "inventory") || strstr(name, "assignment") ||
               strstr(name, "ams")) {
        /* Scale samples do not invalidate the entire inventory on every ADC tick. */
        if (strcmp(name, "spoolbuddy_weight"))
            event_refresh = true;
    }
    sb_display_unlock();
}
void bridge_load_catalogs(void) {
    sb_display_lock();
    if (!catalogs_requested) {
        catalogs_requested = true;
        catalog_stage = catalog_printer = catalog_diameter = 0;
    }
    sb_display_unlock();
}
bool bridge_catalogs_loading(void) {
    sb_display_lock();
    bool loading = catalog_stage >= 0;
    sb_display_unlock();
    return loading;
}
void bridge_reload_catalogs(void) {
    sb_display_lock();
    catalogs_requested = false;
    bridge_load_catalogs();
    sb_display_unlock();
}
void bridge_init(const char *id) {
    sb_display_lock();
    for (unsigned i = 0; i < count; i++)
        cJSON_Delete(commands[(head + i) % 8].body);
    count = head = 0;
    cJSON_Delete(models);
    models = cJSON_CreateObject();
    cJSON_Delete(lookup_targets);
    lookup_targets = cJSON_CreateObject();
    snprintf(device, sizeof(device), "%s", id ? id : "");
    online = false;
    printer = 0;
    spoolman = false;
    refresh = true;
    next_poll = diagnostic_until = diagnostic_next = 0;
    diagnostic_path[0] = 0;
    catalog_stage = -1;
    catalogs_requested = false;
    event_refresh = false;
    event_next = 0;
    sb_display_unlock();
}
const cJSON *bridge_model(const char *key) { return cJSON_GetObjectItemCaseSensitive(models, key); }
const char *bridge_device(void) { return device; }
const char *bridge_inventory(void) {
    return spoolman ? "/spoolman/inventory/spools" : "/inventory/spools";
}
bool bridge_connected(void) { return online; }
void bridge_online(bool value) {
    sb_display_lock();
    if (!value && online) {
        for (unsigned i = 0; i < count; i++)
            cJSON_Delete(commands[(head + i) % 8].body);
        count = head = 0;
    }
    bool changed = online != value;
    online = value;
    if (changed)
        refresh = true;
    sb_display_unlock();
    if (changed)
        original_online(value);
}
void bridge_printer(int id) {
    sb_display_lock();
    if (printer != id)
        cJSON_DeleteItemFromObjectCaseSensitive(models, "printer_status");
    printer = id;
    refresh = true;
    sb_display_unlock();
}
int bridge_printer_id(void) { return printer; }
void bridge_refresh(void) {
    sb_display_lock();
    refresh = true;
    sb_display_unlock();
}
bool bridge_submit(const char *method, const char *path, const cJSON *body) {
    if (!method || !path || strlen(method) >= 8 || strlen(path) >= 1536 || path[0] != '/')
        return false;
    sb_display_lock();
    if (!online || count == 8) {
        original_feedback(false, online ? 429 : 0, NULL);
        sb_display_unlock();
        return false;
    }
    command_t *q = &commands[(head + count) % 8];
    memset(q, 0, sizeof(*q));
    q->body = body ? cJSON_Duplicate(body, true) : NULL;
    if (body && !q->body) {
        sb_display_unlock();
        return false;
    }
    snprintf(q->method, sizeof(q->method), "%s", method);
    snprintf(q->path, sizeof(q->path), "%s", path);
    count++;
    sb_display_unlock();
    return true;
}
bool bridge_lookup(const char *key, const char *path) {
    if (!key || !key[0] || strlen(key) >= 32 || !path || path[0] != '/' || strlen(path) >= 1536)
        return false;
    sb_display_lock();
    if (!online) {
        sb_display_unlock();
        return false;
    }
    /* New searches replace pending reads for the same model. */
    for (unsigned i = 0; i < count; i++) {
        command_t *pending = &commands[(head + i) % 8];
        if (!strcmp(pending->model_key, key)) {
            snprintf(pending->path, sizeof(pending->path), "%s", path);
            cJSON_DeleteItemFromObjectCaseSensitive(lookup_targets, key);
            cJSON_AddStringToObject(lookup_targets, key, path);
            cJSON_DeleteItemFromObjectCaseSensitive(models, key);
            sb_display_unlock();
            return true;
        }
    }
    if (count == 8) {
        sb_display_unlock();
        return false;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(lookup_targets, key);
    cJSON_AddStringToObject(lookup_targets, key, path);
    command_t *q = &commands[(head + count) % 8];
    memset(q, 0, sizeof(*q));
    snprintf(q->method, sizeof(q->method), "GET");
    snprintf(q->path, sizeof(q->path), "%s", path);
    snprintf(q->model_key, sizeof(q->model_key), "%s", key);
    count++;
    cJSON_DeleteItemFromObjectCaseSensitive(models, key);
    sb_display_unlock();
    return true;
}
static bool model(sb_transport_fn send, const char *key, const char *path) {
    int status = 0;
    cJSON *r = send("GET", path, NULL, &status);
    if (!r || status < 200 || status >= 300) {
        bool fatal =
            status == 0 || status == 401 || status == 403 || (status >= 200 && status < 300);
        cJSON_Delete(r);
        if (fatal)
            bridge_online(false);
        return !fatal;
    }
    sb_display_lock();
    if (!cJSON_Compare(bridge_model(key), r, true)) {
        cJSON_DeleteItemFromObjectCaseSensitive(models, key);
        cJSON_AddItemToObject(models, key, r);
        r = NULL;
        original_model_changed();
    }
    sb_display_unlock();
    cJSON_Delete(r);
    return true;
}
/* Optional services can be unauthenticated/offline without disconnecting the device.
 * One request per worker iteration also respects H2 calibration serialization. */
static void catalog_read(sb_transport_fn send, const char *key, const char *path) {
    int code = 0;
    cJSON *r = send("GET", path, NULL, &code);
    if (!r || code < 200 || code >= 300) {
        cJSON_Delete(r);
        r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "http_status", code);
    }
    sb_display_lock();
    cJSON_DeleteItemFromObjectCaseSensitive(models, key);
    cJSON_AddItemToObject(models, key, r);
    original_model_changed();
    sb_display_unlock();
}
static void catalog_tick(sb_transport_fn send) {
    sb_display_lock();
    if (catalog_stage < 0) {
        sb_display_unlock();
        return;
    }
    static const char *keys[] = {"cloud_auth",    "cloud_presets", "orca_auth",
                                 "orca_presets",  "local_presets", "builtin_presets",
                                 "spool_catalog", "locations"};
    static const char *paths[] = {"/cloud/status",      "/cloud/filaments",
                                  "/orca-cloud/status", "/orca-cloud/profiles",
                                  "/local-presets/",    "/cloud/builtin-filaments",
                                  "/inventory/catalog", "/inventory/locations"};
    _Static_assert(sizeof(keys) / sizeof(keys[0]) == sizeof(paths) / sizeof(paths[0]),
                   "catalog paths must match keys");
    if (catalog_stage < (int)(sizeof(keys) / sizeof(keys[0]))) {
        int stage = catalog_stage++;
        bool skip = (stage == 1 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
                                       bridge_model("cloud_auth"), "is_authenticated"))) ||
                    (stage == 3 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
                                       bridge_model("orca_auth"), "connected")));
        sb_display_unlock();
        if (!skip)
            catalog_read(send, keys[stage], paths[stage]);
        return;
    }
    const cJSON *p = cJSON_GetArrayItem(bridge_model("printers"), catalog_printer);
    if (!p) {
        catalog_stage = -1;
        original_model_changed();
        sb_display_unlock();
        return;
    }
    const cJSON *pid = cJSON_GetObjectItemCaseSensitive(p, "id");
    if (!cJSON_IsNumber(pid)) {
        catalog_printer++;
        sb_display_unlock();
        return;
    }
    int id = pid->valueint;
    char key[32], path[160];
    snprintf(key, sizeof(key), "pa_status_%d", id);
    if (catalog_diameter == 0) {
        snprintf(path, sizeof(path), "/printers/%d/status", id);
        catalog_diameter++;
    } else {
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(bridge_model(key), "connected"))) {
            catalog_printer++;
            catalog_diameter = 0;
            sb_display_unlock();
            return;
        }
        static const char *diameters[] = {"0.2", "0.4", "0.6", "0.8"};
        int d = catalog_diameter++ - 1;
        snprintf(key, sizeof(key), "pa_%d_%d", id, d);
        snprintf(path, sizeof(path), "/printers/%d/kprofiles/?nozzle_diameter=%s", id,
                 diameters[d]);
        if (catalog_diameter > 4) {
            catalog_printer++;
            catalog_diameter = 0;
        }
    }
    sb_display_unlock();
    catalog_read(send, key, path);
}

void bridge_poll(sb_transport_fn send, bool force) {
    command_t q = {0};
    sb_display_lock();
    if (!online) {
        sb_display_unlock();
        return;
    }
    if (count) {
        q = commands[head];
        memset(&commands[head], 0, sizeof(q));
        head = (head + 1) % 8;
        count--;
    }
    bool pushed = event_refresh && (int32_t)(lv_tick_get() - event_next) >= 0;
    bool due = force || refresh || pushed || (int32_t)(lv_tick_get() - next_poll) >= 0;
    if (due) {
        event_refresh = false;
        event_next = lv_tick_get() + 500;
        refresh = false;
        next_poll = lv_tick_get() + 10000;
    }
    sb_display_unlock();
    if (q.method[0]) {
        int status = 0;
        cJSON *r = send(q.method, q.path, q.body, &status);
        sb_display_lock();
        const cJSON *target =
            q.model_key[0] ? cJSON_GetObjectItemCaseSensitive(lookup_targets, q.model_key) : NULL;
        bool current_lookup =
            online && cJSON_IsString(target) && !strcmp(target->valuestring, q.path);
        if (q.model_key[0] && !current_lookup) {
            /* A changed query or disconnect invalidates an in-flight catalog response. */
        } else if (q.model_key[0]) {
            cJSON_DeleteItemFromObjectCaseSensitive(models, q.model_key);
            if (r && status >= 200 && status < 300) {
                cJSON_AddItemToObject(models, q.model_key, r);
                r = NULL;
            } else {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddNumberToObject(error, "http_status", status);
                cJSON_AddStringToObject(error, "error",
                                        status ? "Catalog unavailable" : "Connection failed");
                cJSON_AddItemToObject(models, q.model_key, error);
            }
            original_model_changed();
        } else {
            original_command_result(q.method, q.path, status, r);
            original_feedback(status >= 200 && status < 300, status, r);
        }
        sb_display_unlock();
        if (status >= 200 && status < 300 && !strcmp(q.method, "POST") &&
            strstr(q.path, "/diagnostics/") && strstr(q.path, "/run?diagnostic=")) {
            const char *query = strstr(q.path, "/run?diagnostic=");
            size_t prefix = (size_t)(query - q.path);
            snprintf(diagnostic_path, sizeof(diagnostic_path), "%.*s/result%s", (int)prefix, q.path,
                     query + 4);
            diagnostic_until = lv_tick_get() + 60000;
            diagnostic_next = 0;
            sb_display_lock();
            cJSON_DeleteItemFromObjectCaseSensitive(models, "diagnostic");
            sb_display_unlock();
        }
        cJSON_Delete(r);
        cJSON_Delete(q.body);
        due = true;
        // A lost response does not mean the server did not commit. Do not replay the command.
    }
    if (diagnostic_path[0] && (int32_t)(diagnostic_until - lv_tick_get()) > 0 &&
        (int32_t)(lv_tick_get() - diagnostic_next) >= 0) {
        diagnostic_next = lv_tick_get() + 1000;
        if (!model(send, "diagnostic", diagnostic_path))
            return;
        sb_display_lock();
        if (bridge_model("diagnostic"))
            diagnostic_path[0] = 0;
        sb_display_unlock();
    }
    if (!due) {
        if (!q.method[0])
            catalog_tick(send);
        return;
    }
    if (!model(send, "spoolman", "/settings/spoolman"))
        return;
    sb_display_lock();
    const cJSON *sm = bridge_model("spoolman");
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(sm, "spoolman_enabled");
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(sm, "spoolman_url");
    spoolman = (cJSON_IsTrue(enabled) ||
                (cJSON_IsString(enabled) && !strcmp(enabled->valuestring, "true"))) &&
               cJSON_IsString(url) && url->valuestring[0];
    char inventory[192];
    snprintf(inventory, sizeof(inventory), "%s?include_archived=false", bridge_inventory());
    sb_display_unlock();
    if (!model(send, "inventory", inventory))
        return;
    if (!model(send, "printers", "/printers/"))
        return;
    if (!model(send, "devices", "/spoolbuddy/devices"))
        return;
    sb_display_lock();
    const cJSON *ps = bridge_model("printers");
    if (!printer && cJSON_GetArraySize(ps)) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(ps, 0), "id");
        if (cJSON_IsNumber(id))
            printer = id->valueint;
    }
    int chosen = printer;
    sb_display_unlock();
    if (chosen) {
        char path[96];
        snprintf(path, sizeof(path), "/printers/%d/status", chosen);
        if (!model(send, "printer_status", path))
            return;
    }
    if (!model(send, "assignments",
               spoolman ? "/spoolman/inventory/slot-assignments/all" : "/inventory/assignments"))
        return;
    if (spoolman && !model(send, "linked_spools", "/spoolman/spools/linked"))
        return;
    const cJSON *item;
    cJSON_ArrayForEach(item, bridge_model("printers")) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!cJSON_IsNumber(id))
            continue;
        char key[32], path[96];
        snprintf(key, sizeof(key), "printer_%d", id->valueint);
        snprintf(path, sizeof(path), "/printers/%d/status", id->valueint);
        if (!model(send, key, path))
            return;
    }
    /* Language is optional: limited settings permissions must not disconnect inventory. */
    catalog_read(send, "app_settings", "/settings/");
    sb_display_lock();
    const cJSON *language = cJSON_GetObjectItemCaseSensitive(bridge_model("app_settings"), "language");
    if (!sb_settings.language_override && cJSON_IsString(language) && language->valuestring[0]) {
        bool zh = sb_language_is_chinese(language->valuestring);
        if (sb_settings.chinese != zh) ui_language(zh);
    }
    sb_display_unlock();
    if (!q.method[0])
        catalog_tick(send);
}
