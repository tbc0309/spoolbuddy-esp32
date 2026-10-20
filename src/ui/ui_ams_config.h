// SPDX-License-Identifier: AGPL-3.0-or-later
// Original AMS slot action picker and ConfigureAmsSlotModal workflow.
static char slot_request[1536], slot_mapping[1536];
static const cJSON *slot_tray(void) {
    int ams = config_slot / 256, tray = config_slot % 256;
    const cJSON *unit, *entry;
    if (ams == 254) {
        cJSON_ArrayForEach(entry,
                           prop(bridge_model("printer_status"),
                                "vt_tray")) if (number(entry, "id", 254) == tray) return entry;
        return NULL;
    }
    cJSON_ArrayForEach(unit, prop(bridge_model("printer_status"), "ams")) {
        if (number(unit, "id", -1) != ams)
            continue;
        cJSON_ArrayForEach(entry,
                           prop(unit, "tray")) if (number(entry, "id", -1) == tray) return entry;
    }
    return NULL;
}
static void slot_changed(lv_event_t *e) {
    const char *key = lv_event_get_user_data(e);
    cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, key);
    cJSON_AddStringToObject(slot_draft, key, lv_textarea_get_text(lv_event_get_target_obj(e)));
}
static void slot_field(lv_obj_t *panel, const char *label, const char *key, const char *value,
                       int x, int y, int width) {
    text(panel, label, x, y, width, MUTED, &small);
    lv_obj_t *input = field(panel, "", x, y + 22, width, 34);
    lv_textarea_set_text(input, str(slot_draft, key)[0] ? str(slot_draft, key) : value);
    if (!prop(slot_draft, key))
        cJSON_AddStringToObject(slot_draft, key, value);
    lv_obj_set_user_data(input, (void *)key);
    lv_obj_add_event_cb(input, slot_changed, LV_EVENT_VALUE_CHANGED, (void *)key);
}
static bool slot_param(char *path, size_t capacity, const char *key, const char *value) {
    char encoded[768];
    if (!url_encode(value, encoded, sizeof(encoded)))
        return false;
    size_t used = strlen(path);
    int n = snprintf(path + used, capacity - used, "%s%s=%s", strchr(path, '?') ? "&" : "?", key,
                     encoded);
    return n >= 0 && (size_t)n < capacity - used;
}
static void slot_preset_selected(const cJSON *preset, int source) {
    char brand[96], material[32], variant[96];
    parse_preset(str(preset, "name"), brand, material, variant);
    int min = 190, max = 230;
    if (strstr(material, "PETG") || !strcmp(material, "PCTG")) {
        min = 220;
        max = 260;
    } else if (strstr(material, "ABS") || strstr(material, "ASA")) {
        min = 240;
        max = 280;
    } else if (strstr(material, "TPU")) {
        min = 200;
        max = 240;
    } else if (strstr(material, "PC")) {
        min = 260;
        max = 300;
    } else if (strstr(material, "PA")) {
        min = 250;
        max = 290;
    }
    if (source == 2) {
        min = (int)number(preset, "nozzle_temp_min", min);
        max = (int)number(preset, "nozzle_temp_max", max);
    }
    char value[32];
    snprintf(value, sizeof(value), "%d", min);
    cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "slot_temp_min");
    cJSON_AddStringToObject(slot_draft, "slot_temp_min", value);
    snprintf(value, sizeof(value), "%d", max);
    cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "slot_temp_max");
    cJSON_AddStringToObject(slot_draft, "slot_temp_max", value);
    cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "calibration");
}
static void slot_config_save(lv_event_t *e) {
    (void)e;
    int source = (int)number(slot_draft, "preset_source", -1),
        index = (int)number(slot_draft, "preset_index", -1);
    const cJSON *preset = cJSON_GetArrayItem(preset_source(source), index);
    if (source < 0 || !preset) {
        original_status(tr("port.SelectAFilamentPreset"));
        return;
    }
    char brand[96], material[32], variant[96];
    parse_preset(str(preset, "name"), brand, material, variant);
    if (!material[0])
        snprintf(material, sizeof(material), "%s", str(preset, "filament_type"));
    if (!material[0]) {
        original_status(tr("port.MaterialIsRequired"));
        return;
    }
    const char *color = str(slot_draft, "slot_color");
    if (color[0] == '#')
        color++;
    if (strlen(color) != 6 && strlen(color) != 8) {
        original_status(tr("port.InvalidHexColor"));
        return;
    }
    for (size_t i = 0; color[i]; i++)
        if (!isxdigit((unsigned char)color[i])) {
            original_status(tr("port.InvalidHexColor"));
            return;
        }
    char rgba[9];
    snprintf(rgba, sizeof(rgba), "%.6sFF", color);
    char nozzle[32];
    snprintf(nozzle, sizeof(nozzle), "%s", str(slot_draft, "slot_nozzle"));
    if (strcmp(nozzle, "0.2") && strcmp(nozzle, "0.4") && strcmp(nozzle, "0.6") &&
        strcmp(nozzle, "0.8")) {
        original_status(tr("port.InvalidNozzleDiameter"));
        return;
    }
    float min, max;
    if (!sb_parse_weight(str(slot_draft, "slot_temp_min"), &min) ||
        !sb_parse_weight(str(slot_draft, "slot_temp_max"), &max) || min > max || max > 400) {
        original_status(tr("port.InvalidTemperatureRange"));
        return;
    }
    char code[96], tray[96], mapping_id[112], setting[96] = "";
    const char *mapping_source = source == 0   ? "cloud"
                                 : source == 1 ? "orca_cloud"
                                 : source == 2 ? "local"
                                               : "builtin";
    if (source == 2) {
        snprintf(code, sizeof(code), "%d", (int)number(preset, "id", 0));
        snprintf(mapping_id, sizeof(mapping_id), "local_%s", code);
        const char *types[] = {"PLA", "PLA-CF", "PETG",  "PETG-CF", "PCTG", "ABS", "ASA",
                               "PC",  "PA",     "PA-CF", "TPU",     "PVA",  "HIPS"};
        const char *ids[] = {"GFL99", "GFL98", "GFG99", "GFG98", "GFG97", "GFB99", "GFB98",
                             "GFC99", "GFN99", "GFN98", "GFU99", "GFS99", "GFS98"};
        tray[0] = 0;
        for (int i = 0; i < 13; i++)
            if (!strcmp(material, types[i]))
                snprintf(tray, sizeof(tray), "%s", ids[i]);
    } else if (source == 3) {
        snprintf(code, sizeof(code), "%s", str(preset, "filament_id"));
        snprintf(tray, sizeof(tray), "%s", code);
        snprintf(mapping_id, sizeof(mapping_id), "builtin_%s", code);
    } else {
        snprintf(code, sizeof(code), "%s", str(preset, "setting_id"));
        snprintf(mapping_id, sizeof(mapping_id), source == 1 ? "orca_%s" : "%s", code);
        if (source == 1)
            tray[0] = 0;
        else {
            snprintf(setting, sizeof(setting), "%s", code);
            filament_id(code, tray);
            if (!strncmp(tray, "PFUS", 4) || !strncmp(tray, "PFSP", 4) || !strncmp(tray, "PFCN", 4))
                tray[0] = 0;
        }
    }
    int ams = config_slot / 256, id = config_slot % 256;
    char path[1536], value[64];
    snprintf(path, sizeof(path), "/printers/%d/slots/%d/%d/configure", config_printer, ams, id);
    bool ok = slot_param(path, sizeof(path), "tray_info_idx", tray) &&
              slot_param(path, sizeof(path), "tray_type", material) &&
              slot_param(path, sizeof(path), "tray_sub_brands", str(preset, "name")) &&
              slot_param(path, sizeof(path), "tray_color", rgba) &&
              slot_param(path, sizeof(path), "nozzle_diameter", nozzle) &&
              slot_param(path, sizeof(path), "setting_id", setting);
    snprintf(value, sizeof(value), "%.0f", min);
    ok = ok && slot_param(path, sizeof(path), "nozzle_temp_min", value);
    snprintf(value, sizeof(value), "%.0f", max);
    ok = ok && slot_param(path, sizeof(path), "nozzle_temp_max", value);
    // Selected K profile includes the hotend identity and diameter, never index alone.
    const cJSON *cal = prop(slot_draft, "calibration");
    snprintf(value, sizeof(value), "%d", (int)number(cal, "slot_id", -1));
    ok = ok && slot_param(path, sizeof(path), "cali_idx", value);
    if (cal) {
        if (strcmp(str(cal, "nozzle_diameter"), nozzle)) {
            original_status(
                tr("port.SelectAKProfileForThisDiameter"));
            return;
        }
        snprintf(value, sizeof(value), "%.6g", pa_k(cal));
        ok = ok && slot_param(path, sizeof(path), "k_value", value) &&
             slot_param(path, sizeof(path), "kprofile_filament_id", str(cal, "filament_id")) &&
             slot_param(path, sizeof(path), "kprofile_setting_id", str(cal, "setting_id"));
    }
    if (source == 1)
        ok = ok && slot_param(path, sizeof(path), "orca_profile_id", code);
    snprintf(slot_mapping, sizeof(slot_mapping), "/printers/%d/slot-presets/%d/%d", config_printer,
             ams, id);
    ok = ok && slot_param(slot_mapping, sizeof(slot_mapping), "preset_id", mapping_id) &&
         slot_param(slot_mapping, sizeof(slot_mapping), "preset_name", str(preset, "name")) &&
         slot_param(slot_mapping, sizeof(slot_mapping), "preset_source", mapping_source);
    if (!ok) {
        slot_mapping[0] = 0;
        original_status(tr("port.PresetDataIsTooLong"));
        return;
    }
    snprintf(slot_request, sizeof(slot_request), "%s", path);
    confirm_request(tr("port.ConfigureAmsSlot"), "POST", path, NULL);
}
static void ams_command_result(const char *path, int code, const cJSON *reply) {
    if (!slot_request[0] || strcmp(path, slot_request))
        return;
    slot_request[0] = 0;
    if (code >= 200 && code < 300 && slot_mapping[0]) {
        if (str(reply, "tray_info_idx")[0])
            slot_param(slot_mapping, sizeof(slot_mapping), "tray_info_idx",
                       str(reply, "tray_info_idx"));
        bridge_submit("PUT", slot_mapping, NULL);
    }
    slot_mapping[0] = 0;
}
static void slot_k_choose(lv_event_t *e) {
    uintptr_t code = (uintptr_t)lv_event_get_user_data(e);
    int d = (int)(code >> 16), index = (int)(code & 65535);
    cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "calibration");
    if (index) {
        char key[32];
        snprintf(key, sizeof(key), "pa_%d_%d", config_printer, d);
        const cJSON *cal = cJSON_GetArrayItem(prop(bridge_model(key), "profiles"), index - 1);
        if (cal)
            cJSON_AddItemToObject(slot_draft, "calibration", cJSON_Duplicate(cal, true));
    }
    ams_config_show();
}
static void slot_k_open(lv_event_t *e) {
    (void)e;
    lv_obj_t *panel = modal_panel(tr("port.KProfile"), 400);
    lv_obj_t *list = rect(panel, 20, 60, 480, 320, SECONDARY, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    button(list, tr("common.none"), 0, 0, 476, 40, CARD, slot_k_choose, 0);
    int row = 1;
    const cJSON *cal;
    for (int d = 0; d < 4; d++) {
        char key[32];
        snprintf(key, sizeof(key), "pa_%d_%d", config_printer, d);
        int i = 0;
        cJSON_ArrayForEach(cal, prop(bridge_model(key), "profiles")) {
            i++;
            if (strcmp(str(cal, "nozzle_diameter"), str(slot_draft, "slot_nozzle")))
                continue;
            char caption[256];
            snprintf(caption, sizeof(caption), "%s · %s %d · K=%.3f", str(cal, "name"),
                     tr("spoolbuddy.inventory.nozzleTemp"), (int)number(cal, "extruder_id", 0), pa_k(cal));
            button(list, caption, 0, row++ * 44, 476, 40, CARD, slot_k_choose, (d << 16) | i);
        }
    }
}
static void ams_config_show(void) {
    bridge_load_catalogs();
    lv_obj_t *panel = modal_panel(tr("port.ConfigureAmsSlot_8e2b59"), 440);
    lv_obj_t *scroll = rect(panel, 16, 58, 488, 306, SECONDARY, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    const cJSON *preset =
        cJSON_GetArrayItem(preset_source((int)number(slot_draft, "preset_source", -1)),
                           (int)number(slot_draft, "preset_index", -1));
    button(scroll, preset ? str(preset, "name") : tr("port.SelectPreset"), 0, 0, 476, 40,
           CARD, preset_open, 1);
    slot_field(scroll, tr("spoolbuddy.spool.color"), "slot_color", "808080", 0, 54, 232);
    slot_field(scroll, tr("port.NozzleDiameter"), "slot_nozzle", "0.4", 244, 54, 232);
    slot_field(scroll, tr("port.MinimumTemperature"), "slot_temp_min", "190", 0, 124, 232);
    slot_field(scroll, tr("port.MaximumTemperature"), "slot_temp_max", "230", 244, 124,
               232);
    const cJSON *cal = prop(slot_draft, "calibration");
    button(scroll, cal ? str(cal, "name") : tr("port.SelectKProfile"), 0, 200, 476, 40,
           CARD, slot_k_open, 0);
    button(panel, tr("common.save"), 20, 382, 480, 42, GREEN, slot_config_save, 0);
}
static const cJSON *slot_assignment(void) {
    const cJSON *a;
    cJSON_ArrayForEach(
        a, bridge_model("assignments")) if (number(a, "printer_id", -1) == config_printer &&
                                            number(a, "ams_id", -1) == config_slot / 256 &&
                                            number(a, "tray_id", -1) == config_slot % 256) return a;
    return NULL;
}
static bool slot_has_tag(void) {
    const cJSON *tray = slot_tray();
    return (str(tray, "tray_uuid")[0] &&
            strcmp(str(tray, "tray_uuid"), "00000000000000000000000000000000")) ||
           (str(tray, "tag_uid")[0] && strcmp(str(tray, "tag_uid"), "0000000000000000"));
}
static void slot_tag(char out[80]) {
    const cJSON *tray = slot_tray();
    const char *value = str(tray, "tray_uuid")[0] ? str(tray, "tray_uuid") : str(tray, "tag_uid");
    if (value[0])
        snprintf(out, 80, "%s", value);
    else {
        const cJSON *p;
        const char *serial = "";
        cJSON_ArrayForEach(p, bridge_model("printers")) if (number(p, "id", 0) == config_printer) {
            serial = str(p, "serial_number");
            break;
        }
        while (isspace((unsigned char)*serial))
            serial++;
        size_t n = strlen(serial);
        while (n && isspace((unsigned char)serial[n - 1]))
            n--;
        uint32_t hash = 0x811c9dc5u;
        for (size_t i = 0; i < n; i++) {
            hash ^= (unsigned char)toupper((unsigned char)serial[i]);
            hash *= 0x01000193u;
        }
        snprintf(out, 80, "%08lX%04X%04X", (unsigned long)hash, config_slot / 256,
                 config_slot % 256);
    }
    for (char *c = out; *c; c++)
        *c = (char)toupper((unsigned char)*c);
}
static const cJSON *slot_linked(void) {
    char tag[80];
    slot_tag(tag);
    return prop(prop(bridge_model("linked_spools"), "linked"), tag);
}
static char slot_link_query[96];
static void slot_link_choose(lv_event_t *e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    char path[160], tag[80];
    slot_tag(tag);
    snprintf(path, sizeof(path), "/spoolman/spools/%d/link", id);
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "spool_tag", tag);
    cJSON_AddNumberToObject(payload, "printer_id", config_printer);
    cJSON_AddNumberToObject(payload, "ams_id", config_slot / 256);
    cJSON_AddNumberToObject(payload, "tray_id", config_slot % 256);
    confirm_request(tr("port.LinkSpool"), "POST", path, payload);
    cJSON_Delete(payload);
}
static void slot_link_render(void) {
    if (!slot_link_list)
        return;
    lv_obj_clean(slot_link_list);
    int row = 0;
    const cJSON *s;
    cJSON_ArrayForEach(s, bridge_model("unlinked_spools")) {
        char caption[256];
        snprintf(caption, sizeof(caption), "#%d %s %s %s", (int)number(s, "id", 0),
                 str(s, "filament_vendor"), str(s, "filament_name"), str(s, "filament_material"));
        if (!contains(caption, slot_link_query))
            continue;
        button(slot_link_list, caption, 0, row++ * 44, 476, 40, CARD, slot_link_choose,
               (int)number(s, "id", 0));
    }
    if (!row)
        text(slot_link_list, tr("port.NoMatchingUnlinkedSpools"), 8, 20, 460,
             MUTED, &small);
}
static void slot_link_search(lv_event_t *e) {
    snprintf(slot_link_query, sizeof(slot_link_query), "%s",
             lv_textarea_get_text(lv_event_get_target_obj(e)));
    slot_link_render();
}
static void slot_link_open(void) {
    slot_link_query[0] = 0;
    lv_obj_t *panel = modal_panel(tr("port.LinkSpoolmanSpool"), 420);
    lv_obj_t *search = field(panel, tr("spoolbuddy.inventory.search"), 20, 60, 480, 36);
    lv_obj_set_user_data(search, (void *)"slot_link_search");
    lv_obj_add_event_cb(search, slot_link_search, LV_EVENT_VALUE_CHANGED, NULL);
    slot_link_list = rect(panel, 20, 110, 480, 290, SECONDARY, 0);
    lv_obj_add_flag(slot_link_list, LV_OBJ_FLAG_SCROLLABLE);
    bridge_lookup("unlinked_spools", "/spoolman/spools/unlinked");
    slot_link_render();
}
static void slot_action(lv_event_t *e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    if (action == 0) {
        cJSON_Delete(slot_draft);
        slot_draft = cJSON_CreateObject();
        const cJSON *tray = slot_tray();
        cJSON_AddStringToObject(slot_draft, "slot_color",
                                str(tray, "tray_color")[0] ? str(tray, "tray_color") : "808080");
        ams_config_show();
    }
    if (action == 1)
        slot_picker(config_slot);
    if (action == 4) {
        slot_link_open();
        return;
    }
    if (action == 5) {
        const cJSON *linked = slot_linked();
        if (!linked)
            return;
        char path[160];
        snprintf(path, sizeof(path), "/spoolman/spools/%d/unlink", (int)number(linked, "id", 0));
        confirm_request(tr("port.UnlinkSpool"), "POST", path, NULL);
        return;
    }
    if (action == 3) {
        const cJSON *a = slot_assignment();
        if (!a)
            return;
        char path[160];
        if (strstr(bridge_inventory(), "spoolman"))
            snprintf(path, sizeof(path), "/spoolman/inventory/slot-assignments/%d",
                     (int)number(a, "spoolman_spool_id", 0));
        else
            snprintf(path, sizeof(path), "/inventory/assignments/%d/%d/%d", config_printer,
                     config_slot / 256, config_slot % 256);
        confirm_request(tr("port.UnassignSpool"), "DELETE", path, NULL);
        return;
    }
    if (action == 2) {
        char path[160];
        snprintf(path, sizeof(path), "/printers/%d/ams/%d/tray/%d/reset", config_printer,
                 config_slot / 256, config_slot % 256);
        original_confirm(tr("port.ResetSlot"), "POST", path, NULL);
    }
}
static void ams_slot_actions(int code) {
    config_slot = code;
    config_printer = bridge_printer_id();
    lv_obj_t *panel = modal_panel(tr("port.AmsSlot"), 290);
    button(panel, tr("port.ConfigureSlot"), 20, 66, 480, 48, CARD, slot_action, 0);
    const cJSON *a = slot_assignment();
    bool spoolman = strstr(bridge_inventory(), "spoolman") != NULL;
    const cJSON *linked = spoolman ? slot_linked() : NULL;
    if (spoolman || !slot_has_tag())
        button(panel,
               linked     ? tr("port.UnlinkSpool_919774")
               : a        ? tr("port.UnassignAms")
               : spoolman ? tr("port.LinkSpool_63ccbe")
                          : tr("port.AssignSpool"),
               20, 126, 480, 48, CARD, slot_action,
               linked     ? 5
               : a        ? 3
               : spoolman ? 4
                          : 1);
    button(panel, tr("port.ResetSlot_466c2f"), 20, 186, 480, 48, CARD, slot_action, 2);
}
