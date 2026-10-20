// SPDX-License-Identifier: AGPL-3.0-or-later
// Runs the production LVGL callbacks and bridge against an isolated transport.
#include "bridge.h"
#include "ui_original.h"
#include "ui_live.h"
#include "ui.h"
#include "runtime.h"
#include "weight_math.h"
#include "optional_inputs.h"
#include "settings.h"
#include "system_stats.h"
#include "update.h"
#include "lvgl.h"
#include "../boards/esp32-p4-wifi6-touch-lcd-4.3/rev3/sb_board/display_blanking.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
extern bool original_test_click(const char *caption);
extern bool original_test_input(const char *key, const char *value);
extern bool original_test_dropdown(const char *key, int index);
extern void original_test_detail(int id);
extern void original_test_writer(int id);
extern void original_test_close(void);
extern int original_test_cards(bool write);
extern void original_test_colors(void);
extern void original_test_quick(void);
extern void original_test_slot(int code);
extern void original_test_quickadd(bool active);
extern bool sb_sim_capture(const char *path);
extern bool original_test_network(bool connected);
extern void original_test_refresh_clock(void);
extern int original_test_update_height(void);
extern void sb_sim_update_state(const sb_update_state *fixture);
extern bool original_test_text(const char *caption);
extern void original_test_settings_scroll(int y);
static FILE *report;
static int failures, checks, writes, reads, reply_status = 200, read_status = 200;
static bool spoolman, expanded_inventory, supersede_lookup, catalog_fixture;
static int calibration_reads;
static bool inventory_assigned;
static bool system_fixture;
static const char *language_fixture;
static const char *time_format_fixture;
static char lookup_path[512];
static char last_method[8], last_path[1536];
static cJSON *last_body;
static void check(bool ok, const char *message) {
    checks++;
    if (!ok)
        failures++;
    fprintf(report, "%s %s\n", ok ? "PASS" : "FAIL", message);
    fflush(report);
}
static void click(const char *caption) { check(original_test_click(caption), caption); }
static cJSON *transport(const char *method, const char *path, const cJSON *body, int *status) {
    if (strcmp(method, "GET")) {
        writes++;
        if (reply_status >= 200 && reply_status < 300) {
            if (!strcmp(method, "POST") && (!strcmp(path, "/inventory/assignments") ||
                                            !strcmp(path, "/spoolman/inventory/slot-assignments")))
                inventory_assigned = true;
            if (!strcmp(method, "DELETE") && (strstr(path, "/inventory/assignments/") ||
                                              strstr(path, "/inventory/slot-assignments/")))
                inventory_assigned = false;
        }
        snprintf(last_method, sizeof(last_method), "%s", method);
        snprintf(last_path, sizeof(last_path), "%s", path);
        cJSON_Delete(last_body);
        last_body = cJSON_Duplicate(body, true);
        *status = reply_status;
        if (catalog_fixture && !strcmp(method, "POST") && strstr(path, "/spools/bulk"))
            return cJSON_Parse(
                strstr(path, "spoolman")
                    ? "{\"created\":[{\"id\":42}],\"errors\":[{\"index\":1},{\"index\":2}]}"
                    : "[{\"id\":42},{\"id\":43},{\"id\":44}]");
        if (catalog_fixture && !strcmp(method, "POST") && !strcmp(path, "/inventory/spools"))
            return cJSON_Parse("{\"id\":42}");
        return reply_status ? cJSON_Parse("{\"detail\":\"isolated test response\"}") : NULL;
    }
    reads++;
    *status = read_status;
    if (read_status != 200)
        return NULL;
    if (!strcmp(path,"/settings/")) {
        cJSON *settings=cJSON_CreateObject();
        if(language_fixture) cJSON_AddStringToObject(settings,"language",language_fixture);
        if(time_format_fixture) cJSON_AddStringToObject(settings,"time_format",time_format_fixture);
        return settings;
    }
    if(system_fixture && !strcmp(path,"/spoolbuddy/devices")) {
        cJSON *devices=cJSON_CreateArray(),*device=cJSON_CreateObject();
        cJSON_AddStringToObject(device,"device_id",bridge_device());
        sb_system_snapshot_t hardware={.idf_version="v6.1",.arch="riscv32",.cores=2,
            .heap_total=32*1048576,.heap_free=8*1048576,.uptime_us=90061000000ULL,
            .temperature_valid=true,.temperature_c=42.5,.sd_available=true,
            .sd_total=4ULL*1073741824,.sd_free=3ULL*1073741824};
        cJSON_AddItemToObject(device,"system_stats",sb_system_stats_build(&hardware));
        cJSON_AddItemToArray(devices,device);return devices;
    }
    if (catalog_fixture) {
        if (!strcmp(path, "/cloud/status"))
            return cJSON_Parse("{\"is_authenticated\":true}");
        if (!strcmp(path, "/orca-cloud/status"))
            return cJSON_Parse("{\"connected\":true}");
        if (!strcmp(path, "/cloud/filaments"))
            return cJSON_Parse("[{\"setting_id\":\"GFSL00\",\"name\":\"Bambu PLA Basic @P1S\"}]");
        if (!strcmp(path, "/orca-cloud/profiles"))
            return cJSON_Parse("{\"filament\":[{\"setting_id\":\"orca-uuid\",\"name\":\"Generic "
                               "PETG Basic @P1S\"}]}");
        if (!strcmp(path, "/local-presets/"))
            return cJSON_Parse(
                "{\"filament\":[{\"id\":91,\"name\":\"Local PLA Matte "
                "@P1S\",\"preset_type\":\"filament\",\"filament_vendor\":\"Local\"}]}");
        if (!strcmp(path, "/inventory/catalog"))
            return cJSON_Parse("[{\"id\":12,\"name\":\"Test empty spool\",\"weight\":200}]");
        if (!strcmp(path, "/inventory/locations"))
            return cJSON_Parse("[{\"id\":4,\"name\":\"Dry cabinet\"}]");
        if (!strcmp(path, "/cloud/builtin-filaments"))
            return cJSON_Parse("[{\"filament_id\":\"GFL05\",\"name\":\"Generic PLA\"}]");
        if (strstr(path, "/kprofiles/")) {
            calibration_reads++;
            if (strstr(path, "0.4"))
                return cJSON_Parse(
                    "{\"profiles\":[{\"slot_id\":4,\"name\":\"Bambu PLA "
                    "Basic\",\"filament_id\":\"GFL00\",\"k_value\":\"0.021\",\"extruder_id\":0,"
                    "\"nozzle_diameter\":\"0.4\",\"setting_id\":\"GFSL00\"}]}");
            if (strstr(path, "0.6"))
                return cJSON_Parse(
                    "{\"profiles\":[{\"slot_id\":4,\"name\":\"Bambu PLA "
                    "Basic\",\"filament_id\":\"GFL00\",\"k_value\":\"0.035\",\"extruder_id\":1,"
                    "\"nozzle_diameter\":\"0.6\",\"setting_id\":\"GFSL00\"}]}");
            return cJSON_Parse("{\"profiles\":[]}");
        }
        if (!strcmp(path, "/smart-plugs/"))
            return cJSON_Parse(
                "[{\"id\":8,\"printer_id\":3,\"enabled\":true,\"plug_type\":\"tasmota\"},{\"id\":9,"
                "\"printer_id\":3,\"enabled\":true,\"plug_type\":\"mqtt\"}]");
        if (!strcmp(path, "/smart-plugs/8/status"))
            return cJSON_Parse("{\"state\":\"ON\",\"reachable\":true}");
    }
    if (!strcmp(path, "/settings/spoolman"))
        return cJSON_Parse(
            spoolman ? "{\"spoolman_enabled\":\"true\",\"spoolman_url\":\"http://test.invalid\"}"
                     : "{\"spoolman_enabled\":\"false\"}");
    if (strstr(path, "/inventory/colors/search?")) {
        snprintf(lookup_path, sizeof(lookup_path), "%s", path);
        if (supersede_lookup) {
            supersede_lookup = false;
            bridge_lookup("colors", "/inventory/colors/search?manufacturer=New&material=PLA");
        }
        return cJSON_Parse("[{\"manufacturer\":\"Test\",\"color_name\":\"Forest\",\"hex_color\":\"#"
                           "00AE42\",\"effect_type\":\"matte\"}]");
    }
    if (strstr(path, "/spools?")) {
        cJSON *items = cJSON_Parse(
            "[{\"id\":7,\"brand\":\"Test\",\"material\":\"PLA\",\"label_weight\":1000,\"core_"
            "weight\":250,\"weight_used\":200,\"rgba\":\"FF0000FF\",\"tag_uid\":\"AABB\"}]");
        if (expanded_inventory) {
            cJSON_AddItemToArray(items,
                                 cJSON_Parse("{\"id\":8,\"brand\":\"Other\",\"material\":\"ABS\","
                                             "\"color_name\":\"Black\",\"rgba\":\"000000FF\"}"));
            cJSON_AddItemToArray(items,
                                 cJSON_Parse("{\"id\":9,\"brand\":\"Flex\",\"material\":\"TPU\","
                                             "\"tray_uuid\":\"11223344\",\"rgba\":\"FFFFFFFF\"}"));
        }
        return items;
    }
    if (!strcmp(path, "/printers/"))
        return cJSON_Parse("[{\"id\":3,\"name\":\"Test printer\"}]");
    if (strstr(path, "/status"))
        return cJSON_Parse("{\"connected\":true,\"ams\":[{\"id\":0,\"tray\":[{\"id\":0,\"tray_"
                           "type\":\"PLA\",\"tray_"
                           "color\":\"FF0000FF\"}]}]}");
    if (strstr(path, "assignments"))
        return !inventory_assigned
                   ? cJSON_CreateArray()
                   : cJSON_Parse(
                         spoolman
                             ? "[{\"spoolman_spool_id\":7,\"printer_id\":3,\"ams_id\":0,\"tray_"
                               "id\":0}]"
                             : "[{\"spool_id\":7,\"printer_id\":3,\"ams_id\":0,\"tray_id\":0}]");
    return cJSON_Parse("[]");
}
static double numeric(const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(last_body, key);
    return cJSON_IsNumber(v) ? v->valuedouble : -999999;
}
static float test_manual_grams(void) {
    float grams;
    ui_manual_value(&grams);
    return grams;
}
static int override_count;
static float override_grams;
static void test_override(float grams, void *context) {
    (void)context;
    override_count++;
    override_grams = grams;
}
static void detail(void) {
    original_test_detail(7);
    bridge_poll(transport, false);
}
static void send_confirmed(void) {
#ifdef SB_ENABLE_ACTION_CONFIRMATION
    click("Confirm");
#endif
    bridge_poll(transport, false);
}
int port_transport_test(void) {
    report = fopen("port-transport-test.txt", "w");
    if (!report)
        return 2;
    cJSON *body = cJSON_Parse("{\"value\":1234.5}");
    const char *methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE"};
    for (int i = 0; i < 5; i++) {
        int status = 0;
        cJSON *reply =
            sim_request(methods[i], "/test/echo?language=zh-CN", i ? body : NULL, &status);
        const cJSON *method = cJSON_GetObjectItemCaseSensitive(reply, "method");
        check(status == 200 && cJSON_IsString(method) && !strcmp(method->valuestring, methods[i]),
              methods[i]);
        cJSON_Delete(reply);
    }
    int status = 0;
    cJSON *reply = sim_request("GET", "/test/redirect", NULL, &status);
    check(status == 302, "redirect not followed");
    cJSON_Delete(reply);
    reply = sim_request("GET", "/test/unauthorized", NULL, &status);
    check(status == 401 && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(reply, "detail")),
          "HTTP error JSON preserved");
    cJSON_Delete(reply);
    reply = sim_request("GET", "/test/malformed", NULL, &status);
    check(status == 200 && !reply, "invalid JSON rejected");
    cJSON_Delete(reply);
    reply = sim_request("GET", "/test/oversize", NULL, &status);
    check(!reply, "oversize response rejected");
    cJSON_Delete(reply);
    reply = sim_request("DELETE", "/test/empty", NULL, &status);
    check(status == 204 && cJSON_IsObject(reply), "empty HTTP 204 accepted");
    cJSON_Delete(reply);
    reply = sim_request("GET", "/cloud/builtin-filaments", NULL, &status);
    check(status == 200 && cJSON_IsArray(reply),
          "large unpaginated catalog uses bounded catalog allowance");
    cJSON_Delete(reply);
    cJSON_Delete(body);
    fprintf(report, "%d checks, %d failures\n", checks, failures);
    fclose(report);
    return failures ? 1 : 0;
}
int port_selftest(void) {
    report = fopen("port-selftest.txt", "w");
    if (!report)
        return 2;
    check(fabs(sb_weight_from_adc(1100, 100, .5) - 500) < .001,
          "calibration uses grams per count, matching latest daemon");
    check(fabs(sb_weight_from_adc(-900, 100, -.5) - 500) < .001, "negative ADC slope supported");
    original_language(false);
    bridge_online(true);
    bridge_poll(transport, true);
    check(cJSON_GetArraySize(bridge_model("inventory")) == 1, "inventory model loaded");
    check(bridge_printer_id() == 3, "printer selected from real model IDs");
    detail();
    click("Sync Weight");
    bridge_poll(transport, false);
    check(writes == 0, "missing weight cannot write inventory");
#ifdef SB_ENABLE_MANUAL_WEIGHT
    click("Manual input");
    check(original_test_input("manual_weight", "1234.5"), "manual input field");
    click("Apply");
    check(ui_manual_active() && fabs(test_manual_grams() - 1234.5) < .01,
          "manual gross weight accepted without hardware");
    sb_optional_inputs_poll(1, test_override, NULL);
    check(override_count == 1 && override_grams == 1234.5f, "patch input publishes changed weight");
    sb_optional_inputs_poll(2, test_override, NULL);
    check(override_count == 1, "patch publisher rate limits repeated reads");
    sb_optional_inputs_poll(1000001, test_override, NULL);
    check(override_count == 2, "patch publisher refreshes after one second");
#else
    check(!original_test_click("Manual input"), "upstream profile has no manual-weight UI patch");
    ui_weight(1234.5, true, true);
    sb_optional_inputs_poll(1, test_override, NULL);
    check(override_count == 0, "upstream profile has no optional input publisher");
#endif
    detail();
    click("Sync Weight");
    check(writes == 0, "queued action is not sent from UI callback");
    send_confirmed();
    check(writes == 1 && !strcmp(last_path, "/spoolbuddy/scale/update-spool-weight") &&
              numeric("spool_id") == 7 && numeric("weight_grams") == 1235,
          "sync sends rounded gross weight like upstream and selected spool exactly once");
    original_test_writer(7);
    click("Write tag");
    send_confirmed();
    check(!strcmp(last_path, "/spoolbuddy/nfc/write-tag") && numeric("spool_id") == 7,
          "NFC write command contract");
    original_test_writer(7);
    click("Untag Selected Spool");
    send_confirmed();
    const cJSON *uid = cJSON_GetObjectItemCaseSensitive(last_body, "tag_uid");
    const cJSON *uuid = cJSON_GetObjectItemCaseSensitive(last_body, "tray_uuid");
    check(!strcmp(last_method, "PATCH") && !strcmp(last_path, "/inventory/spools/7/link-tag") &&
              cJSON_IsString(uid) && !uid->valuestring[0] && cJSON_IsString(uuid) &&
              !uuid->valuestring[0],
          "unlink uses empty strings rather than ignored null values");
    detail();
    click("Assign to AMS");
    click("AMS A1   PLA");
    send_confirmed();
    check(!strcmp(last_path, "/inventory/assignments") && numeric("printer_id") == 3 &&
              numeric("spool_id") == 7 && numeric("ams_id") == 0 && numeric("tray_id") == 0,
          "AMS assignment contract");
    original_test_slot(0);
    click("Unassign AMS");
    send_confirmed();
    check(!strcmp(last_method, "DELETE") && !strcmp(last_path, "/inventory/assignments/3/0/0"),
          "AMS removal contract");
    original_page(4);
    original_subpage(1);
    click("2m");
    bridge_poll(transport, false);
    check(!strcmp(last_method, "PUT") && strstr(last_path, "/display") &&
              numeric("blank_timeout") == 120,
          "display timeout uses PUT and seconds");
    original_page(4);
    original_subpage(2);
    click("Tare");
    check(ui_take_tare() && !ui_take_tare(), "tare consumed once by hardware worker");
    click("Calibrate");
    ui_raw(100, true);
    click("Set Zero");
    check(original_test_input("calibration_weight", "500"), "known calibration mass field");
    ui_raw(600, true);
    click("Calibrate");
    send_confirmed();
    check(strstr(last_path, "/calibration/set-factor") && numeric("raw_adc") == 600 &&
              numeric("tare_raw_adc") == 100 && numeric("known_weight_grams") == 500,
          "calibration keeps raw ADC values and known mass");
    original_page(2);
    original_subpage(1);
    original_form(0);
    check(original_test_input("brand", "Test Brand"), "new spool brand field");
    check(original_test_input("label_weight", "1000"), "new spool label weight field");
    click("Create Spool");
    send_confirmed();
    check(!strcmp(last_path, "/inventory/spools") && numeric("label_weight") == 1000 &&
              numeric("weight_used") == 0,
          "new spool form produces numeric API payload");
    original_form(1);
    original_test_quickadd(true);
    check(original_test_input("rgba", "not-a-color"), "full form color field");
    int invalid_before = writes;
    click("Create Spool");
    bridge_poll(transport, false);
    check(writes == invalid_before, "invalid color cannot create spool");
    check(original_test_input("rgba", "#00AE42"), "full form accepts six digit color");
    click("Create Spool");
    send_confirmed();
    const cJSON *color = cJSON_GetObjectItemCaseSensitive(last_body, "rgba");
    check(cJSON_IsString(color) && !strcmp(color->valuestring, "00AE42FF"),
          "hex color normalized to upstream RGBA");
    int before = writes;
#ifdef SB_ENABLE_ACTION_CONFIRMATION
    original_test_writer(7);
    click("Untag Selected Spool");
    click("Cancel");
    bridge_poll(transport, false);
    check(writes == before, "cancel produces no mutation");
#endif
    reply_status = 0;
    detail();
    click("Sync Weight");
    send_confirmed();
    before = writes;
    bridge_poll(transport, true);
    bridge_poll(transport, true);
    check(writes == before, "lost write response is never replayed implicitly");
    reply_status = 409;
    detail();
    click("Sync Weight");
    send_confirmed();
    before = writes;
    bridge_poll(transport, true);
    check(writes == before, "HTTP conflict is never retried implicitly");
    reply_status = 200;
    cJSON *payload = cJSON_Parse("{\"spool_id\":7}");
    bridge_submit("POST", "/isolated/queued", payload);
    cJSON_Delete(payload);
    bridge_online(false);
    bridge_online(true);
    before = writes;
    bridge_poll(transport, true);
    check(writes == before, "disconnect discards pending writes");
    read_status = 401;
    before = reads;
    bridge_poll(transport, true);
    check(reads == before + 1 && !bridge_connected(),
          "authentication failure stops read batch and marks offline");
    read_status = 200;
    bridge_online(true);
    expanded_inventory = true;
    bridge_poll(transport, true);
    original_page(3);
    click("All (3)");
    check(original_test_cards(false) == 3, "inventory renders all live spools");
    click("ABS");
    check(original_test_cards(false) == 1, "material filter is generated from inventory");
    check(sb_sim_capture("integration-material-filter.bmp"), "capture dynamic material filter");
    click("TPU");
    check(original_test_cards(false) == 1, "non-PLA/PETG materials supported");
    click("All (3)");
    check(original_test_input("inventory_search", "black"), "inventory search input");
    check(original_test_cards(false) == 1, "case insensitive color search");
    original_test_input("inventory_search", "");
    original_page(2);
    original_subpage(0);
    check(original_test_cards(true) == 2,
          "existing writer follows upstream tag UID rule including tray-only spools");
    original_subpage(2);
    check(original_test_cards(true) == 2, "replacement writer includes tag or tray bindings only");
    click("Test PLA ");
    original_test_input("write_search", "fLeX");
    check(original_test_cards(true) == 1, "write search filters without rebuilding text input");
    check(!original_test_click("Write tag"),
          "filter clears hidden selection and removes stale write action");
    check(sb_sim_capture("integration-write-search.bmp"), "capture filtered writer");
    original_test_input("write_search", "missing");
    check(original_test_cards(true) == 0, "write search has empty state");
    original_test_input("write_search", "");
    original_subpage(1);
    original_form(0);
    original_test_input("brand", "Test & Brand");
    original_test_colors();
    bridge_poll(transport, false);
    check(strstr(lookup_path, "manufacturer=Test%20%26%20Brand") &&
              strstr(lookup_path, "material=PLA"),
          "catalog query percent-encodes brand and material");
    check(sb_sim_capture("integration-color-catalog.bmp"), "capture live color catalog");
    click("Test · Forest");
    original_test_colors();
    bridge_poll(transport, false);
    original_test_input("catalog_search", "missing");
    check(!original_test_click("Test · Forest"), "catalog search removes unmatched color choices");
    original_test_input("catalog_search", "");
    click("Test · Forest");
    click("Create Spool");
    send_confirmed();
    const cJSON *effect = cJSON_GetObjectItemCaseSensitive(last_body, "effect_type");
    check(cJSON_IsString(effect) && !strcmp(effect->valuestring, "matte"),
          "catalog choice keeps effect metadata in create payload");
    original_test_close();
    read_status = 200;
    bridge_lookup("colors", "/inventory/colors/search?manufacturer=Old&material=PLA");
    supersede_lookup = true;
    bridge_poll(transport, false);
    check(!bridge_model("colors"), "in-flight catalog reply discarded when query changes");
    bridge_poll(transport, false);
    check(cJSON_IsArray(bridge_model("colors")) && strstr(lookup_path, "manufacturer=New"),
          "latest catalog query replaces old response");
    bridge_lookup("colors", "/inventory/colors/search?manufacturer=First&material=PLA");
    bridge_lookup("colors", "/inventory/colors/search?manufacturer=Last&material=PLA");
    bridge_poll(transport, false);
    check(strstr(lookup_path, "manufacturer=Last") != NULL,
          "pending catalog searches coalesce to latest query");
    spoolman = true;
    bridge_online(true);
    bridge_poll(transport, true);
    check(!strcmp(bridge_inventory(), "/spoolman/inventory/spools"),
          "Spoolman mode uses separate inventory namespace");
    detail();
    click("Assign to AMS");
    click("AMS A1   PLA");
    send_confirmed();
    check(!strcmp(last_path, "/spoolman/inventory/slot-assignments") &&
              numeric("spoolman_spool_id") == 7,
          "Spoolman assignment uses spoolman_spool_id");
    original_test_slot(0);
    click("Unassign AMS");
    send_confirmed();
    check(!strcmp(last_path, "/spoolman/inventory/slot-assignments/7"),
          "Spoolman removal uses spool ID");
    original_test_close();
    catalog_fixture = true;
    spoolman = false;
    bridge_init("test-device");
    bridge_online(true);
    bridge_poll(transport, true);
    original_page(2);
    original_subpage(1);
    original_form(1);
    original_test_quickadd(false);
    for (int i = 0; i < 24; i++)
        bridge_poll(transport, false);
    check(!bridge_catalogs_loading() && calibration_reads == 4,
          "PA loads every standard nozzle sequentially");
    click("Search filament presets...");
    check(original_test_input("preset_search", "local"), "preset picker search field");
    check(!original_test_click("Bambu Cloud · Bambu PLA Basic @P1S"),
          "preset search filters cloud rows");
    click("Local · Local PLA Matte @P1S");
    click("Create Spool");
    send_confirmed();
    check(!strcmp(last_path, "/inventory/spools") &&
              !strcmp(cJSON_GetObjectItemCaseSensitive(last_body, "slicer_filament")->valuestring,
                      "91"),
          "local preset persists unique numeric identity");
    check(!strcmp(cJSON_GetObjectItemCaseSensitive(last_body, "brand")->valuestring, "Local"),
          "local vendor preserved");
    click("Local PLA Matte @P1S");
    click("Orca Cloud · Generic PETG Basic @P1S");
    click("Create Spool");
    send_confirmed();
    check(!strcmp(cJSON_GetObjectItemCaseSensitive(last_body, "slicer_filament")->valuestring,
                  "orca-uuid"),
          "Orca cloud preset identity preserved");
    click("Generic PETG Basic @P1S");
    click("Bambu Cloud · Bambu PLA Basic @P1S");
    click("PA Profile");
    click("Auto-select");
    check(sb_sim_capture("integration-pa-profiles.bmp"), "capture real PA selection state");
    click("Create Spool");
    send_confirmed();
    bridge_poll(transport, false);
    check(!strcmp(last_path, "/inventory/spools/42/k-profiles") &&
              cJSON_GetArraySize(last_body) == 2,
          "creation saves both nozzle profiles to created spool ID");
    const cJSON *left = cJSON_GetArrayItem(last_body, 1);
    check(
        fabs(cJSON_GetObjectItemCaseSensitive(left, "k_value")->valuedouble - .035) < .00001 &&
            cJSON_GetObjectItemCaseSensitive(left, "extruder")->valueint == 1 &&
            !strcmp(cJSON_GetObjectItemCaseSensitive(left, "nozzle_diameter")->valuestring, "0.6"),
        "duplicate calibration indices preserve nozzle-specific K and diameter");
    original_test_close();
    original_test_quick();
    bridge_poll(transport, false);
    bridge_poll(transport, false);
    check(sb_sim_capture("integration-power-menu.bmp"), "capture original quick power menu");
    click("Test printer · ON");
    before = writes;
    bridge_poll(transport, false);
    check(writes == before, "power toggle always requires original confirmation");
    click("Cancel");
    original_test_quick();
    bridge_poll(transport, false);
    click("Test printer · ON");
    click("Confirm");
    bridge_poll(transport, false);
    check(!strcmp(last_path, "/smart-plugs/8/control") &&
              !strcmp(cJSON_GetObjectItemCaseSensitive(last_body, "action")->valuestring, "toggle"),
          "confirmed toggle uses original smart plug API");
    original_test_close();
    original_test_slot(0);
    click("Configure slot");
    click("Select preset");
    click("Built-in · Generic PLA");
    check(original_test_input("slot_color", "#112233"), "AMS color input");
    click("Select K profile");
    click("Bambu PLA Basic · Nozzle 0 · K=0.021");
    click("Save");
    send_confirmed();
    check(strstr(last_path, "/printers/3/slots/0/0/configure?") &&
              strstr(last_path, "tray_color=112233FF") &&
              strstr(last_path, "tray_info_idx=GFL05") && strstr(last_path, "k_value=0%2E021"),
          "AMS configure preserves color, filament ID and selected K value");
    bridge_poll(transport, false);
    check(!strcmp(last_method, "PUT") && strstr(last_path, "/printers/3/slot-presets/0/0?") &&
              strstr(last_path, "preset_id=builtin%5FGFL05"),
          "successful configure saves upstream preset mapping");
    original_test_slot(0);
    click("Reset slot");
    before = writes;
    click("Cancel");
    bridge_poll(transport, false);
    check(writes == before, "cancelled slot reset cannot mutate printer");
    original_test_slot(0);
    click("Reset slot");
    click("Confirm");
    bridge_poll(transport, false);
    check(!strcmp(last_path, "/printers/3/ams/0/tray/0/reset"),
          "confirmed reset uses original API");
    original_test_close();
    original_page(2);
    original_subpage(1);
    original_form(1);
    original_test_quickadd(true);
    click("Catalog");
    click("Test empty spool · 200g");
    check(original_test_dropdown("location_id", 1), "storage location picker");
    click("Create Spool");
    send_confirmed();
    check(numeric("core_weight") == 200 && numeric("core_weight_catalog_id") == 12 &&
              numeric("location_id") == 4,
          "catalog tare and real location ID persist in API payload");
    check(original_test_input("core_weight", "225"), "manual tare override input");
    click("Create Spool");
    send_confirmed();
    check(numeric("core_weight") == 225 &&
              !cJSON_GetObjectItemCaseSensitive(last_body, "core_weight_catalog_id"),
          "manual tare override detaches catalog identity");
    check(original_test_input("quantity", "3"), "quick stock quantity field");
    click("Create Spool");
    send_confirmed();
    check(!strcmp(last_path, "/inventory/spools/bulk") && numeric("quantity") == 3 &&
              cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(last_body, "spool")),
          "quick stock uses original bulk envelope");
    check(original_test_input("quantity", "101"), "invalid bulk quantity input");
    before = writes;
    click("Create Spool");
    bridge_poll(transport, false);
    check(writes == before, "bulk quantity above 100 rejected before API");
    original_test_input("quantity", "1");
    ui_tag("A1B2C3D4", "Unknown");
    original_page(0);
    click("Add to Inventory");
    before = writes;
    click("Cancel");
    bridge_poll(transport, false);
    check(writes == before, "cancelled unknown tag quick-add does not create spool");
    click("Add to Inventory");
    click("Add Anyway");
    bridge_poll(transport, false);
    check(!strcmp(last_path, "/inventory/spools") &&
              !strcmp(cJSON_GetObjectItemCaseSensitive(last_body, "tag_uid")->valuestring,
                      "A1B2C3D4") &&
              numeric("core_weight") == 250,
          "unknown tag quick-add retains original NFC provenance and core weight");
    check(sb_live.spool_id == 42, "quick-created spool replaces unknown tag card without rescan");
    original_test_quick();
    bridge_poll(transport, false);
    bridge_poll(transport, false);
    click("Restart Browser");
    click("Confirm");
    bridge_poll(transport, false);
    const cJSON *command = cJSON_GetObjectItemCaseSensitive(last_body, "command");
    check(cJSON_IsString(command) && !strcmp(command->valuestring, "restart_browser"),
          "original system commands preserve browser versus daemon identity");
    original_test_close();
    cJSON *push = cJSON_Parse("{\"type\":\"printer_status\",\"printer_id\":3,\"data\":{"
                              "\"connected\":true,\"ams\":[],\"test_marker\":77}}");
    bridge_event(push);
    cJSON_Delete(push);
    check(
        cJSON_GetObjectItemCaseSensitive(bridge_model("printer_status"), "test_marker")->valueint ==
            77,
        "WebSocket event updates selected printer model");
    push = cJSON_Parse(
        "{\"type\":\"printer_status\",\"printer_id\":9,\"data\":{\"connected\":false}}");
    bridge_event(push);
    cJSON_Delete(push);
    check(
        cJSON_GetObjectItemCaseSensitive(bridge_model("printer_status"), "test_marker")->valueint ==
            77,
        "other printer push cannot overwrite selected slots");
    before = writes;
    push = cJSON_Parse("{\"type\":\"spoolbuddy_tag_write_failed\",\"device_id\":\"another-device\","
                       "\"message\":\"ignored\"}");
    bridge_event(push);
    cJSON_Delete(push);
    check(writes == before, "foreign device event never issues a write");
    ui_network(true);check(original_test_network(true),"network icon reflects connected link at original header size");
    sb_sim_capture("integration-network-connected.bmp");
    bridge_online(false);check(original_test_network(true),"backend disconnect does not falsely show Wi-Fi disconnected");
    ui_network(false);check(original_test_network(false),"network icon reflects lost link");
    ui_network(true);original_page(4);check(original_test_network(true),"page navigation preserves network icon state");
    bridge_online(true); system_fixture=true;language_fixture="en";bridge_poll(transport,true);
    check(!sb_settings.chinese,"language follows backend English setting");
    original_subpage(4);
    check(original_test_text("CPU") && original_test_text("Cores") && original_test_text("42.5°C"),"system page renders CPU and real snapshot temperature");
    check(original_test_text("Memory") && original_test_text("Used") && original_test_text("Free"),"system page matches upstream memory rows");
    check(original_test_text("Disk") && original_test_text("OS") && original_test_text("Runtime"),"system page contains disk OS and runtime cards");
    check(original_test_text("FreeRTOS") && original_test_text("riscv32") && original_test_text("1d 1h 1m"),"system values use ESP32 platform and uptime");
    sb_sim_capture("integration-system-en-top.bmp");
    original_test_settings_scroll(100);sb_sim_capture("integration-system-en-bottom.bmp");
    language_fixture="zh-CN";bridge_poll(transport,true);
    check(sb_settings.chinese && original_test_text("运行信息"),"backend language switches system page to Chinese");
    sb_sim_capture("integration-system-zh-top.bmp");
    original_test_settings_scroll(100);sb_sim_capture("integration-system-zh-bottom.bmp");
    language_fixture="de";bridge_poll(transport,true);
    check(!sb_settings.chinese,"unsupported backend locale falls back to supported English");
    const char *zh_locales[]={"zh-TW","zh-HK","zh-Hant","ZH_hans"};
    for(unsigned i=0;i<sizeof(zh_locales)/sizeof(zh_locales[0]);i++) {
        language_fixture=zh_locales[i];bridge_poll(transport,true);
        check(sb_settings.chinese,"all backend Chinese variants use Simplified Chinese");
    }
    sb_settings.language_override=true;ui_language(false);language_fixture="zh-CN";bridge_poll(transport,true);
    check(!sb_settings.chinese,"explicit English config overrides backend Chinese");
    ui_language(true);language_fixture="en";bridge_poll(transport,true);
    check(sb_settings.chinese,"explicit Chinese config overrides backend English");
    sb_settings.language_override=false;bridge_poll(transport,true);
    check(!sb_settings.chinese,"omitted config language follows backend again");
    for(int lang=0;lang<2;lang++) for(int format=0;format<2;format++) {
        language_fixture=lang?"zh-CN":"en";time_format_fixture=format?"12h":"24h";
        bridge_poll(transport,true);original_test_refresh_clock();
        check(original_test_network(true),"compact header fits both languages and 12/24-hour clocks");
        char screenshot[80];snprintf(screenshot,sizeof(screenshot),"integration-topbar-%s-%s.bmp",
                                    lang?"zh":"en",format?"12h":"24h");
        sb_sim_capture(screenshot);
    }
    language_fixture="en";time_format_fixture="24h";bridge_poll(transport,true);original_test_refresh_clock();
    original_subpage(3);
    check(original_test_text(SB_FIRMWARE_VERSION),"update version is plain upstream version without build suffix");
    check(original_test_text("Update from TF Card"),"TF update entry is available");
    check(original_test_text("Check for Updates"),"project OTA settings replace Pi USB-only placeholder");
    check(!original_test_text("Apply Update") && !original_test_text("Force Update"),"OTA uses one check-and-install action without a second confirmation");
    check(original_test_update_height()==96,"idle update card has no reserved status/progress whitespace");
    const sb_update_phase phases[]={SB_UPDATE_CHECKING,SB_UPDATE_READY,SB_UPDATE_DOWNLOADING,
        SB_UPDATE_VERIFYING,SB_UPDATE_REBOOTING,SB_UPDATE_ERROR,SB_UPDATE_CURRENT,SB_UPDATE_EMPTY};
    for(int lang=0;lang<2;lang++) {
        ui_language(lang!=0);
        for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);i++) {
            sb_update_state fixture={.phase=phases[i],.progress=37};
            fixture.artifact=phases[i]==SB_UPDATE_READY || phases[i]==SB_UPDATE_DOWNLOADING ||
                phases[i]==SB_UPDATE_VERIFYING || phases[i]==SB_UPDATE_REBOOTING;
            if(fixture.artifact)snprintf(fixture.version,sizeof(fixture.version),"1.2.5.8");
            if(phases[i]==SB_UPDATE_ERROR)snprintf(fixture.message,sizeof(fixture.message),"HTTPS request failed: connection closed");
            sb_sim_update_state(&fixture);original_test_refresh_clock();
            check(original_test_update_height()<=226,"all update states fit on one screen in either language");
            bool busy=phases[i]==SB_UPDATE_CHECKING || phases[i]==SB_UPDATE_DOWNLOADING ||
                phases[i]==SB_UPDATE_VERIFYING || phases[i]==SB_UPDATE_REBOOTING;
            check(original_test_text(lang?"检查更新":"Check for Updates")==!busy,"update actions are hidden while busy and return afterwards");
            if(fixture.artifact)check(original_test_text(lang?"有可用更新: 1.2.5.8":"Update available: 1.2.5.8"),"available firmware version is displayed");
            if(phases[i]==SB_UPDATE_DOWNLOADING)check(original_test_text("37%"),"download progress percentage is displayed");
            if(phases[i]==SB_UPDATE_DOWNLOADING || phases[i]==SB_UPDATE_ERROR) {
                char screenshot[80];snprintf(screenshot,sizeof(screenshot),"integration-updates-%s-%s.bmp",lang?"zh":"en",phases[i]==SB_UPDATE_DOWNLOADING?"progress":"error");sb_sim_capture(screenshot);
            }
        }
        sb_update_state idle={0};sb_sim_update_state(&idle);original_test_refresh_clock();
        check(original_test_update_height()==96,"update card returns to compact layout when reset");
    }
    ui_language(false);
    before=writes;click("Check for Updates");
    check(writes==before,"OTA check never submits a Raspberry Pi update command");
    check(original_test_text("OTA runs on the ESP32 board"),"simulator honestly reports it cannot run board OTA");
    sb_sim_capture("integration-updates-en.bmp");
    /* Exercise the board's pure LVGL blanker with the actual renderer, not GPIO. */
    sb_display_blanking blanker={0};
    lv_display_t *display=lv_display_get_default();
    uint32_t children=lv_obj_get_child_count(lv_display_get_layer_sys(display));
    sb_display_blanking_set(&blanker,display,true);
    lv_obj_update_layout(blanker.mask);
    check(blanker.mask && lv_obj_get_parent(blanker.mask)==lv_display_get_layer_sys(display),
          "blackout is above every application page and dialog");
    check(lv_obj_get_width(blanker.mask)==800 && lv_obj_get_height(blanker.mask)==480,
          "blackout covers the entire rotated display");
    check(lv_obj_has_flag(blanker.mask,LV_OBJ_FLAG_CLICKABLE),"blackout intercepts hidden controls");
    lv_obj_t *same_mask=blanker.mask;
    for(int i=0;i<100;i++) sb_display_blanking_set(&blanker,display,true);
    check(blanker.mask==same_mask && lv_obj_get_child_count(lv_display_get_layer_sys(display))==children+1,
          "repeated blanking does not allocate more overlays");
    sb_sim_capture("integration-blank-mask.bmp");
    sb_display_blanking_set(&blanker,display,false);
    check(lv_obj_has_flag(blanker.mask,LV_OBJ_FLAG_HIDDEN),"wake restores the existing application without resetting it");
    for(int i=0;i<100;i++) {
        sb_display_blanking_set(&blanker,display,true);
        sb_display_blanking_set(&blanker,display,false);
    }
    check(lv_obj_get_child_count(lv_display_get_layer_sys(display))==children+1,
          "repeated sleep and wake reuse the same mask");
    sb_sim_capture("integration-blank-restored.bmp");
    lv_obj_delete(blanker.mask);
    fprintf(report, "\n%d checks, %d failures. Isolated data only; no physical hardware tested.\n",
            checks, failures);
    fclose(report);
    cJSON_Delete(last_body);
    return failures ? 1 : 0;
}
