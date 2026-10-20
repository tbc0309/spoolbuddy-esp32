#pragma once
#include <stdint.h>
typedef void (*sb_optional_weight_sink)(float grams,void *context);
/* Default no-op; an explicitly selected input patch may publish an override. */
void sb_optional_inputs_poll(int64_t now_us,sb_optional_weight_sink sink,void *context);
