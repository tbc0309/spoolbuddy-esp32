// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
/* Tokens remain in memory only. Never log the returned URL. */
static bool sb_realtime_url(char *out, size_t capacity, const char *base, const char *token) {
    if (strncmp(base, "http://", 7) && strncmp(base, "https://", 8))
        return false;
    bool tls = !strncmp(base, "https://", 8);
    const char *host = base + (tls ? 8 : 7);
    size_t n = strlen(host);
    while (n && host[n - 1] == '/')
        n--;
    int written = snprintf(out, capacity, "%s://%.*s/api/v1/ws", tls ? "wss" : "ws", (int)n, host);
    if (written < 0 || (size_t)written >= capacity)
        return false;
    if (!token || !*token)
        return true;
    size_t used = (size_t)written;
    if (used + 7 >= capacity)
        return false;
    memcpy(out + used, "?token=", 7);
    used += 7;
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)token; *p; p++) {
        bool plain = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                     (*p >= '0' && *p <= '9') || strchr("-_.~", *p);
        if (used + (plain ? 1 : 3) >= capacity)
            return false;
        if (plain)
            out[used++] = (char)*p;
        else {
            out[used++] = '%';
            out[used++] = hex[*p >> 4];
            out[used++] = hex[*p & 15];
        }
    }
    out[used] = 0;
    return true;
}
