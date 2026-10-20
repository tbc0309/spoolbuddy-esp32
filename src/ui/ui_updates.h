// SPDX-License-Identifier: AGPL-3.0-or-later
/* Project OTA renderer, checking and installing newer OTA releases in one action. */
#ifndef SB_FIRMWARE_BUILD
#define SB_FIRMWARE_BUILD 1
#endif
static const char *update_caption(sb_update_phase phase) {
    switch(phase) {
    case SB_UPDATE_CHECKING:return tr("port.CheckingForUpdates");
    case SB_UPDATE_READY:return tr("spoolbuddy.settings.updateAvailable");
    case SB_UPDATE_CURRENT:return tr("spoolbuddy.settings.upToDate");
    case SB_UPDATE_EMPTY:return tr("port.NoReleasePublishedForThisBoard");
    case SB_UPDATE_DOWNLOADING:return tr("port.DownloadingFirmware");
    case SB_UPDATE_VERIFYING:return tr("port.VerifyingFirmware");
    case SB_UPDATE_REBOOTING:return tr("port.UpdateVerifiedRestarting");
    case SB_UPDATE_ERROR:return tr("spoolbuddy.settings.updateFailed");
    default:return "";
    }
}
static void update_settings(lv_obj_t *content) {
    sb_update_state state;sb_update_snapshot(&state);
    bool busy=state.phase==SB_UPDATE_CHECKING || state.phase==SB_UPDATE_DOWNLOADING ||
              state.phase==SB_UPDATE_VERIFYING || state.phase==SB_UPDATE_REBOOTING;
    bool progress=state.phase==SB_UPDATE_DOWNLOADING || state.phase==SB_UPDATE_VERIFYING ||
                  state.phase==SB_UPDATE_REBOOTING;
    lv_obj_t *versions=rect(content,0,0,759,96,CARD,8);
    text(versions,tr("spoolbuddy.settings.currentVersion"),12,12,260,MUTED,&small);
    lv_obj_t *v=text(versions,SB_FIRMWARE_VERSION,410,12,337,TEXT,&regular);
    lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);
    int y=44;
    if(state.phase!=SB_UPDATE_IDLE) {
        text(versions,update_caption(state.phase),12,y,734,
             state.phase==SB_UPDATE_ERROR?0xf87171:busy?0x86efac:TEXT,&small);
        y+=28;
    }
    if(state.artifact && state.version[0] && state.phase!=SB_UPDATE_CURRENT) {
        char target[112];
        snprintf(target,sizeof(target),"%s: %s",tr("spoolbuddy.settings.updateAvailable"),state.version);
        text(versions,target,12,y,734,0x86efac,&small);y+=28;
    }
    if(progress) {
        char value[16];int percent=state.progress<0?0:state.progress>100?100:state.progress;
        system_usage(versions,percent,12,y+8,672);
        snprintf(value,sizeof(value),"%d%%",percent);text(versions,value,694,y+1,52,TEXT,&small);
        y+=28;
    } else if(state.message[0]) {
        char detail[192];const char *key=sb_i18n_key(state.message);
        const char *message=key?tr(key):state.message;
        if(!strncmp(state.message,"OTA failed: ",12)) {
            snprintf(detail,sizeof(detail),"%s: %s",tr("spoolbuddy.settings.updateFailed"),state.message+12);message=detail;
        } else if(!strncmp(state.message,"HTTPS request failed: ",22)) {
            snprintf(detail,sizeof(detail),"%s: %s",tr("port.HttpsRequestFailed"),state.message+22);message=detail;
        } else {
            int http_code=0;
            if(sscanf(state.message,"HTTP %d (private repository needs OTA credentials)",&http_code)==1) {
                snprintf(detail,sizeof(detail),"HTTP %d · %s",http_code,tr("port.PrivateOtaNeedsCredentials"));message=detail;
            }
        }
        lv_obj_t *label=text(versions,message,12,y,734,MUTED,&small);
        lv_obj_set_height(label,40);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);y+=48;
    }
    if(!busy) {
        button(versions,tr("spoolbuddy.settings.checkUpdates"),12,y,363,40,0x3f3f46,action_cb,ACT_CHECK_UPDATE);
        button(versions,tr("port.UpdateFromTfCard"),383,y,364,40,0x3f3f46,action_cb,ACT_UPDATE_SD);
        y+=40;
    }
    lv_obj_set_height(versions,y+12);
}
