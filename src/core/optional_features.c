// No local feature patches in the default upstream-parity build.
#include "ui.h"
#include "ui_live.h"
bool ui_manual_active(void) { return false; }
bool ui_take_manual(float *grams) {
    if (grams)
        *grams = 0;
    return false;
}
bool ui_manual_value(float *grams) {
    if (grams)
        *grams = 0;
    return false;
}
void ui_manual_set(float grams) { (void)grams; }
void ui_manual_clear(void) {}

#include "optional_inputs.h"
void sb_optional_inputs_poll(int64_t now_us, sb_optional_weight_sink sink, void *context) {
    (void)now_us;
    (void)sink;
    (void)context;
}
