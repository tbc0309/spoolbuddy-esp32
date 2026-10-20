// Optional blanket confirmation UI; not part of upstream's normal direct-action flow.
#include "confirm_actions.h"
#include "bridge.h"
#include <stdlib.h>
#include <stdio.h>
typedef struct {
    char method[8], path[1536];
    cJSON *body;
    void (*close)(void);
    void (*sent)(const char *);
} confirmation_t;
static void deleted(lv_event_t *e) {
    confirmation_t *c = lv_event_get_user_data(e);
    cJSON_Delete(c->body);
    free(c);
}
static void cancel(lv_event_t *e) {
    confirmation_t *c = lv_event_get_user_data(e);
    c->close();
}
static void apply(lv_event_t *e) {
    confirmation_t *c = lv_event_get_user_data(e);
    if (bridge_submit(c->method, c->path, c->body)) {
        char path[1536];
        snprintf(path, sizeof(path), "%s", c->path);
        c->sent(path);
    }
}
void sb_confirm_open(lv_obj_t *panel, const char *method, const char *path, const cJSON *body,
                     bool chinese, void (*close)(void), void (*sent)(const char *)) {
    confirmation_t *c = calloc(1, sizeof(*c));
    if (!c) {
        close();
        return;
    }
    snprintf(c->method, sizeof(c->method), "%s", method);
    snprintf(c->path, sizeof(c->path), "%s", path);
    c->body = body ? cJSON_Duplicate(body, true) : NULL;
    c->close = close;
    c->sent = sent;
    lv_obj_add_event_cb(panel, deleted, LV_EVENT_DELETE, c);
    lv_obj_t *text = lv_label_create(panel);
    lv_label_set_text(text, chinese ? "确认执行此修改？" : "Confirm this change?");
    lv_obj_set_pos(text, 20, 80);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *button = lv_button_create(panel);
        lv_obj_set_pos(button, i ? 270 : 20, 158);
        lv_obj_set_size(button, 230, 48);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label,
                          i ? (chinese ? "确认" : "Confirm") : (chinese ? "取消" : "Cancel"));
        lv_obj_center(label);
        lv_obj_add_event_cb(button, i ? apply : cancel, LV_EVENT_CLICKED, c);
    }
}
