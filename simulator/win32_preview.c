// SPDX-License-Identifier: AGPL-3.0-or-later
// Native 800x480 LVGL host with optional Bambuddy transport.
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "lvgl.h"
#include "ui_original.h"
#include "settings.h"
#include "bridge.h"
#include "ui_live.h"
#include "ui.h"
#include "runtime.h"
#ifdef SB_ENABLE_SIMULATOR_CONTROLS
extern void sb_virtual_nfc_key(unsigned key);
#endif
static HWND window;
static CRITICAL_SECTION lock;
static BITMAPINFO bitmap;
static uint32_t frame[800 * 480], draw[800 * 480];
static int pointer_x, pointer_y;
static bool pressed, done, capture, chinese;
static float scale = 1;
static lv_obj_t *scroll_target(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN))
        return NULL;
    lv_area_t a;
    lv_obj_get_coords(root, &a);
    if (pointer_x < a.x1 || pointer_x > a.x2 || pointer_y < a.y1 || pointer_y > a.y2)
        return NULL;
    for (int i = (int)lv_obj_get_child_count(root) - 1; i >= 0; i--) {
        lv_obj_t *o = scroll_target(lv_obj_get_child(root, i));
        if (o)
            return o;
    }
    return lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLLABLE) ? root : NULL;
}
void sb_display_lock(void) { EnterCriticalSection(&lock); }
void sb_display_unlock(void) { LeaveCriticalSection(&lock); }
void sb_display_brightness(int value) { (void)value; }
static uint32_t ticks(void) { return (uint32_t)GetTickCount64(); }
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *pixels) {
    int width = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(frame + y * 800 + a->x1, pixels + (y - a->y1) * width * 4, width * 4);
    lv_display_flush_ready(display);
    InvalidateRect(window, NULL, FALSE);
}
static void pointer(lv_indev_t *input, lv_indev_data_t *data) {
    (void)input;
    data->point.x = pointer_x;
    data->point.y = pointer_y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static LRESULT CALLBACK procedure_ui(HWND h, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_MOUSEMOVE:
        pointer_x = (int)(GET_X_LPARAM(l) / scale);
        pointer_y = (int)(GET_Y_LPARAM(l) / scale);
        return 0;
    case WM_LBUTTONDOWN:
        pointer_x = (int)(GET_X_LPARAM(l) / scale);
        pointer_y = (int)(GET_Y_LPARAM(l) / scale);
        pressed = true;
        SetCapture(h);
        return 0;
    case WM_LBUTTONUP:
        pressed = false;
        ReleaseCapture();
        return 0;
    case WM_KILLFOCUS:
        pressed = false;
        return 0;
    case WM_MOUSEWHEEL: {
        lv_obj_t *o = scroll_target(lv_screen_active());
        if (o)
            lv_obj_scroll_by(o, 0, GET_WHEEL_DELTA_WPARAM(w) > 0 ? 70 : -70, LV_ANIM_ON);
        return 0;
    }
    case WM_CHAR: {
        extern void original_host_char(unsigned code);
        original_host_char((unsigned)w);
        return 0;
    }
    case WM_KEYDOWN:
#ifdef SB_ENABLE_SIMULATOR_CONTROLS
        sb_virtual_nfc_key((unsigned)w);
#endif
        if (w == VK_F2) {
            chinese = !chinese;
            original_language(chinese);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        EnterCriticalSection(&lock);
        SetStretchBltMode(dc, HALFTONE);
        StretchDIBits(dc, 0, 0, (int)(800 * scale), (int)(480 * scale), 0, 0, 800, 480, frame,
                      &bitmap, DIB_RGB_COLORS, SRCCOPY);
        LeaveCriticalSection(&lock);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CLOSE:
        done = true;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, message, w, l);
}
static LRESULT CALLBACK procedure(HWND h, UINT message, WPARAM w, LPARAM l) {
    EnterCriticalSection(&lock);
    LRESULT result = procedure_ui(h, message, w, l);
    LeaveCriticalSection(&lock);
    return result;
}
void sb_display_start(void) {
    SetProcessDPIAware();
    InitializeCriticalSection(&lock);
    lv_init();
    lv_tick_set_cb(ticks);
    WNDCLASSW wc = {.lpfnWndProc = procedure,
                    .hInstance = GetModuleHandleW(NULL),
                    .lpszClassName = L"SpoolBuddyOriginalPreview",
                    .hCursor = LoadCursorW(NULL, IDC_ARROW)};
    RegisterClassW(&wc);
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    scale = fminf(1, (work.right - work.left - 32) / 800.0f);
    scale = fminf(scale, (work.bottom - work.top - 64) / 480.0f);
    RECT r = {0, 0, (int)(800 * scale), (int)(480 * scale)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    window = CreateWindowExW(0, wc.lpszClassName, L"SpoolBuddy · 原版界面预览 · 800×480 LVGL",
                             style, work.left + ((work.right - work.left) - (r.right - r.left)) / 2,
                             work.top + ((work.bottom - work.top) - (r.bottom - r.top)) / 2,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    bitmap.bmiHeader = (BITMAPINFOHEADER){.biSize = sizeof(BITMAPINFOHEADER),
                                          .biWidth = 800,
                                          .biHeight = -480,
                                          .biPlanes = 1,
                                          .biBitCount = 32,
                                          .biCompression = BI_RGB};
    lv_display_t *display = lv_display_create(800, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, pointer);
    if (!capture)
        ShowWindow(window, SW_SHOW);
}
static bool screenshot(const char *path) {
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    BITMAPFILEHEADER header = {.bfType = 0x4d42, .bfSize = 54 + sizeof(frame), .bfOffBits = 54};
    fwrite(&header, 1, sizeof(header), f);
    fwrite(&bitmap.bmiHeader, 1, sizeof(bitmap.bmiHeader), f);
    fwrite(frame, 1, sizeof(frame), f);
    fclose(f);
    return true;
}
bool sb_sim_capture(const char *path) { return screenshot(path); }
static bool click_nav(int page) {
    pointer_x = page * 160 + 80;
    pointer_y = 451;
    lv_indev_t *indev = lv_indev_get_next(NULL);
    pressed = true;
    lv_indev_read(indev);
    pressed = false;
    lv_indev_read(indev);
    lv_refr_now(NULL);
    int green = 0;
    for (int y = 436; y < 460; y++)
        for (int x = page * 160 + 68; x < page * 160 + 92; x++)
            if (((frame[y * 800 + x] >> 8) & 255) > ((frame[y * 800 + x] >> 16) & 255) + 30 &&
                ((frame[y * 800 + x] >> 8) & 255) > (frame[y * 800 + x] & 255) + 30)
                green++;
    return green > 8;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    int argc, page = 0;
    bool connect = false, selftest = false, readtest = false, transporttest = false,
         backendtest = false, statstest = false, realtimetest=false, realtime_denied=false;
    char config[MAX_PATH] = "";
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i],L"--realtime-test") || !wcscmp(argv[i],L"--realtime-denied-test")) {
            realtimetest=true;realtime_denied=!wcscmp(argv[i],L"--realtime-denied-test");capture=true;
        }
        if (!wcscmp(argv[i], L"--stats-test")) {
            statstest = true;
            capture = true;
        }
        if (!wcscmp(argv[i], L"--backend-test")) {
            backendtest = true;
            connect = true;
            capture = true;
        }
        if (!wcscmp(argv[i], L"--transport-test")) {
            transporttest = true;
            capture = true;
        }
        if (!wcscmp(argv[i], L"--read-test")) {
            readtest = true;
            connect = true;
            capture = true;
        }
        if (!wcscmp(argv[i], L"--self-test")) {
            selftest = true;
            capture = true;
        }
        if (!wcscmp(argv[i], L"--capture"))
            capture = true;
        if (!wcscmp(argv[i], L"--language") && i + 1 < argc)
            chinese = !wcscmp(argv[++i], L"zh-CN");
        if (!wcscmp(argv[i], L"--page") && i + 1 < argc)
            page = _wtoi(argv[++i]);
        if (!wcscmp(argv[i], L"--config") && i + 1 < argc)
            WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, config, sizeof(config), NULL, NULL);
        if (!wcscmp(argv[i], L"--connect"))
            connect = true;
    }
    LocalFree(argv);
    if (config[0]) {
        char error[160];
        if (!sb_settings_read_file(config, error, sizeof(error))) {
            MessageBoxW(NULL, L"Invalid spoolbuddy.json", L"SpoolBuddy", MB_ICONERROR);
            return 2;
        }
        chinese = sb_settings.chinese;
    }
    if (connect && !config[0]) {
        MessageBoxW(NULL, L"Load a configuration with --config first", L"SpoolBuddy", MB_ICONERROR);
        return 2;
    }
    if (statstest) {
        extern int port_stats_test(void);
        return port_stats_test();
    }
    sb_live.enabled = connect || selftest || realtimetest;
    original_init(chinese);
    bridge_init("p4-sim-windows");
    original_page(page);
    if(realtimetest) {
        extern int port_realtime_test(bool denied);
        int result=port_realtime_test(realtime_denied);DestroyWindow(window);return result;
    }
    if (backendtest) {
        extern int port_backend_e2e(void);
        int result = port_backend_e2e();
        DestroyWindow(window);
        return result;
    }
    if (transporttest) {
        extern int port_transport_test(void);
        int result = port_transport_test();
        DestroyWindow(window);
        return result;
    }
    if (readtest) {
        bridge_online(true);
        bridge_poll(sim_request, true);
        bool ok = bridge_connected() && cJSON_IsArray(bridge_model("inventory")) &&
                  cJSON_IsArray(bridge_model("printers"));
        FILE *report = fopen("port-read-test.txt", "w");
        if (report) {
            fprintf(report,
                    "Read-only backend connection: %s\nInventory spools: %d\nPrinters: %d\nNo "
                    "registration, heartbeat, or mutation sent.\n",
                    ok ? "PASS" : "FAIL", cJSON_GetArraySize(bridge_model("inventory")),
                    cJSON_GetArraySize(bridge_model("printers")));
            fclose(report);
        }
        for (int lang = 0; lang < 2; lang++) {
            original_language(lang == 1);
            for (int p = 0; p < 5; p++) {
                original_page(p);
                char path[100];
                snprintf(path, sizeof(path), "live-%s-%d.bmp", lang ? "zh" : "en", p);
                ok = screenshot(path) && ok;
            }
        }
        DestroyWindow(window);
        return ok ? 0 : 1;
    }
    if (selftest) {
        extern int port_selftest(void);
        int result = port_selftest();
        DestroyWindow(window);
        return result;
    }
    if (capture) {
        bool ok = true;
        const char *names[] = {"dashboard", "ams", "write", "inventory", "settings"};
        for (int lang = 0; lang < 2; lang++) {
            original_language(lang == 1);
            for (int p = 0; p < 5; p++) {
                ok = click_nav(p) && ok;
                original_subpage(0);
                if (p == 2)
                    original_form(0);
                char path[120];
                snprintf(path, sizeof(path), "original-%s-%s.bmp", lang ? "zh" : "en", names[p]);
                ok = screenshot(path) && ok;
                if (p == 2 || p == 4)
                    for (int sub = 1; sub < (p == 2 ? 3 : 5); sub++) {
                        original_subpage(sub);
                        snprintf(path, sizeof(path), "original-%s-%s-%d.bmp", lang ? "zh" : "en",
                                 names[p], sub);
                        ok = screenshot(path) && ok;
                        if (p == 2 && sub == 1) {
                            for (int mode = 1; mode <= 2; mode++) {
                                original_form(mode);
                                snprintf(path, sizeof(path), "original-%s-write-full-%d.bmp",
                                         lang ? "zh" : "en", mode);
                                ok = screenshot(path) && ok;
                            }
                            original_form(0);
                        }
                    }
            }
            original_page(0);
            original_online(true);
            char idle[80];
            snprintf(idle, sizeof(idle), "original-%s-dashboard-idle.bmp", lang ? "zh" : "en");
            ok = screenshot(idle) && ok;
            original_online(false);
        }
        FILE *report = fopen("original-preview-check.txt", "w");
        if (report) {
            fprintf(report,
                    "Native LVGL pointer navigation: %s\n28 bilingual page/state "
                    "captures\nNetwork/sensor actions excluded from this target\n",
                    ok ? "PASS" : "FAIL");
            fclose(report);
        }
        DestroyWindow(window);
        return ok ? 0 : 1;
    }
    if (connect)
        SetWindowTextW(window, L"SpoolBuddy · 原版复刻 · F2 语言");
    if (connect && !sim_start())
        return 3;
    MSG msg;
    while (!done) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                done = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        sb_display_lock();
        lv_timer_handler();
        sb_display_unlock();
        Sleep(5);
    }
    if (connect)
        sim_stop();
    return 0;
}
