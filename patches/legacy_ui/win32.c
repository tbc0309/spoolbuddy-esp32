// SPDX-License-Identifier: AGPL-3.0-or-later
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <winhttp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "ui.h"
#include "ui_platform.h"
#include "settings.h"
#include "screens.h"
#include "cJSON.h"

static HWND window, scale_edit, config_label, load_button;
static CRITICAL_SECTION display_lock, action_lock;
static uint32_t frame[480 * 800], draw[480 * 800];
static BITMAPINFO bitmap;
static float window_scale = 1.0f;
static HFONT controls_font;
static int mouse_x, mouse_y;
static bool pressed, hidden, live, tag_present, registered;
static volatile LONG running = 1;
static unsigned commands;
static float requested_scale = 980, scale_value = 980;
static char selected_config[MAX_PATH], last_write[2048];
static int last_write_id;
static int last_scan_id;
static uint8_t virtual_tag[872];
static float demo_remaining = 730;
static char uid[21] = "04A1B2C3D4E5F6";
enum { SCAN = 1, REMOVE = 2, SCALE = 4, CONNECT = 8, DEMO = 16 };
static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = calloc(n, sizeof(wchar_t));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
static void enqueue(unsigned action) {
    EnterCriticalSection(&action_lock);
    commands |= action;
    LeaveCriticalSection(&action_lock);
}
void sb_display_lock(void) { EnterCriticalSection(&display_lock); }
void sb_display_unlock(void) { LeaveCriticalSection(&display_lock); }
void sb_display_brightness(int p) { (void)p; }
static uint32_t ticks(void) { return (uint32_t)GetTickCount64(); }
static void flush(lv_display_t *d, const lv_area_t *area, uint8_t *pixels) {
    int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++)
        memcpy(frame + y * 800 + area->x1, pixels + (y - area->y1) * w * 4, w * 4);
    lv_display_flush_ready(d);
    InvalidateRect(window, NULL, FALSE);
}
static void pointer(lv_indev_t *i, lv_indev_data_t *data) {
    (void)i;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static HWND control(const wchar_t *type, const wchar_t *label, DWORD style, int x, int y, int w,
                    int h, int id) {
    HWND o =
        CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | style, (int)(x * window_scale),
                        (int)(y * window_scale), (int)(w * window_scale), (int)(h * window_scale),
                        window, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(o, WM_SETFONT, (WPARAM)controls_font, TRUE);
    return o;
}
static void load_config(void) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW dialog = {.lStructSize = sizeof(dialog),
                            .hwndOwner = window,
                            .lpstrFilter = L"JSON configuration\0*.json\0All files\0*.*\0",
                            .lpstrFile = file,
                            .nMaxFile = MAX_PATH,
                            .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST};
    if (!GetOpenFileNameW(&dialog))
        return;
    FILE *f = _wfopen(file, L"rb");
    if (!f)
        return;
    char text[8193], error[160];
    size_t n = fread(text, 1, 8192, f);
    int extra = fgetc(f);
    fclose(f);
    text[n] = 0;
    if (extra != EOF || memchr(text, 0, n) ||
        !sb_settings_parse(text, &sb_settings, error, sizeof(error))) {
        MessageBoxW(window, L"Invalid spoolbuddy.json / 配置文件无效", L"Configuration",
                    MB_OK | MB_ICONERROR);
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, file, -1, selected_config, sizeof(selected_config), NULL, NULL);
    ui_language(sb_settings.chinese);
    char summary[400];
    snprintf(summary, sizeof(summary), "SSID: %s\r\n%s", sb_settings.ssid, sb_settings.backend_url);
    wchar_t *w = wide(summary);
    SetWindowTextW(config_label, w);
    free(w);
}
static LRESULT CALLBACK procedure(HWND h, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_MOUSEMOVE:
        mouse_x = (int)(GET_X_LPARAM(l) / window_scale);
        mouse_y = (int)(GET_Y_LPARAM(l) / window_scale);
        return 0;
    case WM_LBUTTONDOWN:
        mouse_x = (int)(GET_X_LPARAM(l) / window_scale);
        mouse_y = (int)(GET_Y_LPARAM(l) / window_scale);
        if (mouse_x < 800 && mouse_y < 480) {
            SetFocus(h);
            pressed = true;
            SetCapture(h);
        }
        return 0;
    case WM_LBUTTONUP:
        pressed = false;
        ReleaseCapture();
        return 0;
    case WM_KILLFOCUS:
        pressed = false;
        return 0;
    case WM_CHAR:
        if ((w >= '0' && w <= '9') || w == '.') {
            char key[2] = {(char)w, 0};
            ui_test_key(key);
        } else if (w == 8)
            ui_test_key(LV_SYMBOL_BACKSPACE);
        else if (w == 13)
            ui_test_key(LV_SYMBOL_OK);
        else if (w == 27)
            ui_test_key(LV_SYMBOL_KEYBOARD);
        return 0;
    case WM_COMMAND:
        if (HIWORD(w) != BN_CLICKED)
            return 0;
        switch (LOWORD(w)) {
        case 101:
            enqueue(SCAN);
            break;
        case 102:
            enqueue(REMOVE);
            break;
        case 103: {
            wchar_t value[40];
            char utf[100];
            GetWindowTextW(scale_edit, value, 40);
            WideCharToMultiByte(CP_UTF8, 0, value, -1, utf, sizeof(utf), NULL, NULL);
            float g;
            if (!sb_parse_weight(utf, &g)) {
                MessageBoxW(h, L"Enter 0–100000 g / 请输入有效重量", L"Weight", MB_OK);
                break;
            }
            EnterCriticalSection(&action_lock);
            requested_scale = g;
            commands |= SCALE;
            LeaveCriticalSection(&action_lock);
            break;
        }
        case 104:
            load_config();
            break;
        case 105:
            if (!selected_config[0]) {
                MessageBoxW(h, L"请先加载 SD 配置文件 / Load SD configuration first",
                            L"Configuration", MB_OK);
                break;
            }
            EnableWindow(load_button, FALSE);
            enqueue(CONNECT);
            break;
        case 106:
            EnableWindow(load_button, TRUE);
            enqueue(DEMO);
            break;
        case 107:
            ui_test_click(sb_settings.chinese ? "手动输入" : "Manual input");
            break;
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT p;
        HDC dc = BeginPaint(h, &p);
        EnterCriticalSection(&display_lock);
        SetStretchBltMode(dc, HALFTONE);
        StretchDIBits(dc, 0, 0, (int)(800 * window_scale), (int)(480 * window_scale), 0, 0, 800,
                      480, frame, &bitmap, DIB_RGB_COLORS, SRCCOPY);
        LeaveCriticalSection(&display_lock);
        EndPaint(h, &p);
        return 0;
    }
    case WM_CLOSE:
        InterlockedExchange(&running, 0);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, message, w, l);
}
void sb_display_start(void) {
    SetProcessDPIAware();
    InitializeCriticalSection(&display_lock);
    InitializeCriticalSection(&action_lock);
    lv_init();
    lv_tick_set_cb(ticks);
    WNDCLASSW wc = {.lpfnWndProc = procedure,
                    .hInstance = GetModuleHandleW(NULL),
                    .lpszClassName = L"SpoolBuddyP4Simulator",
                    .hCursor = LoadCursorW(NULL, IDC_ARROW),
                    .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1)};
    RegisterClassW(&wc);
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    window_scale = fminf(1.0f, fminf((work.right - work.left - 30) / 1280.0f,
                                     (work.bottom - work.top - 60) / 800.0f));
    if (window_scale < 0.4f)
        window_scale = 0.4f;
    controls_font = CreateFontW(-(int)(18 * window_scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    RECT r = {0, 0, (int)(1280 * window_scale), (int)(800 * window_scale)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    window = CreateWindowExW(0, wc.lpszClassName, L"SpoolBuddy P4 · 功能联调模拟器 · 虚拟外设",
                             style, work.left + ((work.right - work.left) - (r.right - r.left)) / 2,
                             work.top + ((work.bottom - work.top) - (r.bottom - r.top)) / 2,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = 800;
    bitmap.bmiHeader.biHeight = -480;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    lv_display_t *display = lv_display_create(800, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, pointer);
    control(L"STATIC", L"功能联调模拟器 / Functional simulator", 0, 825, 24, 430, 30, 0);
    control(L"STATIC",
            L"同一份 LVGL 界面、配置与重量校验代码\r\n虚拟 NFC / 电子秤；不模拟 P4 CPU 或射频。", 0,
            825, 65, 430, 52, 0);
    control(L"BUTTON", L"放入虚拟标签 / Scan virtual tag", BS_PUSHBUTTON, 825, 142, 430, 44, 101);
    control(L"BUTTON", L"移除标签 / Remove tag", BS_PUSHBUTTON, 825, 198, 430, 44, 102);
    control(L"STATIC", L"虚拟电子秤整盘总重 (g) / Virtual scale", 0, 825, 264, 430, 24, 0);
    scale_edit = control(L"EDIT", L"980.0", WS_BORDER | ES_AUTOHSCROLL, 825, 298, 190, 36, 0);
    control(L"BUTTON", L"传入读数 / Inject", BS_PUSHBUTTON, 1030, 296, 225, 40, 103);
    control(L"BUTTON", L"打开手动重量键盘 / Manual input", BS_PUSHBUTTON, 825, 358, 430, 44, 107);
    load_button = control(L"BUTTON", L"加载 SD 配置 / Load spoolbuddy.json", BS_PUSHBUTTON, 825,
                          434, 430, 44, 104);
    config_label = control(L"STATIC", L"尚未加载 SD 配置 / No SD configuration loaded", 0, 825, 490,
                           430, 60, 0);
    control(L"BUTTON", L"连接真实后端 / Connect to real API", BS_PUSHBUTTON, 825, 564, 430, 44,
            105);
    control(L"BUTTON", L"断开并恢复演示 / Offline demo", BS_PUSHBUTTON, 825, 620, 430, 44, 106);
    control(L"STATIC",
            L"默认离线演示，不修改真实库存。\r\n连接后，扫描和同步会发送真实 API 请求。\r\n左侧 EN "
            L"/ 中文 可切换界面语言。",
            0, 825, 698, 430, 80, 0);
    if (!hidden)
        ShowWindow(window, SW_SHOW);
}
static void screenshot(const char *path) {
    sb_display_lock();
    lv_refr_now(NULL);
    FILE *file = fopen(path, "wb");
    if (file) {
        BITMAPFILEHEADER header = {
            .bfType = 0x4d42,
            .bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(frame),
            .bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
        fwrite(&header, 1, sizeof(header), file);
        fwrite(&bitmap.bmiHeader, 1, sizeof(bitmap.bmiHeader), file);
        fwrite(frame, 1, sizeof(frame), file);
        fclose(file);
    }
    sb_display_unlock();
}
static cJSON *http(const char *path, const cJSON *payload) {
    char address[700];
    snprintf(address, sizeof(address), "%s/api/v1%s", sb_settings.backend_url, path);
    wchar_t *url = wide(address);
    wchar_t host[256], route[512];
    URL_COMPONENTS parts = {.dwStructSize = sizeof(parts),
                            .lpszHostName = host,
                            .dwHostNameLength = 256,
                            .lpszUrlPath = route,
                            .dwUrlPathLength = 512};
    if (!url || !WinHttpCrackUrl(url, 0, 0, &parts)) {
        free(url);
        return NULL;
    }
    host[parts.dwHostNameLength] = 0;
    route[parts.dwUrlPathLength] = 0;
    HINTERNET session = WinHttpOpen(L"SpoolBuddy simulator/0.2.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
              connection = NULL, request = NULL;
    cJSON *result = NULL;
    char *body = payload ? cJSON_PrintUnformatted(payload) : NULL;
    if (!session)
        goto cleanup;
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);
    connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection)
        goto cleanup;
    request = WinHttpOpenRequest(connection, payload ? L"POST" : L"GET", route, NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
        goto cleanup;
    wchar_t headers[500] = L"Content-Type: application/json\r\n";
    if (sb_settings.api_key[0]) {
        wchar_t *key = wide(sb_settings.api_key);
        wcscat(headers, L"X-API-Key: ");
        wcscat(headers, key);
        wcscat(headers, L"\r\n");
        free(key);
    }
    if (!WinHttpSendRequest(request, headers, (DWORD)-1, body, body ? strlen(body) : 0,
                            body ? strlen(body) : 0, 0) ||
        !WinHttpReceiveResponse(request, NULL))
        goto cleanup;
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status < 200 || status >= 300)
        goto cleanup;
    char data[65536];
    size_t used = 0;
    DWORD read;
    while (used < sizeof(data) - 1 &&
           WinHttpReadData(request, data + used, sizeof(data) - 1 - used, &read) && read)
        used += read;
    data[used] = 0;
    result = cJSON_Parse(data);
cleanup:
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    free(url);
    free(body);
    return result;
}
static cJSON *sim_get(const char *path) { return http(path, NULL); }
static cJSON *payload(void) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "device_id", "p4-sim-windows");
    return p;
}
static cJSON *post(const char *path, cJSON *p) {
    char full[180];
    snprintf(full, sizeof(full), "/spoolbuddy%s", path);
    cJSON *r = http(full, p);
    cJSON_Delete(p);
    return r;
}
static bool register_device(void) {
    cJSON *p = payload();
    cJSON_AddStringToObject(p, "hostname", "SpoolBuddy-P4-Simulator");
    cJSON_AddStringToObject(p, "ip_address", "127.0.0.1");
    cJSON_AddStringToObject(p, "firmware_version", "sim-0.2.0");
    cJSON_AddBoolToObject(p, "has_nfc", true);
    cJSON_AddBoolToObject(p, "has_scale", true);
    cJSON_AddBoolToObject(p, "has_backlight", true);
    cJSON_AddNumberToObject(p, "tare_offset", 0);
    cJSON_AddNumberToObject(p, "calibration_factor", 1);
    cJSON_AddStringToObject(p, "nfc_reader_type", "virtual PN5180");
    cJSON_AddStringToObject(p, "nfc_connection", "simulator");
    cJSON *r = post("/devices/register", p);
    bool ok = r != NULL;
    cJSON_Delete(r);
    return ok;
}
static void scan(void) {
    last_scan_id=0;
    tag_present = true;
    ui_tag(uid, "NFC sticker detected");
    ui_wake();
    if (!live) {
        ui_spool(uid, 7, "Demo / 示例耗材", "PLA", demo_remaining, 250);
        return;
    }
    cJSON *p = payload();
    cJSON_AddStringToObject(p, "tag_uid", uid);
    cJSON_AddNumberToObject(p, "sak", 0);
    cJSON_AddStringToObject(p, "tag_type", "ntag");
    cJSON *r = post("/nfc/tag-scanned", p);
    cJSON *id = cJSON_GetObjectItemCaseSensitive(r, "spool_id");
    if (cJSON_IsNumber(id)) {
        last_scan_id=id->valueint;
        char route[100];
        snprintf(route, sizeof(route), "/inventory/spools/%d", id->valueint);
        cJSON *spool = http(route, NULL);
        cJSON *b = cJSON_GetObjectItemCaseSensitive(spool, "brand"),
              *m = cJSON_GetObjectItemCaseSensitive(spool, "material"),
              *lw = cJSON_GetObjectItemCaseSensitive(spool, "label_weight"),
              *used = cJSON_GetObjectItemCaseSensitive(spool, "weight_used"),
              *core = cJSON_GetObjectItemCaseSensitive(spool, "core_weight");
        if (spool)
            ui_spool(uid, id->valueint, cJSON_IsString(b) ? b->valuestring : "",
                     cJSON_IsString(m) ? m->valuestring : "",
                     cJSON_IsNumber(lw) && cJSON_IsNumber(used)
                         ? fmax(0, lw->valuedouble - used->valuedouble)
                         : 0,
                     cJSON_IsNumber(core) ? core->valuedouble : 250);
        else
            ui_details(uid, "Unknown spool: link it in Bambuddy");
        cJSON_Delete(spool);
    } else
        ui_details(uid, "Unknown spool: link it in Bambuddy");
    cJSON_Delete(r);
}
static void remove_tag(void) {
    if (live && tag_present) {
        cJSON *p = payload();
        cJSON_AddStringToObject(p, "tag_uid", uid);
        cJSON_Delete(post("/nfc/tag-removed", p));
    }
    tag_present = false;
    ui_tag("", "Ready for the next spool");
}
static bool sync_weight(int id, float g) {
    if (!live) {
        demo_remaining = fmaxf(0, g - 250);
        ui_status("Weight saved to inventory");
        return true;
    }
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "spool_id", id);
    cJSON_AddNumberToObject(p, "weight_grams", g);
    cJSON *r = post("/scale/update-spool-weight", p);
    bool ok = r != NULL;
    cJSON_Delete(r);
    ui_status(ok ? "Weight saved to inventory" : "Weight sync failed");
    return ok;
}
static void heartbeat(void) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "nfc_ok", true);
    cJSON_AddBoolToObject(p, "scale_ok", true);
    cJSON_AddNumberToObject(p, "uptime_s", GetTickCount64() / 1000);
    cJSON *r = post("/devices/p4-sim-windows/heartbeat", p);
    if (!r) {
        registered = false;
        screens_online(false);
        ui_status("Backend disconnected; retrying...");
        return;
    }
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(r, "pending_command");
    if (cJSON_IsString(cmd) && !strcmp(cmd->valuestring, "write_tag") && tag_present) {
        cJSON *w = cJSON_GetObjectItemCaseSensitive(r, "pending_write_payload"),
              *hex = cJSON_GetObjectItemCaseSensitive(w, "ndef_data_hex"),
              *id = cJSON_GetObjectItemCaseSensitive(w, "spool_id");
        if (cJSON_IsString(hex) && cJSON_IsNumber(id) &&
            strlen(hex->valuestring) < sizeof(last_write) &&
            (id->valueint != last_write_id || strcmp(hex->valuestring, last_write))) {
            const char *s = hex->valuestring;
            size_t n = strlen(s);
            bool ok = n > 0 && !(n % 2) && n <= 1776;
            for (size_t i = 0; i < n; i++)
                if (!strchr("0123456789abcdefABCDEF", s[i]))
                    ok = false;
            uint8_t decoded[888];
            if (ok) {
                for (size_t i = 0; i < n / 2; i++) {
                    char byte[3] = {s[i * 2], s[i * 2 + 1], 0};
                    decoded[i] = (uint8_t)strtoul(byte, NULL, 16);
                }
                size_t start = n >= 8 && decoded[0] == 0xe1 ? 4 : 0, count = n / 2 - start;
                ok = count >= 2 && count <= sizeof(virtual_tag) && decoded[start] == 3;
                if (ok) {
                    memcpy(virtual_tag, decoded + start, count);
                    ok = !memcmp(virtual_tag, decoded + start, count);
                }
            }
            cJSON *result = payload();
            cJSON_AddNumberToObject(result, "spool_id", id->valueint);
            cJSON_AddStringToObject(result, "tag_uid", uid);
            cJSON_AddBoolToObject(result, "success", ok);
            cJSON_AddStringToObject(result, "message",
                                    ok ? "SIMULATED virtual tag write: no RF hardware"
                                       : "Invalid simulated NDEF data");
            cJSON *ack = post("/nfc/write-result", result);
            if (ack) {
                snprintf(last_write, sizeof(last_write), "%s", s);
                last_write_id = id->valueint;
            }
            cJSON_Delete(ack);
            ui_status(ok ? "Tag write verified" : "Tag write failed");
        }
    } else if (!cJSON_IsString(cmd))
        last_write[0] = 0;
    cJSON_Delete(r);
}
static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    ULONGLONG next_heartbeat = 0, next_weight = 0;
    while (InterlockedCompareExchange(&running, 1, 1)) {
        EnterCriticalSection(&action_lock);
        unsigned actions = commands;
        commands = 0;
        float scale = requested_scale;
        LeaveCriticalSection(&action_lock);
        if (actions & DEMO) {
            live = false;
            registered = false;
            screens_online(false);
            screens_demo();
            remove_tag();
            ui_status("Simulation mode: virtual peripherals");
        }
        if (actions & CONNECT) {
            live = true;
            registered = false;
            ui_status("Backend mode: real API requests");
        }
        if (live && !registered) {
            registered = register_device();
            if (!registered) {
                ui_status("Bambuddy unavailable; retrying...");
                Sleep(1000);
                continue;
            }
        }
        screens_online(live && registered);
        if(live && registered) screens_network(sim_get, http, "p4-sim-windows", false);
        if (actions & SCAN)
            scan();
        if (actions & REMOVE)
            remove_tag();
        if (actions & SCALE) {
            scale_value = scale;
            ui_weight(scale_value, true, true);
        }
        if (ui_take_tare()) {
            scale_value = 0;
            ui_weight(0, true, true);
            ui_status("Scale zeroed");
        }
        int id;
        float g;
        if (ui_take_sync(&id, &g))
            sync_weight(id, g);
        float manual;
        ui_take_manual(&manual);
        ULONGLONG now = GetTickCount64();
        if (live && now >= next_heartbeat) {
            heartbeat();
            next_heartbeat = now + 5000;
        }
        if (live && now >= next_weight) {
            float gross = ui_manual_value(&manual) ? manual : scale_value;
            cJSON *p = payload();
            cJSON_AddNumberToObject(p, "weight_grams", gross);
            cJSON_AddBoolToObject(p, "stable", true);
            cJSON_Delete(post("/scale/reading", p));
            next_weight = now + 1000;
        }
        Sleep(50);
    }
    return 0;
}
static int self_test(const char *backend) {
    FILE *log = fopen("sim-self-test.txt", "w");
    int failures = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        bool pass = (x);                                                                           \
        fprintf(log, "%s %s\n", pass ? "PASS" : "FAIL", #x);                                       \
        fflush(log);                                                                               \
        if (!pass)                                                                                 \
            failures++;                                                                            \
    } while (0)
    float g;
    CHECK(!sb_parse_weight("-1", &g));
    CHECK(!sb_parse_weight("nan", &g));
    CHECK(!sb_parse_weight("100001", &g));
    CHECK(sb_parse_weight("1234.5", &g) && g == 1234.5f);
    char error[160];
    sb_settings_t original = sb_settings, candidate;
    CHECK(!sb_settings_parse("{\"wifi\":{}}", &candidate, error, sizeof(error)));
    const char *valid =
        "{\"wifi\":{\"ssid\":\"test\",\"password\":\"password\"},\"bambuddy\":{\"url\":\"http://"
        "127.0.0.1:8000/\",\"api_key\":\"\"},\"language\":\"zh-CN\"}";
    CHECK(sb_settings_parse(valid, &candidate, error, sizeof(error)) &&
          !strcmp(candidate.backend_url, "http://127.0.0.1:8000"));
    FILE *config = fopen("sd-test.json", "wb");
    CHECK(config != NULL);
    if (config) {
        fwrite("\xef\xbb\xbf", 1, 3, config);
        fwrite(valid, 1, strlen(valid), config);
        fclose(config);
    }
    CHECK(sb_settings_read_file("sd-test.json", error, sizeof(error)) && sb_settings.chinese);
    FILE *invalid = fopen("invalid-test.json", "wb");
    if (invalid) {
        fwrite("{}", 1, 2, invalid);
        fclose(invalid);
    }
    sb_settings_t before = sb_settings;
    CHECK(!sb_settings_read_file("invalid-test.json", error, sizeof(error)) &&
          !memcmp(&before, &sb_settings, sizeof(before)));
    sb_settings = original;
    if (backend) {
        snprintf(sb_settings.backend_url, sizeof(sb_settings.backend_url), "%s", backend);
        live = true;
        CHECK(register_device());
        screens_online(true);
        screens_network(sim_get, http, "p4-sim-windows", true);
        screens_test_page(2);
        CHECK(ui_test_click("#7   测试品牌  PLA\nGreen   730 g"));
        CHECK(ui_test_click("写入标签"));
        CHECK(ui_test_click("确认"));
        screens_network(sim_get, http, "p4-sim-windows", false);
        screens_test_page(1);
        CHECK(ui_test_click("AMS A · 1\nPLA\n剩余 73%"));
        CHECK(ui_test_click("确认"));
        screens_network(sim_get, http, "p4-sim-windows", false);
        screens_test_page(0);
    }
    ui_status(live ? "Backend mode: real API requests" : "Simulation mode: virtual peripherals");
    scan();
    ui_weight(980, true, true);
    screenshot("sim-zh.bmp");
    CHECK(ui_test_click("手动输入"));
    ui_test_input("100001");
    CHECK(ui_test_click("采用重量"));
    CHECK(!ui_manual_active());
    ui_test_input("");
    CHECK(ui_test_key("1"));
    CHECK(ui_test_key("2"));
    CHECK(ui_test_key("3"));
    CHECK(ui_test_key("4"));
    CHECK(ui_test_key("."));
    CHECK(ui_test_key("5"));
    CHECK(ui_test_click("采用重量"));
    CHECK(ui_manual_value(&g) && g == 1234.5f);
    ui_weight(300, true, true);
    CHECK(ui_manual_value(&g) && g == 1234.5f);
    screenshot("sim-manual-zh.bmp");
    CHECK(ui_test_click("将整盘总重同步到库存"));
    int id;
    CHECK(ui_take_sync(&id, &g) && id == 7 && g == 1234.5f);
    CHECK(sync_weight(id, g));
    CHECK(ui_test_click("EN"));
    CHECK(ui_test_click("Manual input"));
    screenshot("sim-keyboard-en.bmp");
    CHECK(ui_test_click("Cancel"));
    screenshot("sim-en.bmp");
    for(int page=1;page<=4;page++) {
        screens_test_page(page); char name[50]; snprintf(name,sizeof(name),"sim-page-%d.bmp",page); screenshot(name);
    }
    screens_test_page(0);
    CHECK(ui_test_click("Use electronic scale"));
    CHECK(!ui_manual_active());
    if (live) {
        heartbeat();
        CHECK(virtual_tag[0] == 3);
    }
    remove_tag();
    CHECK(ui_test_click("Save gross weight to inventory"));
    CHECK(!ui_take_sync(&id, &g));
    if (live)
        heartbeat();
    fprintf(log, "Failures: %d\n", failures);
    fclose(log);
    return failures ? 1 : 0;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool connection_test = argc > 1 && !wcscmp(argv[1], L"--connection-test");
    hidden = connection_test || (argc > 1 && !wcscmp(argv[1], L"--self-test"));
    bool auto_connect = false;
    for (int i = 1; i < argc && !hidden; i++) {
        if (!wcscmp(argv[i], L"--connect")) auto_connect = true;
        else if (!wcscmp(argv[i], L"--config") && i + 1 < argc)
            WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, selected_config,
                                sizeof(selected_config), NULL, NULL);
        else if (!wcscmp(argv[i], L"--scan-uid") && i + 1 < argc)
            WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, uid, sizeof(uid), NULL, NULL);
    }
    if (selected_config[0]) {
        char error[160];
        if (!sb_settings_read_file(selected_config, error, sizeof(error))) {
            MessageBoxW(NULL, L"Invalid startup SD configuration", L"SpoolBuddy", MB_ICONERROR);
            LocalFree(argv);
            return 2;
        }
    }
    char backend[256] = "";
    if (hidden && argc > 2)
        WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, backend, sizeof(backend), NULL, NULL);
    bool probe_scan = connection_test && argc > 3;
    if(probe_scan) WideCharToMultiByte(CP_UTF8,0,argv[3],-1,uid,sizeof(uid),NULL,NULL);
    LocalFree(argv);
    ui_init();
    ui_status("Simulation mode: virtual peripherals");
    ui_weight(980, true, true);
    screens_demo();
    if (hidden) {
        if(connection_test) {
            if (!backend[0]) { DestroyWindow(window); return 2; }
            snprintf(sb_settings.backend_url,sizeof(sb_settings.backend_url),"%s",backend);
            live=true;
            bool ok=register_device();
            if(ok) {
                screens_online(true);
                screens_network(sim_get,http,"p4-sim-windows",true);
                heartbeat();
                ui_status("Connected to Bambuddy");
                ui_weight(520,true,true);
                /* Known fixture UID supplied by the live test caller; no inventory writes. */
                if(probe_scan) scan();
                if(probe_scan && last_scan_id<=0) ok=false;
                cJSON *reading=payload();
                cJSON_AddNumberToObject(reading,"weight_grams",520);
                cJSON_AddBoolToObject(reading,"stable",true);
                cJSON *ack=post("/scale/reading",reading);
                ok=ok && ack!=NULL; cJSON_Delete(ack);
                screenshot("live-dashboard.bmp");
                for(int page=1;page<=4;page++) {
                    screens_test_page(page); char path[80];
                    snprintf(path,sizeof(path),"live-page-%d.bmp",page); screenshot(path);
                }
                screens_test_page(0);
                bool manual_ok=ui_test_click("手动输入");
                ui_test_input("543.2");
                manual_ok=manual_ok && ui_test_click("采用重量");
                float manual=0;
                manual_ok=manual_ok && ui_manual_value(&manual) && fabsf(manual-543.2f)<0.01f;
                reading=payload(); cJSON_AddNumberToObject(reading,"weight_grams",manual);
                cJSON_AddBoolToObject(reading,"stable",true);
                ack=post("/scale/reading",reading); ok=ok && manual_ok && ack!=NULL; cJSON_Delete(ack);
                screenshot("live-manual.bmp");
            }
            DestroyWindow(window); return ok?0:1;
        }
        int result = self_test(backend[0] ? backend : NULL);
        DestroyWindow(window);
        return result;
    }
    if (selected_config[0]) {
        wchar_t *label = wide(sb_settings.backend_url);
        SetWindowTextW(config_label, label);
        free(label);
    }
    if (auto_connect && selected_config[0]) {
        EnableWindow(load_button, FALSE);
        enqueue(CONNECT);
        if(strcmp(uid,"04A1B2C3D4E5F6")) enqueue(SCAN);
    }
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    MSG message;
    while (InterlockedCompareExchange(&running, 1, 1)) {
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                InterlockedExchange(&running, 0);
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        sb_display_lock();
        lv_timer_handler();
        sb_display_unlock();
        Sleep(5);
    }
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    return 0;
}
