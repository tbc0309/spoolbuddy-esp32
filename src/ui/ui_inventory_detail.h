// SPDX-License-Identifier: AGPL-3.0-or-later
// Original InventorySpoolInfoCard data and action placement.
static void detail_values(void) {
    if (!detail_remaining)
        return;
    const cJSON *s = spool_by_id(detail_spool);
    if (!s)
        return;
    double core = number(s, "core_weight", 250);
    if (core <= 0)
        core = 250;
    double inventory = fmax(0, number(s, "label_weight", 1000) - number(s, "weight_used", 0));
    double remaining =
        valid_weight() && round(gross()) >= 10 ? fmax(0, round(gross()) - core) : inventory;
    double label = number(s, "label_weight", 1000);
    char value[256];
    snprintf(value, sizeof(value),
             tr("port.Remaining0fg0fg0f"),
             remaining, label, label > 0 ? fmin(100, 100 * remaining / label) : 0);
    lv_label_set_text(detail_remaining, value);
    if (valid_weight())
        snprintf(
            value, sizeof(value),
            tr("port.LiveScale0fgDifference0fg"),
            round(gross()), round(gross()) - (inventory + core));
    else
        snprintf(value, sizeof(value), "%s", tr("port.LiveScaleUnavailable"));
    lv_label_set_text(detail_scale, value);
    const cJSON *profiles = prop(s, "k_profiles");
    if (!cJSON_GetArraySize(profiles))
        profiles = bridge_model("detail_profiles");
    snprintf(value, sizeof(value), "%s", tr("port.KProfile_c178e5"));
    const cJSON *p;
    cJSON_ArrayForEach(p, profiles) {
        size_t used = strlen(value);
        if (used + 32 >= sizeof(value))
            break;
        snprintf(value + used, sizeof(value) - used, "%.3f (%smm) ", number(p, "k_value", 0),
                 str(p, "nozzle_diameter"));
    }
    lv_label_set_text(detail_profiles, value);
}
static void detail_show(int id) {
    const cJSON *s = spool_by_id(id);
    if (!s)
        return;
    selected_spool = detail_spool = id;
    lv_obj_t *panel = modal_panel(tr("port.SpoolDetails"), 440);
    lv_obj_t *card = rect(panel, 16, 58, 488, 364, SECONDARY, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    spool(card, 12, 4, 80, spool_color(s));
    char value[256];
    snprintf(value, sizeof(value), "%s  #%d", str(s, "color_name"), id);
    text(card, value, 112, 8, 352, TEXT, &heading);
    snprintf(value, sizeof(value), "%s %s %s", str(s, "brand"), str(s, "material"),
             str(s, "subtype"));
    text(card, value, 112, 42, 352, MUTED, &regular);
    detail_remaining = text(card, "", 12, 102, 460, TEXT, &regular);
    detail_scale = text(card, "", 12, 136, 460, MUTED, &small);
    double core = number(s, "core_weight", 250);
    double net = fmax(0, number(s, "label_weight", 0) - number(s, "weight_used", 0));
    snprintf(value, sizeof(value),
             tr("port.Gross0fgEmptySpool0fg"),
             number(s, "last_scale_weight", net + core), core);
    text(card, value, 12, 172, 460, TEXT, &regular);
    snprintf(value, sizeof(value),
             tr("port.Used0fgCostKg2f"),
             number(s, "weight_used", 0), number(s, "cost_per_kg", 0));
    text(card, value, 12, 206, 460, MUTED, &regular);
    detail_profiles = text(card, "", 12, 240, 460, MUTED, &small);
    snprintf(value, sizeof(value), "%s: %s", tr("spoolbuddy.inventory.slicerFilament"),
             str(s, "slicer_filament_name")[0] ? str(s, "slicer_filament_name")
                                               : str(s, "slicer_filament"));
    text(card, value, 12, 274, 460, TEXT, &small);
    snprintf(value, sizeof(value), "%s: %s", tr("spoolbuddy.inventory.note"), str(s, "note"));
    text(card, value, 12, 330, 460, MUTED, &small);
    const cJSON *a = assignment(id);
    lv_obj_t *assign =
        button(card, a ? tr("port.AssignedToAms") : tr("spoolbuddy.modal.assignToAms"),
               12, 394, 220, 42, GREEN, action_cb, ACT_ASSIGN);
    if (a)
        lv_obj_add_state(assign, LV_STATE_DISABLED);
    lv_obj_t *sync =
        button(card, tr("spoolbuddy.modal.syncWeight"), 252, 394, 220, 42, CARD, action_cb, ACT_SYNC);
    if (!valid_weight())
        lv_obj_add_state(sync, LV_STATE_DISABLED);
    button(card, tr("spoolbuddy.dashboard.close"), 12, 448, 220, 42, CARD, close_overlay, 0);
    if (a && strstr(bridge_inventory(), "spoolman"))
        button(card, tr("port.UnassignAms"), 252, 448, 220, 42, CARD, action_cb, 500);
#ifdef SB_ENABLE_MANUAL_WEIGHT
    button(card, tr("port.ManualInput"), 12, 502, 460, 42, CARD, action_cb, 0);
#endif
    char path[160];
    snprintf(path, sizeof(path), "%s/%d/k-profiles", bridge_inventory(), id);
    bridge_lookup("detail_profiles", path);
    detail_values();
}
