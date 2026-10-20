// SPDX-License-Identifier: AGPL-3.0-or-later
/* Native LVGL reconstruction of upstream components/spoolbuddy and pages/spoolbuddy.
 * The React app remains the visual reference; no browser or host renderer is required. */
#include "screens.h"
#include "ui.h"
#include "ui_platform.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
extern const lv_font_t sb_font_20;
static lv_font_t font;
#define MAX_SPOOLS 96
#define MAX_PRINTERS 8
#define MAX_SLOTS 32
typedef struct { int id; char brand[64], material[40], color[64], uid[80]; float remaining, core;
                 unsigned rgba; } spool_t;
typedef struct { int id; char name[64]; } printer_t;
typedef struct { int ams, tray, remain; char material[40]; unsigned color; } slot_t;
static spool_t spools[MAX_SPOOLS];
static printer_t printers[MAX_PRINTERS];
static slot_t slots[MAX_SLOTS];
static int nspools, total_spools, nprinters, nslots, printer_id, selected_id, current_page;
static bool zh, online, loaded, request_refresh = true, printer_connected;
static uint32_t next_poll;
static lv_obj_t *page, *nav[5], *nav_labels[5], *printer_selector, *online_label, *stats;
static lv_obj_t *confirm;
static char printer_options[600], model_error[120];
typedef struct { int type, spool, printer, ams, tray; } action_t;
static action_t pending, proposed;
enum { WRITE = 1, CANCEL_WRITE, ASSIGN };
static const char *tr(const char *en, const char *cn) { return zh ? cn : en; }
static const cJSON *item(const cJSON *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o,k); }
static const char *str(const cJSON *o,const char *k) {
    const cJSON *v=item(o,k); return cJSON_IsString(v) ? v->valuestring : "";
}
static double num(const cJSON *o,const char *k,double fallback) {
    const cJSON *v=item(o,k); return cJSON_IsNumber(v) ? v->valuedouble : fallback;
}
static unsigned color(const char *s) { return strlen(s)>=6 ? strtoul(s,NULL,16) >> (strlen(s)>=8?8:0) : 0x808080; }
static lv_obj_t *label(lv_obj_t *p,const char *v,int x,int y,int w) {
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_label_set_text(o,v); lv_obj_set_style_text_line_space(o,-8,0); return o;
}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,unsigned bg) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_color(o,lv_color_hex(0xdddddd),0); lv_obj_set_style_text_font(o,&font,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(bg),0); lv_obj_set_style_border_color(o,lv_color_hex(0x363e46),0);
    lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_pad_all(o,0,0); lv_obj_set_style_radius(o,10,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t *button(lv_obj_t *p,const char *s,int x,int y,int w,int h,lv_event_cb_t cb,int id) {
    lv_obj_t *o=lv_button_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x2b2b2b),0); lv_obj_set_style_radius(o,7,0);
    lv_obj_set_style_text_font(o,&font,0); lv_obj_set_style_text_color(o,lv_color_hex(0xeeeeee),0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void *)(intptr_t)id);
    lv_obj_t *t=lv_label_create(o); lv_label_set_text(t,s); lv_obj_center(t); return o;
}
static spool_t *selected(void) {
    for(int i=0;i<nspools;i++) if(spools[i].id==selected_id) return &spools[i];
    return NULL;
}
static void render(void);
static void refresh_cb(lv_event_t *e) { (void)e; request_refresh=true; }
static void confirm_close(lv_event_t *e) { (void)e; if(confirm) lv_obj_delete(confirm); confirm=NULL; }
static void confirm_apply(lv_event_t *e) {
    (void)e;
    if (!online) { ui_status("Connect before saving weight"); confirm_close(NULL); return; }
    if (pending.type) { ui_status("Request pending"); return; }
    pending=proposed; confirm_close(NULL); ui_status("Request queued");
}
static void propose(action_t a) {
    if(confirm) return;
    proposed=a;
    confirm=box(lv_screen_active(),130,95,540,275,0x262c32);
    label(confirm,a.type==WRITE ? tr("Write NFC tag?","写入 NFC 标签？") :
          tr("Assign spool to AMS slot?","将耗材分配到 AMS 槽位？"),20,16,500);
    char message[220];
    if(a.type==WRITE) snprintf(message,sizeof(message),"%s #%d\n%s",tr("Spool","耗材"),a.spool,
                               tr("Use a blank NTAG. This changes the tag.","请使用空白 NTAG，此操作会修改标签。"));
    else snprintf(message,sizeof(message),"%s #%d  →  AMS %d / %d\n%s",tr("Spool","耗材"),a.spool,a.ams,a.tray+1,
                  tr("This changes the inventory assignment.","此操作会修改库存关联。"));
    label(confirm,message,20,75,500);
    button(confirm,tr("Cancel","取消"),20,204,235,50,confirm_close,0);
    lv_obj_t *ok=button(confirm,tr("Confirm","确认"),280,204,235,50,confirm_apply,0);
    lv_obj_set_style_bg_color(ok,lv_color_hex(0x00ae42),0);
}
static void select_cb(lv_event_t *e) {
    selected_id=(int)(intptr_t)lv_event_get_user_data(e);
    spool_t *s=selected();
    if(s && current_page==3) {
        ui_select_inventory(s->uid,s->id,s->brand,s->material,s->remaining,s->core);
        screens_dashboard();
    } else render();
}
static void write_cb(lv_event_t *e) {
    (void)e;
    if(!selected()) { ui_status("Select a spool first"); return; }
    propose((action_t){.type=WRITE,.spool=selected_id});
}
static void cancel_write_cb(lv_event_t *e) {
    (void)e; if(!pending.type) pending=(action_t){.type=CANCEL_WRITE};
}
static void assign_cb(lv_event_t *e) {
    int i=(int)(intptr_t)lv_event_get_user_data(e);
    if(!selected()) { ui_status("Select a spool in Inventory first"); return; }
    if(i<0 || i>=nslots) return;
    propose((action_t){.type=ASSIGN,.spool=selected_id,.printer=printer_id,.ams=slots[i].ams,.tray=slots[i].tray});
}
static void printer_cb(lv_event_t *e) {
    (void)e; unsigned i=lv_dropdown_get_selected(printer_selector);
    if(i<(unsigned)nprinters) printer_id=printers[i].id;
    request_refresh=true; nslots=0; render();
}
static void nav_cb(lv_event_t *e) {
    current_page=(int)(intptr_t)lv_event_get_user_data(e); request_refresh=true; render();
}
static void brightness_cb(lv_event_t *e) {
    int brightness=lv_slider_get_value(lv_event_get_target_obj(e));
    ui_local_brightness(brightness);
}
static void language_cb(lv_event_t *e) { (void)e; ui_language(!zh); }
static void tare_cb(lv_event_t *e) { (void)e;
#ifdef SB_SIMULATOR
    ui_test_click(tr("Tare / Zero","电子秤去皮"));
#else
    extern void ui_request_tare(void); ui_request_tare();
#endif
}
static void list_spools(lv_obj_t *p,int x,int y,int w,int h) {
    lv_obj_t *list=box(p,x,y,w,h,0x181818); lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);
    if(!loaded) label(list,model_error[0]?model_error:tr("Loading inventory...","正在读取库存…"),15,15,w-30);
    if(loaded && !nspools) label(list,tr("No spools in inventory","库存中暂无耗材"),15,15,w-30);
    for(int i=0;i<nspools;i++) {
        spool_t *s=&spools[i]; char row[240];
        snprintf(row,sizeof(row),"#%d   %s  %s\n%s   %.0f g",s->id,s->brand,s->material,s->color,(double)s->remaining);
        lv_obj_t *b=button(list,row,8,8+i*84,w-30,76,select_cb,s->id);
        lv_obj_set_style_bg_color(b,lv_color_hex(s->id==selected_id?0x144329:0x262c32),0);
        lv_obj_t *swatch=box(b,4,6,7,55,s->rgba); lv_obj_set_style_border_width(swatch,0,0);
        lv_obj_t *caption=lv_obj_get_child(b,0); lv_obj_set_width(caption,w-68);
        lv_obj_set_pos(caption,18,-6); lv_obj_set_style_text_line_space(caption,-8,0);
    }
}
static void render(void) {
    if(!page) return;
    lv_obj_clean(page);
    if(current_page==0) lv_obj_add_flag(page,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(page,LV_OBJ_FLAG_HIDDEN);
    const char *en[]={"Dashboard","AMS","Write","Inventory","Settings"};
    const char *cn[]={"首页","AMS","写卡","库存","设置"};
    const char *icons[]={LV_SYMBOL_HOME,LV_SYMBOL_LIST,LV_SYMBOL_WIFI,LV_SYMBOL_DIRECTORY,LV_SYMBOL_SETTINGS};
    for(int i=0;i<5;i++) {
        char caption[100]; snprintf(caption,sizeof(caption),"%s  %s",icons[i],zh?cn[i]:en[i]);
        lv_label_set_text(nav_labels[i],caption); lv_obj_center(nav_labels[i]);
        lv_obj_set_style_text_color(nav[i],lv_color_hex(current_page==i?0x00ae42:0x888888),0);
        lv_obj_set_style_bg_color(nav[i],lv_color_hex(current_page==i?0x181c20:0x262c32),0);
    }
    lv_label_set_text(online_label,online?tr("Online","在线"):tr("Offline","离线"));
    lv_obj_set_style_text_color(online_label,lv_color_hex(online?0x00ae42:0xaaaaaa),0);
    int materials=0,brands=0;
    for(int i=0;i<nspools;i++) {
        bool new_material=true,new_brand=true;
        for(int j=0;j<i;j++) { if(!strcmp(spools[i].material,spools[j].material)) new_material=false;
            if(!strcmp(spools[i].brand,spools[j].brand)) new_brand=false; }
        materials+=new_material; brands+=new_brand;
    }
    char summary[160]; snprintf(summary,sizeof(summary),"%d %s     |     %d %s     |     %d %s",total_spools,tr("spools","卷耗材"),materials,tr("Material","种材质"),brands,tr("Brand","个品牌"));
    lv_label_set_text(stats,summary);
    if(current_page==1) {
        label(page,tr("AMS · Filament slots","AMS · 耗材槽位"),16,10,480);
        button(page,tr("Refresh","刷新"),650,8,130,42,refresh_cb,0);
        if(!nslots) label(page,tr("No AMS data for this printer","此打印机暂无 AMS 数据"),24,90,740);
        for(int i=0;i<nslots;i++) {
            int col=i%4,row=i/4; slot_t *s=&slots[i]; char title[180];
            char remaining[40], unit[32];
            if(s->ams>=0 && s->ams<26) snprintf(unit,sizeof(unit),"%c",'A'+s->ams);
            else snprintf(unit,sizeof(unit),"%d",s->ams);
            if(s->remain<0) snprintf(remaining,sizeof(remaining),"%s",tr("Unknown","未知"));
            else snprintf(remaining,sizeof(remaining),"%d%%",s->remain);
            snprintf(title,sizeof(title),"AMS %s · %d\n%s\n%s %s",unit,s->tray+1,s->material,
                     tr("Remaining","剩余"),remaining);
            lv_obj_t *b=button(page,title,16+col*194,62+row*180,182,166,assign_cb,i);
            lv_obj_set_style_border_width(b,3,0); lv_obj_set_style_border_color(b,lv_color_hex(s->color),0);
            lv_obj_set_style_text_line_space(lv_obj_get_child(b,0),-6,0);
        }
        lv_obj_add_flag(page,LV_OBJ_FLAG_SCROLLABLE);
    } else if(current_page==2) {
        label(page,tr("Write NFC tag · Existing spool","写入 NFC 标签 · 现有耗材"),16,8,760);
        list_spools(page,16,52,470,270);
        spool_t *s=selected(); char details[240];
        if(s) snprintf(details,sizeof(details),"#%d\n%s %s\n%.0f g",s->id,s->brand,s->material,(double)s->remaining);
        else snprintf(details,sizeof(details),"%s",tr("Select an existing spool","选择现有耗材"));
        label(page,details,510,54,270);
        button(page,tr("Write tag","写入标签"),510,198,270,52,write_cb,0);
        button(page,tr("Cancel write","取消写入"),510,266,270,48,cancel_write_cb,0);
    } else if(current_page==3) {
        char heading[100]; snprintf(heading,sizeof(heading),"%s · %d%s",tr("Inventory","耗材库存"),total_spools,
                                    total_spools>MAX_SPOOLS?tr(" (first 96)","（显示前 96 卷）"):"");
        label(page,heading,16,8,600); button(page,tr("Refresh","刷新"),650,8,130,42,refresh_cb,0);
        list_spools(page,16,56,764,272);
    } else if(current_page==4) {
        label(page,tr("Device","设备"),16,8,760);
        lv_obj_t *a=box(page,16,52,370,112,0x262c32);
        label(a,"PN5180 · SPI\nNAU7802 · I2C\nESP32-P4 Rev3.x",16,8,338);
        lv_obj_t *b=box(page,402,52,380,112,0x262c32);
        label(b,tr("Wi-Fi / Backend: SD configuration\n/sdcard/spoolbuddy.json\nRestart after editing the file",
                   "Wi-Fi / 后端：SD 卡配置\n/sdcard/spoolbuddy.json\n修改配置文件后重启"),12,8,356);
        label(page,tr("Brightness (current session)","屏幕亮度（本次运行）"),16,180,420);
        lv_obj_t *slider=lv_slider_create(page); lv_obj_set_pos(slider,20,233); lv_obj_set_size(slider,350,16);
        lv_slider_set_range(slider,10,100); lv_slider_set_value(slider,ui_brightness_value(),LV_ANIM_OFF);
        lv_obj_add_event_cb(slider,brightness_cb,LV_EVENT_VALUE_CHANGED,NULL);
        button(page,zh?"English":"简体中文",414,188,170,50,language_cb,0);
        button(page,tr("Tare / Zero","电子秤去皮"),600,188,180,50,tare_cb,0);
        label(page,sb_settings.backend_url,16,272,764);
    }
}
void screens_init(void) {
    zh=sb_settings.chinese; font=sb_font_20; font.fallback=&lv_font_montserrat_20;
    lv_obj_t *screen=lv_screen_active();
    lv_obj_t *statistics=box(screen,16,53,768,35,0x202428);
    lv_obj_set_style_radius(statistics,7,0);
    stats=label(statistics,"",10,-2,744);
    lv_obj_set_style_text_color(stats,lv_color_hex(0x888888),0);
    printer_selector=lv_dropdown_create(screen); lv_obj_set_pos(printer_selector,240,3); lv_obj_set_size(printer_selector,320,42);
    lv_obj_set_style_text_font(printer_selector,&font,0); lv_obj_set_style_bg_color(printer_selector,lv_color_hex(0x181c20),0);
    lv_obj_set_style_text_color(printer_selector,lv_color_hex(0xdddddd),0);
    lv_dropdown_set_options(printer_selector,tr("No printers online","暂无在线打印机"));
    lv_obj_add_event_cb(printer_selector,printer_cb,LV_EVENT_VALUE_CHANGED,NULL);
    online_label=label(screen,"",660,6,120);
    page=box(screen,0,48,800,340,0x181c20); lv_obj_set_style_border_width(page,0,0); lv_obj_set_style_radius(page,0,0);
    for(int i=0;i<5;i++) {
        nav[i]=button(screen,"",i*160,424,160,56,nav_cb,i);
        lv_obj_set_style_radius(nav[i],0,0); nav_labels[i]=lv_obj_get_child(nav[i],0);
    }
    render();
}
void screens_language(bool chinese) { zh=chinese; render(); }
void screens_online(bool value) { sb_display_lock(); if(online!=value) { online=value; render(); } sb_display_unlock(); }
void screens_dashboard(void) { current_page=0; render(); }
void screens_test_page(int value) { sb_display_lock(); current_page=value; render(); sb_display_unlock(); }
static void parse_spools(const cJSON *data) {
    nspools=0; total_spools=cJSON_GetArraySize(data);
    const cJSON *o;
    cJSON_ArrayForEach(o,data) {
        if(nspools>=MAX_SPOOLS) break;
        spool_t *s=&spools[nspools++]; memset(s,0,sizeof(*s)); s->id=num(o,"id",0);
        snprintf(s->brand,sizeof(s->brand),"%s",str(o,"brand"));
        snprintf(s->material,sizeof(s->material),"%s",str(o,"material"));
        snprintf(s->color,sizeof(s->color),"%s",str(o,"color_name"));
        snprintf(s->uid,sizeof(s->uid),"%s",str(o,"tag_uid"));
        s->rgba=color(str(o,"rgba")); s->core=num(o,"core_weight",250);
        s->remaining=fmax(0,num(o,"label_weight",1000)-num(o,"weight_used",0));
    }
    loaded=true; model_error[0]=0;
}
void screens_demo(void) {
    sb_display_lock();
    spools[0]=(spool_t){.id=7,.brand="Demo / 示例",.material="PLA",.color="Green / 绿色",.rgba=0x00ae42,.core=250,.remaining=730};
    nspools=total_spools=1; loaded=true; render(); sb_display_unlock();
}
void screens_network(sb_get_fn get,sb_post_fn post,const char *device,bool force) {
    action_t a;
    sb_display_lock(); a=pending; pending.type=0; bool refresh=request_refresh;
    request_refresh=false; int chosen=printer_id; sb_display_unlock();
    if(a.type) {
        cJSON *body=cJSON_CreateObject(), *r=NULL; char path[160];
        if(a.type==WRITE) { cJSON_AddStringToObject(body,"device_id",device); cJSON_AddNumberToObject(body,"spool_id",a.spool);
            r=post("/spoolbuddy/nfc/write-tag",body); }
        if(a.type==CANCEL_WRITE) { snprintf(path,sizeof(path),"/spoolbuddy/devices/%s/cancel-write",device); r=post(path,body); }
        if(a.type==ASSIGN) {
            cJSON_AddNumberToObject(body,"spool_id",a.spool); cJSON_AddNumberToObject(body,"printer_id",a.printer);
            cJSON_AddNumberToObject(body,"ams_id",a.ams); cJSON_AddNumberToObject(body,"tray_id",a.tray);
            r=post("/inventory/assignments",body);
        }
        ui_status(r ? (a.type==WRITE?"Place an NTAG sticker to write":"Request completed") : "Request failed");
        cJSON_Delete(body); cJSON_Delete(r); refresh=true;
    }
    uint32_t now=lv_tick_get();
    if(!force&&!refresh&&(int32_t)(now-next_poll)<0) return;
    next_poll=now+10000;
    cJSON *inventory=get("/inventory/spools?include_archived=false");
    cJSON *ps=get("/printers/");
    /* Parse only the safe fields required by the UI; never display/store printer credentials. */
    sb_display_lock();
    if(cJSON_IsArray(inventory)) parse_spools(inventory);
    else snprintf(model_error,sizeof(model_error),"%s",tr("Inventory unavailable; check API access","库存读取失败，请检查 API 权限"));
    if(cJSON_IsArray(ps)) {
        nprinters=0; printer_options[0]=0; const cJSON *p;
        cJSON_ArrayForEach(p,ps) {
            if(nprinters>=MAX_PRINTERS) break;
            printer_t *q=&printers[nprinters]; q->id=num(p,"id",0);
            snprintf(q->name,sizeof(q->name),"%.60s",str(p,"name"));
            if(nprinters) { strcat(printer_options,"\n"); }
            strcat(printer_options,q->name); nprinters++;
        }
        bool exists=false;
        for(int i=0;i<nprinters;i++) if(printers[i].id==chosen) exists=true;
        if(!exists) chosen=nprinters?printers[0].id:0;
        printer_id=chosen;
        lv_dropdown_set_options(printer_selector,nprinters?printer_options:tr("No printers","暂无打印机"));
        for(int i=0;i<nprinters;i++) if(printers[i].id==chosen) lv_dropdown_set_selected(printer_selector,i);
    }
    sb_display_unlock(); cJSON_Delete(inventory); cJSON_Delete(ps);
    cJSON *status=NULL; char path[100];
    if(chosen) { snprintf(path,sizeof(path),"/printers/%d/status",chosen); status=get(path); }
    sb_display_lock();
    if(chosen==printer_id) {
        nslots=0; printer_connected=cJSON_IsTrue(item(status,"connected"));
        const cJSON *ams,*tray;
        cJSON_ArrayForEach(ams,item(status,"ams")) cJSON_ArrayForEach(tray,item(ams,"tray")) {
            if(nslots>=MAX_SLOTS) break;
            slot_t *s=&slots[nslots++]; s->ams=num(ams,"id",0); s->tray=num(tray,"id",0);
            s->remain=num(tray,"remain",0); s->color=color(str(tray,"tray_color"));
            snprintf(s->material,sizeof(s->material),"%s",str(tray,"tray_type"));
        }
    }
    render(); sb_display_unlock(); cJSON_Delete(status);
}
