// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stddef.h>
void sb_memory_init(void);
void *sb_memory_alloc(size_t size);
void *sb_memory_calloc(size_t count, size_t size);
