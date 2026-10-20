#pragma once
#include <stdint.h>
/* RFC 7231 Date is UTC, independent of the configured display timezone. */
int64_t sb_http_date_epoch(const char *date);
