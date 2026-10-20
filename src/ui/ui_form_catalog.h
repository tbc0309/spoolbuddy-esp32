// SPDX-License-Identifier: AGPL-3.0-or-later
// Private helpers for the original new-spool form and quick menu.
// Included by ui_original.c so all widgets share its lifetime and display lock.
static void draft_string(const char *key, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(draft, key);
    cJSON_AddStringToObject(draft, key, value);
}
static const char *materials[] = {"PLA-CF", "PETG-CF", "ABS-GF", "ASA-CF", "PA-CF",  "PAHT-CF",
                                  "PA6-CF", "PA6-GF",  "PPA-CF", "PPA-GF", "PET-CF", "PPS-CF",
                                  "PC-CF",  "PC-ABS",  "PCTG",   "PETG",   "PLA",    "ABS",
                                  "ASA",    "PC",      "PA",     "TPU",    "PVA",    "HIPS",
                                  "BVOH",   "PPS",     "PEEK",   "PEI"};
static void parse_preset(const char *name, char *brand, char *material, char *variant) {
    char clean[256], upper[256];
    snprintf(clean, sizeof(clean), "%s", name);
    char *at = strchr(clean, '@');
    if (at)
        *at = 0;
    snprintf(upper, sizeof(upper), "%s", clean);
    for (char *c = upper; *c; c++)
        *c = (char)toupper((unsigned char)*c);
    brand[0] = material[0] = variant[0] = 0;
    for (unsigned i = 0; i < sizeof(materials) / sizeof(materials[0]); i++) {
        char *found = strstr(upper, materials[i]);
        if (!found)
            continue;
        size_t before = (size_t)(found - upper);
        while (before && (isspace((unsigned char)clean[before - 1]) || clean[before - 1] == '_' ||
                          clean[before - 1] == '-'))
            before--;
        snprintf(brand, 96, "%.*s", (int)before, clean);
        snprintf(material, 32, "%s", materials[i]);
        const char *after = clean + (found - upper) + strlen(materials[i]);
        while (*after && (isspace((unsigned char)*after) || *after == '_' || *after == '-'))
            after++;
        snprintf(variant, 96, "%s", after);
        size_t n = strlen(variant);
        while (n && isspace((unsigned char)variant[n - 1]))
            variant[--n] = 0;
        return;
    }
}
static const cJSON *preset_source(int source) {
    switch (source) {
    case 0:
        return bridge_model("cloud_presets");
    case 1:
        return prop(bridge_model("orca_presets"), "filament");
    case 2:
        return prop(bridge_model("local_presets"), "filament");
    default: {
        const cJSON *presets = bridge_model("builtin_presets");
        if (cJSON_IsArray(presets) && cJSON_GetArraySize(presets))
            return presets;
        static cJSON *fallback;
        if (!fallback)
            fallback = cJSON_Parse("[{\"filament_id\":\"GFL00\",\"name\":\"Bambu PLA "
                                   "Basic\"},{\"filament_id\":\"GFL01\",\"name\":\"Bambu PLA "
                                   "Matte\"},{\"filament_id\":\"GFL05\",\"name\":\"Generic "
                                   "PLA\"},{\"filament_id\":\"GFG00\",\"name\":\"Bambu PETG "
                                   "Basic\"},{\"filament_id\":\"GFG05\",\"name\":\"Generic "
                                   "PETG\"},{\"filament_id\":\"GFB00\",\"name\":\"Bambu ABS "
                                   "Basic\"},{\"filament_id\":\"GFB05\",\"name\":\"Generic "
                                   "ABS\"},{\"filament_id\":\"GFA00\",\"name\":\"Bambu ASA "
                                   "Basic\"},{\"filament_id\":\"GFU00\",\"name\":\"Bambu TPU "
                                   "95A\"},{\"filament_id\":\"GFU05\",\"name\":\"Generic "
                                   "TPU\"},{\"filament_id\":\"GFC00\",\"name\":\"Bambu PC "
                                   "Basic\"},{\"filament_id\":\"GFN00\",\"name\":\"Bambu PA "
                                   "Basic\"},{\"filament_id\":\"GFN05\",\"name\":\"Generic "
                                   "PA\"},{\"filament_id\":\"GFS00\",\"name\":\"Bambu "
                                   "PLA-CF\"},{\"filament_id\":\"GFT00\",\"name\":\"Bambu "
                                   "PETG-CF\"},{\"filament_id\":\"GFNC0\",\"name\":\"Bambu "
                                   "PA-CF\"},{\"filament_id\":\"GFV00\",\"name\":\"Bambu PVA\"}]");
        return fallback;
    }
    }
}
static void preset_choose(lv_event_t *e) {
    uintptr_t choice = (uintptr_t)lv_event_get_user_data(e);
    int source = (int)(choice >> 24), index = (int)(choice & 0xffffff);
    const cJSON *entry = cJSON_GetArrayItem(preset_source(source), index);
    if (!entry || (!preset_for_slot && !draft))
        return;
    if (preset_for_slot) {
        cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "preset_source");
        cJSON_AddNumberToObject(slot_draft, "preset_source", source);
        cJSON_DeleteItemFromObjectCaseSensitive(slot_draft, "preset_index");
        cJSON_AddNumberToObject(slot_draft, "preset_index", index);
        slot_preset_selected(entry, source);
        ams_config_show();
        return;
    }
    char code[96], brand[96], material[32], variant[96];
    if (source == 2)
        snprintf(code, sizeof(code), "%d", (int)number(entry, "id", 0));
    else
        snprintf(code, sizeof(code), "%s", str(entry, source == 3 ? "filament_id" : "setting_id"));
    parse_preset(str(entry, "name"), brand, material, variant);
    draft_string("slicer_filament", code);
    draft_string("slicer_filament_name", str(entry, "name"));
    if (material[0])
        draft_string("material", material);
    draft_string("brand", source == 2 && str(entry, "filament_vendor")[0]
                              ? str(entry, "filament_vendor")
                              : brand);
    draft_string("subtype", variant);
    cJSON_Delete(selected_profiles);
    selected_profiles = cJSON_CreateArray();
    close_overlay(NULL);
    draw_page();
}
static void preset_render(void) {
    if (!preset_list)
        return;
    lv_obj_clean(preset_list);
    int row = 0;
    const char *sources[] = {"Bambu Cloud", "Orca Cloud", "Local", "Built-in"};
    for (int source = 0; source < 4; source++) {
        const cJSON *entry;
        int i = 0;
        cJSON_ArrayForEach(entry, preset_source(source)) {
            int index = i++;
            if (!contains(str(entry, "name"), preset_query))
                continue;
            char caption[300];
            snprintf(caption, sizeof(caption), "%s · %s", sources[source], str(entry, "name"));
            button(preset_list, caption, 0, row++ * 44, 476, 40, CARD, preset_choose,
                   (source << 24) | index);
        }
    }
    if (!row)
        text(preset_list,
             bridge_catalogs_loading() ? tr("port.LoadingPresets")
                                       : tr("port.NoMatchingPresets"),
             8, 20, 460, MUTED, &small);
}
static void preset_search(lv_event_t *e) {
    snprintf(preset_query, sizeof(preset_query), "%s",
             lv_textarea_get_text(lv_event_get_target_obj(e)));
    preset_render();
}
static void preset_reload(lv_event_t *e) {
    (void)e;
    bridge_reload_catalogs();
    preset_render();
}
static void preset_open(lv_event_t *e) {
    preset_for_slot = e && (intptr_t)lv_event_get_user_data(e) == 1;
    bridge_load_catalogs();
    preset_query[0] = 0;
    lv_obj_t *panel = modal_panel(tr("inventory.slicerPreset"), 420);
    button(panel, tr("common.refresh"), 330, 10, 120, 34, CARD, preset_reload, 0);
    lv_obj_t *search =
        field(panel, tr("inventory.searchPresets"), 20, 62, 480, 36);
    lv_obj_set_user_data(search, (void *)"preset_search");
    lv_obj_add_event_cb(search, preset_search, LV_EVENT_VALUE_CHANGED, NULL);
    preset_list = rect(panel, 20, 110, 480, 290, SECONDARY, 0);
    lv_obj_add_flag(preset_list, LV_OBJ_FLAG_SCROLLABLE);
    preset_render();
}
static void filament_id(const char *src, char out[96]) {
    size_t n = 0;
    for (size_t i = 0; src[i] && src[i] != '_' && n < 95; i++) {
        if (i == 2 && toupper((unsigned char)src[0]) == 'G' &&
            toupper((unsigned char)src[1]) == 'F' && toupper((unsigned char)src[i]) == 'S')
            continue;
        out[n++] = (char)toupper((unsigned char)src[i]);
    }
    out[n] = 0;
}
static bool pa_matches(const cJSON *cal) {
    if (!str(draft, "material")[0])
        return false;
    char fid[96], calid[96];
    filament_id(str(draft, "slicer_filament"), fid);
    filament_id(str(cal, "filament_id"), calid);
    if (fid[0] && !strcmp(fid, calid)) {
        bool generic = strlen(calid) == 5 && !strcmp(calid + 3, "99");
        if (!generic)
            return true;
        if (!str(draft, "brand")[0] || !strcmp(str(draft, "brand"), "Generic")) {
            const char *codes = "LGBAUCFV";
            const char *types[] = {"PLA", "PETG", "ABS", "ASA", "TPU", "PC", "PA", "PVA"};
            for (int i = 0; i < 8; i++)
                if (calid[2] == codes[i] && !strcmp(str(draft, "material"), types[i]))
                    return true;
        }
    }
    const char *name = str(cal, "name");
    const char *prefix[] = {"High Flow_", "High Flow ", "Standard_", "Standard ",
                            "HF_",        "HF ",        "S_",        "S "};
    for (int i = 0; i < 8; i++)
        if (!strncmp(name, prefix[i], strlen(prefix[i]))) {
            name += strlen(prefix[i]);
            break;
        }
    char brand[96], material[32], variant[96];
    parse_preset(name, brand, material, variant);
    return !strcmp(material, str(draft, "material")) &&
           (!str(draft, "brand")[0] || contains(brand, str(draft, "brand")) ||
            (brand[0] && contains(str(draft, "brand"), brand))) &&
           (!str(draft, "subtype")[0] || contains(variant, str(draft, "subtype")) ||
            contains(name, str(draft, "subtype")));
}
static cJSON *pa_selected(int printer, int extruder) {
    cJSON *p;
    cJSON_ArrayForEach(p, selected_profiles) if (number(p, "printer_id", 0) == printer &&
                                                 number(p, "extruder", 0) == extruder) return p;
    return NULL;
}
static double pa_k(const cJSON *cal) {
    const cJSON *v = prop(cal, "k_value");
    return cJSON_IsString(v) ? strtod(v->valuestring, NULL) : number(cal, "k_value", 0);
}
static void pa_select(const cJSON *cal, int printer, bool automatic) {
    int extruder = (int)number(cal, "extruder_id", 0), idx = (int)number(cal, "slot_id", -1);
    cJSON *old = pa_selected(printer, extruder);
    if (automatic && old && number(old, "k_value", 0) >= pa_k(cal))
        return;
    bool same = old && number(old, "cali_idx", -2) == idx;
    if (old)
        cJSON_Delete(cJSON_DetachItemViaPointer(selected_profiles, old));
    if (same && !automatic)
        return;
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "printer_id", printer);
    cJSON_AddNumberToObject(p, "extruder", extruder);
    cJSON_AddNumberToObject(p, "cali_idx", idx);
    cJSON_AddNumberToObject(p, "k_value", pa_k(cal));
    const char *diameter = str(cal, "nozzle_diameter");
    cJSON_AddStringToObject(p, "nozzle_diameter", diameter[0] ? diameter : "0.4");
    cJSON_AddStringToObject(p, "name", str(cal, "name"));
    cJSON_AddStringToObject(p, "setting_id", str(cal, "setting_id"));
    cJSON_AddItemToArray(selected_profiles, p);
}
static const cJSON *pa_entry(int ordinal, int *printer) {
    const cJSON *p, *cal;
    int row = 0;
    cJSON_ArrayForEach(p, bridge_model("printers")) {
        int id = (int)number(p, "id", 0);
        for (int d = 0; d < 4; d++) {
            char key[32];
            snprintf(key, sizeof(key), "pa_%d_%d", id, d);
            cJSON_ArrayForEach(cal, prop(bridge_model(key), "profiles")) {
                if (!pa_matches(cal))
                    continue;
                if (row++ == ordinal) {
                    *printer = id;
                    return cal;
                }
            }
        }
    }
    return NULL;
}
static void pa_toggle(lv_event_t *e) {
    int printer;
    const cJSON *cal = pa_entry((int)(intptr_t)lv_event_get_user_data(e), &printer);
    if (cal) {
        pa_select(cal, printer, false);
        pa_render();
    }
}
static void pa_auto(lv_event_t *e) {
    (void)e;
    cJSON_Delete(selected_profiles);
    selected_profiles = cJSON_CreateArray();
    int printer;
    const cJSON *cal;
    for (int i = 0; (cal = pa_entry(i, &printer)); i++)
        pa_select(cal, printer, true);
    pa_render();
}
static void pa_render(void) {
    if (!pa_list)
        return;
    int y = lv_obj_get_scroll_y(pa_list);
    lv_obj_clean(pa_list);
    int row = 0;
    const cJSON *p, *cal;
    cJSON_ArrayForEach(p, bridge_model("printers")) {
        int id = (int)number(p, "id", 0);
        char key[32];
        snprintf(key, sizeof(key), "pa_status_%d", id);
        char title[160];
        snprintf(title, sizeof(title), "%s · %s", str(p, "name"),
                 cJSON_IsTrue(prop(bridge_model(key), "connected")) ? tr("settings.connected")
                                                                    : tr("spoolbuddy.status.offline"));
        text(pa_list, title, 4, row * 48 + 8, 392, TEXT, &regular);
        row++;
        int ordinal = 0, printer;
        const cJSON *entry;
        for (; (entry = pa_entry(ordinal, &printer)); ordinal++) {
            if (printer != id)
                continue;
            cal = entry;
            int ext = (int)number(cal, "extruder_id", 0);
            cJSON *selected = pa_selected(id, ext);
            bool chosen =
                selected && number(selected, "cali_idx", -2) == number(cal, "slot_id", -1);
            char caption[256];
            snprintf(caption, sizeof(caption), "%s %s · %s %d · %smm · K=%.3f",
                     chosen ? "[x]" : "[ ]", str(cal, "name"), tr("spoolbuddy.inventory.nozzleTemp"), ext,
                     str(cal, "nozzle_diameter"), pa_k(cal));
            button(pa_list, caption, 0, row++ * 48, 392, 44, chosen ? 0x153c2e : CARD, pa_toggle,
                   ordinal);
        }
    }
    if (!row)
        text(pa_list,
             bridge_catalogs_loading() ? tr("port.LoadingProfiles")
                                       : tr("common.noPrinters"),
             8, 18, 380, MUTED, &small);
    lv_obj_scroll_to_y(pa_list, y, LV_ANIM_OFF);
}
static char native_confirm_method[8], native_confirm_path[192];
static cJSON *native_confirm_body;
static void native_confirm_send(lv_event_t *e) {
    (void)e;
    if (bridge_submit(native_confirm_method, native_confirm_path, native_confirm_body))
        close_overlay(NULL);
}
static void original_confirm(const char *title, const char *method, const char *path,
                             const cJSON *payload) {
    snprintf(native_confirm_method, sizeof(native_confirm_method), "%s", method);
    snprintf(native_confirm_path, sizeof(native_confirm_path), "%s", path);
    cJSON_Delete(native_confirm_body);
    native_confirm_body = cJSON_Duplicate(payload, true);
    lv_obj_t *panel = modal_panel(title, 220);
    text(panel, tr("port.AreYouSure"), 20, 76, 480, TEXT, &regular);
    button(panel, tr("spoolbuddy.writeTag.cancel"), 20, 150, 232, 44, CARD, close_overlay, 0);
    button(panel, tr("spoolbuddy.quickMenu.confirm"), 268, 150, 232, 44, GREEN, native_confirm_send, 0);
}
static void plug_confirm(lv_event_t *e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    char path[96];
    snprintf(path, sizeof(path), "/smart-plugs/%d/control", id);
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "action", "toggle");
    original_confirm(tr("port.TogglePrinterPower"), "POST", path, payload);
    cJSON_Delete(payload);
}
static void quick_render(void) {
    if (!quick_list)
        return;
    lv_obj_clean(quick_list);
    int row = 0;
    const cJSON *printer, *plug;
    cJSON_ArrayForEach(printer, bridge_model("printers")) {
        cJSON_ArrayForEach(plug, bridge_model("plugs")) {
            if (number(plug, "printer_id", 0) != number(printer, "id", -1) ||
                !cJSON_IsTrue(prop(plug, "enabled")) || !strcmp(str(plug, "plug_type"), "mqtt"))
                continue;
            int id = (int)number(plug, "id", 0);
            char key[32], path[96], caption[160];
            snprintf(key, sizeof(key), "plug_%d", id);
            const cJSON *state = bridge_model(key);
            if (!state) {
                snprintf(path, sizeof(path), "/smart-plugs/%d/status", id);
                bridge_lookup(key, path);
            }
            snprintf(caption, sizeof(caption), "%s · %s", str(printer, "name"),
                     state ? str(state, "state") : tr("spoolbuddy.settings.sshKeyLoading"));
            button(quick_list, caption, 0, row++ * 48, 760, 42, CARD, plug_confirm, id);
            break;
        }
    }
    if (!row)
        text(quick_list, tr("port.NoLinkedPowerPlugs"), 8, 12, 744, MUTED,
             &small);
}

static void location_choices(void) {
    if (!location_select)
        return;
    char options[2048];
    snprintf(options, sizeof(options), "%s", tr("inventory.storageLocationNone"));
    const cJSON *entry;
    int i = 0, chosen = 0;
    cJSON_ArrayForEach(entry, bridge_model("locations")) {
        if (strlen(options) + strlen(str(entry, "name")) + 2 >= sizeof(options))
            break;
        strcat(options, "\n");
        strcat(options, str(entry, "name"));
        i++;
        if (number(draft, "location_id", -1) == number(entry, "id", -2))
            chosen = i;
    }
    lv_dropdown_set_options(location_select, options);
    lv_dropdown_set_selected(location_select, chosen);
}
static void location_changed(lv_event_t *e) {
    int selected = lv_dropdown_get_selected(lv_event_get_target_obj(e));
    const cJSON *entry = cJSON_GetArrayItem(bridge_model("locations"), selected - 1);
    cJSON_DeleteItemFromObjectCaseSensitive(draft, "location_id");
    if (entry)
        cJSON_AddNumberToObject(draft, "location_id", number(entry, "id", 0));
    else
        cJSON_AddNullToObject(draft, "location_id");
}
static void core_catalog_choose(lv_event_t *e) {
    const cJSON *entry =
        cJSON_GetArrayItem(bridge_model("spool_catalog"), (int)(intptr_t)lv_event_get_user_data(e));
    if (!entry)
        return;
    cJSON_DeleteItemFromObjectCaseSensitive(draft, "core_weight");
    cJSON_AddNumberToObject(draft, "core_weight", number(entry, "weight", 250));
    cJSON_DeleteItemFromObjectCaseSensitive(draft, "core_weight_catalog_id");
    cJSON_AddNumberToObject(draft, "core_weight_catalog_id", number(entry, "id", 0));
    core_touched = true;
    close_overlay(NULL);
    draw_page();
}
static void core_catalog_open(lv_event_t *e) {
    (void)e;
    lv_obj_t *panel = modal_panel(tr("port.EmptySpoolWeight"), 400);
    lv_obj_t *list = rect(panel, 20, 60, 480, 320, SECONDARY, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    int row = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, bridge_model("spool_catalog")) {
        char caption[192];
        snprintf(caption, sizeof(caption), "%s · %.0fg", str(entry, "name"),
                 number(entry, "weight", 0));
        button(list, caption, 0, row * 44, 476, 40, CARD, core_catalog_choose, row);
        row++;
    }
    if (!row)
        text(list, tr("port.NoCatalogEntriesAvailable"), 8, 20, 460, MUTED,
             &small);
}
