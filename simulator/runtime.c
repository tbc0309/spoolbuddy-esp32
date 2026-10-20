// SPDX-License-Identifier: AGPL-3.0-or-later
#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "response_limit.h"
#include "realtime.h"
#include "settings.h"
#include "bridge.h"
#include "ui.h"
#include "optional_inputs.h"
static HANDLE thread;
static volatile LONG running;
static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = calloc(n, sizeof(wchar_t));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
cJSON *sim_request(const char *method, const char *path, const cJSON *payload, int *status) {
    *status = 0;
    char address[2048];
    if (snprintf(address, sizeof(address), "%s/api/v1%s", sb_settings.backend_url, path) >=
        (int)sizeof(address))
        return NULL;
    wchar_t *url = wide(address), *verb = wide(method);
    char *body = payload ? cJSON_PrintUnformatted(payload) : NULL;
    HINTERNET session = NULL, connection = NULL, request = NULL;
    cJSON *result = NULL;
    wchar_t host[256], route[2048], extra[2048];
    URL_COMPONENTS parts = {.dwStructSize = sizeof(parts),
                            .lpszHostName = host,
                            .dwHostNameLength = 256,
                            .lpszUrlPath = route,
                            .dwUrlPathLength = 2048,
                            .lpszExtraInfo = extra,
                            .dwExtraInfoLength = 2048};
    if (!url || !verb || (payload && !body) || !WinHttpCrackUrl(url, 0, 0, &parts))
        goto cleanup;
    host[parts.dwHostNameLength] = 0;
    route[parts.dwUrlPathLength] = 0;
    extra[parts.dwExtraInfoLength] = 0;
    if (parts.dwUrlPathLength + parts.dwExtraInfoLength >= 2048)
        goto cleanup;
    wcscat(route, extra);
    session = WinHttpOpen(L"SpoolBuddy-P4-native/0.3", WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        goto cleanup;
    WinHttpSetTimeouts(session, 1500, 1500, 1500, 1500);
    connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection)
        goto cleanup;
    request = WinHttpOpenRequest(connection, verb, route, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
        goto cleanup;
    // Keep the key in headers. Never place it in a URL or a diagnostic log.
    wchar_t headers[600] = L"Content-Type: application/json\r\n";
    if (sb_settings.api_key[0]) {
        wchar_t *key = wide(sb_settings.api_key);
        if (!key)
            goto cleanup;
        wcscat(headers, L"X-API-Key: ");
        wcscat(headers, key);
        wcscat(headers, L"\r\n");
        free(key);
    }
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    if (!WinHttpSendRequest(request, headers, (DWORD)-1, body, body ? strlen(body) : 0,
                            body ? strlen(body) : 0, 0) ||
        !WinHttpReceiveResponse(request, NULL))
        goto cleanup;
    DWORD code = 0, size = sizeof(code);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX))
        goto cleanup;
    *status = code;
    size_t limit = sb_response_limit(path);
    char *data = malloc(limit + 1);
    if (!data)
        goto cleanup;
    size_t used = 0;
    DWORD n;
    bool ok = true;
    while (used < limit) {
        if (!WinHttpReadData(request, data + used, (DWORD)(limit - used), &n)) {
            ok = false;
            break;
        }
        if (!n)
            break;
        used += n;
    }
    if (used == limit) {
        char extra_byte;
        if (!WinHttpReadData(request, &extra_byte, 1, &n) || n)
            ok = false;
    }
    data[used] = 0;
    if (ok)
        result = used ? cJSON_ParseWithOpts(data, NULL, true) : cJSON_CreateObject();
    free(data);
cleanup:
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    free(url);
    free(verb);
    free(body);
    return result;
}
static bool register_device(void) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "device_id", bridge_device());
    cJSON_AddStringToObject(p, "hostname", "SpoolBuddy-P4-Native-Simulator");
    cJSON_AddStringToObject(p, "ip_address", "127.0.0.1");
    cJSON_AddStringToObject(p, "firmware_version", SB_FIRMWARE_VERSION);
    cJSON_AddBoolToObject(p, "has_nfc", false);
    cJSON_AddBoolToObject(p, "has_scale", false);
    cJSON_AddBoolToObject(p, "has_backlight", true);
    cJSON_AddNumberToObject(p, "tare_offset", 0);
    cJSON_AddNumberToObject(p, "calibration_factor", 1);
    int status;
    cJSON *r = sim_request("POST", "/spoolbuddy/devices/register", p, &status);
    bool accepted = r && status >= 200 && status < 300;
    cJSON_Delete(r);
    cJSON_Delete(p);
    return accepted;
}
static void heartbeat_response(const cJSON *reply) {
    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(reply, "display_brightness");
    const cJSON *timeout = cJSON_GetObjectItemCaseSensitive(reply, "display_blank_timeout");
    if (cJSON_IsNumber(brightness) && cJSON_IsNumber(timeout))
        ui_display_settings(brightness->valueint, timeout->valueint);
    const cJSON *command = cJSON_GetObjectItemCaseSensitive(reply, "pending_command");
    if (!cJSON_IsString(command))
        return;
    const char *kind = !strcmp(command->valuestring, "run_nfc_diag")        ? "nfc"
                       : !strcmp(command->valuestring, "run_scale_diag")    ? "scale"
                       : !strcmp(command->valuestring, "run_read_tag_diag") ? "read_tag"
                                                                            : NULL;
    char path[192];
    cJSON *p = cJSON_CreateObject();
    if (kind) {
        snprintf(path, sizeof(path), "/spoolbuddy/diagnostics/%s/result", bridge_device());
        cJSON_AddStringToObject(p, "diagnostic", kind);
        cJSON_AddBoolToObject(p, "success", false);
        cJSON_AddNumberToObject(p, "exit_code", 1);
        cJSON_AddStringToObject(p, "output",
                                "Windows simulator: no physical PN5180 or NAU7802 attached. "
                                "Virtual UI inputs do not verify hardware.");
    } else if (!strcmp(command->valuestring, "write_tag")) {
        snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/cancel-write", bridge_device());
        ui_status("Simulator has no physical NFC writer; request cancelled");
    } else {
        snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/system/command-result",
                 bridge_device());
        cJSON_AddStringToObject(p, "command", command->valuestring);
        cJSON_AddBoolToObject(p, "success", false);
        cJSON_AddStringToObject(p, "message",
                                "Physical device command is unavailable in the Windows simulator");
    }
    int status;
    cJSON *result = sim_request("POST", path, p, &status);
    cJSON_Delete(result);
    cJSON_Delete(p);
}
static void publish_override(float grams, void *context) {
    (void)context;
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "device_id", bridge_device());
    cJSON_AddNumberToObject(p, "weight_grams", grams);
    cJSON_AddBoolToObject(p, "stable", true);
    int status;
    cJSON *r = sim_request("POST", "/spoolbuddy/scale/reading", p, &status);
    cJSON_Delete(p);
    cJSON_Delete(r);
}
static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    bool registered = false;
    ULONGLONG heartbeat = 0;
    while (InterlockedCompareExchange(&running, 1, 1)) {
        ULONGLONG now = GetTickCount64();
        if (!registered) {
            registered = register_device();
            bridge_online(registered);
            if (!registered) {
                Sleep(1000);
                continue;
            }
        }
        if (now >= heartbeat) {
            cJSON *p = cJSON_CreateObject();
            cJSON_AddBoolToObject(p, "nfc_ok", false);
            cJSON_AddBoolToObject(p, "scale_ok", false);
            cJSON_AddNumberToObject(p, "uptime_s", now / 1000);
            char route[192];
            snprintf(route, sizeof(route), "/spoolbuddy/devices/%s/heartbeat", bridge_device());
            int status;
            cJSON *r = sim_request("POST", route, p, &status);
            registered = r && status >= 200 && status < 300;
            if (registered)
                heartbeat_response(r);
            cJSON_Delete(p);
            cJSON_Delete(r);
            bridge_online(registered);
            heartbeat = now + 5000;
        }
        if (registered) {
            sb_optional_inputs_poll((int64_t)now * 1000, publish_override, NULL);
            bridge_poll(sim_request, false);
        }
        ui_network(registered);
        sb_realtime_poll(registered, sim_request);
        Sleep(50);
    }
    sb_realtime_poll(false, sim_request);
    bridge_online(false);
    return 0;
}
bool sim_start(void) {
    InterlockedExchange(&running, 1);
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    return thread != NULL;
}
void sim_stop(void) {
    InterlockedExchange(&running, 0);
    if (thread) {
        WaitForSingleObject(thread, 30000);
        CloseHandle(thread);
        thread = NULL;
    }
}
