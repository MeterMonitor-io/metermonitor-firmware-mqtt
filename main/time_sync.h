#pragma once
#include <stddef.h>

void time_sync_start(void);
void time_sync_wait_for_sync(void);
void time_sync_get_iso8601(char *buf, size_t len);
