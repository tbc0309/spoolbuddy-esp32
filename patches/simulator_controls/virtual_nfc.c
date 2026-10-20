// SPDX-License-Identifier: AGPL-3.0-or-later
#include <windows.h>
#include "bridge.h"
#include "ui_live.h"
#include "ui.h"
#include "ui_original.h"
void sb_virtual_nfc_key(unsigned w) {
    if (sb_live.enabled && w == VK_F3) {
        static int virtual_index;
        const cJSON *inventory = bridge_model("inventory");
        int count = cJSON_GetArraySize(inventory);
        if (count) {
            const cJSON *s = cJSON_GetArrayItem(inventory, virtual_index++ % count);
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(s, "id");
            const cJSON *uid = cJSON_GetObjectItemCaseSensitive(s, "tag_uid");
            const cJSON *brand = cJSON_GetObjectItemCaseSensitive(s, "brand");
            const cJSON *material = cJSON_GetObjectItemCaseSensitive(s, "material");
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(s, "label_weight");
            const cJSON *used = cJSON_GetObjectItemCaseSensitive(s, "weight_used");
            const cJSON *core = cJSON_GetObjectItemCaseSensitive(s, "core_weight");
            ui_tag(cJSON_IsString(uid) && uid->valuestring[0] ? uid->valuestring : "SIMULATED",
                   "Virtual NFC");
            ui_spool(sb_live.uid, cJSON_IsNumber(id) ? id->valueint : 0,
                     cJSON_IsString(brand) ? brand->valuestring : "",
                     cJSON_IsString(material) ? material->valuestring : "",
                     cJSON_IsNumber(label) && cJSON_IsNumber(used)
                         ? label->valuedouble - used->valuedouble
                         : 0,
                     cJSON_IsNumber(core) ? core->valuedouble : 250);
            original_page(0);
            original_status("Virtual NFC input (F4 removes tag)");
        }
    }
    if (sb_live.enabled && w == VK_F4)
        ui_tag("", "Virtual tag removed");
}
