// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <string.h>
static size_t sb_response_limit(const char *path) {
    // Catalog lists are unpaginated upstream. Bound them separately from normal replies.
    return !strcmp(path, "/cloud/filaments") || !strcmp(path, "/cloud/builtin-filaments") ||
                   !strcmp(path, "/local-presets/") || !strcmp(path, "/orca-cloud/profiles")
               ? 2u * 1024u * 1024u
               : 65536u;
}
