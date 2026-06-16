#pragma once

#include <stddef.h>

void rtc_init(void);
void rtc_read_time(char *buf, size_t n);
