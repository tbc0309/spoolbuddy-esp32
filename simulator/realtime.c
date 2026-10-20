// SPDX-License-Identifier: AGPL-3.0-or-later
#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>
#include "realtime.h"
#include "realtime_url.h"
#include "settings.h"
static HINTERNET ws_session, ws_connection, ws_socket;
static HANDLE ws_thread;
static volatile LONG ws_done, ws_forbidden;
static ULONGLONG ws_retry;
static bool ws_denied;
static DWORD WINAPI receive_events(void *argument) {
    HINTERNET socket = argument;
    char *message = malloc(65537);
    size_t used = 0;
    if (message)
        for (;;) {
            DWORD read = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            DWORD error = WinHttpWebSocketReceive(socket, message + used, (DWORD)(65536 - used),
                                                  &read, &type);
            if (error == ERROR_WINHTTP_TIMEOUT)
                continue;
            if (error)
                break;
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                USHORT status = 0;
                DWORD size = 0;
                if (WinHttpWebSocketQueryCloseStatus(socket, &status, NULL, 0, &size) == NO_ERROR &&
                    status == 4401)
                    InterlockedExchange(&ws_forbidden, 1);
                break;
            }
            if (type != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
                type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
                break;
            used += read;
            if (used >= 65536)
                break;
            if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                message[used] = 0;
                cJSON *event = cJSON_ParseWithOpts(message, NULL, true);
                if (event)
                    bridge_event(event);
                cJSON_Delete(event);
                used = 0;
            }
        }
    free(message);
    InterlockedExchange(&ws_done, 1);
    return 0;
}
static void stop_socket(void) {
    if (ws_socket) {
        WinHttpCloseHandle(ws_socket);
        ws_socket = NULL;
    }
    if (ws_thread) {
        WaitForSingleObject(ws_thread, INFINITE);
        CloseHandle(ws_thread);
        ws_thread = NULL;
    }
    if (ws_connection) {
        WinHttpCloseHandle(ws_connection);
        ws_connection = NULL;
    }
    if (ws_session) {
        WinHttpCloseHandle(ws_session);
        ws_session = NULL;
    }
}
void sb_realtime_poll(bool connected, sb_transport_fn transport) {
    if (!connected) {
        stop_socket();
        return;
    }
    if (ws_thread && !InterlockedCompareExchange(&ws_done, 0, 0))
        return;
    if (ws_thread) {
        stop_socket();
        ws_retry = GetTickCount64() + 3000;
    }
    if (InterlockedCompareExchange(&ws_forbidden, 0, 0))
        ws_denied = true;
    if (ws_denied || GetTickCount64() < ws_retry)
        return;
    ws_retry = GetTickCount64() + 5000;
    int code = 0;
    cJSON *reply = transport("POST", "/auth/ws-token", NULL, &code);
    if (code == 401 || code == 403) {
        ws_denied = true;
        cJSON_Delete(reply);
        return;
    }
    if (code < 200 || code >= 300) {
        cJSON_Delete(reply);
        return;
    }
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(reply, "token");
    char url[2048];
    bool valid = sb_realtime_url(url, sizeof(url), sb_settings.backend_url,
                                 cJSON_IsString(token) ? token->valuestring : "");
    cJSON_Delete(reply);
    if (!valid)
        return;
    // WinHTTP accepts the HTTP scheme during the upgrade handshake.
    char http_url[2050];
    snprintf(http_url, sizeof(http_url), "http%s%s", !strncmp(url, "wss", 3) ? "s" : "",
             strchr(url, ':'));
    wchar_t wide[2050], host[256], path[2050];
    if (!MultiByteToWideChar(CP_UTF8, 0, http_url, -1, wide, 2050))
        return;
    URL_COMPONENTS parts = {.dwStructSize = sizeof(parts),
                            .lpszHostName = host,
                            .dwHostNameLength = 256,
                            .lpszUrlPath = path,
                            .dwUrlPathLength = 2050};
    if (!WinHttpCrackUrl(wide, 0, 0, &parts))
        return;
    host[parts.dwHostNameLength] = 0;
    path[parts.dwUrlPathLength] = 0;
    ws_session = WinHttpOpen(L"SpoolBuddy-realtime", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    if (!ws_session)
        return;
    WinHttpSetTimeouts(ws_session, 2000, 2000, 2000, 1000);
    ws_connection = WinHttpConnect(ws_session, host, parts.nPort, 0);
    HINTERNET request =
        ws_connection
            ? WinHttpOpenRequest(ws_connection, L"GET", path, NULL, NULL, NULL,
                                 parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
            : NULL;
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!request) {
        stop_socket();
        return;
    }
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    if (WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(request, NULL)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                            &status, &size, NULL);
        if (status == 101)
            ws_socket = WinHttpWebSocketCompleteUpgrade(request, 0);
        else if (status == 401 || status == 403)
            ws_denied = true;
    }
    WinHttpCloseHandle(request);
    if (!ws_socket) {
        stop_socket();
        return;
    }
    InterlockedExchange(&ws_done, 0);
    ws_thread = CreateThread(NULL, 0, receive_events, ws_socket, 0, NULL);
    if (!ws_thread)
        stop_socket();
}
