#pragma once
#include <stdbool.h>
/* Optional board capability. Boards without it keep the original UI behavior. */
#ifdef SB_ENABLE_BOOT_SLEEP
void sb_sleep_init(void);
bool sb_sleep_blank(void);
void sb_sleep_wake(void);
#else
static inline void sb_sleep_init(void) {}
static inline bool sb_sleep_blank(void) { return false; }
static inline void sb_sleep_wake(void) {}
#endif
