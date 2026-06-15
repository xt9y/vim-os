#ifndef RTC_H
#define RTC_H

#include <stddef.h>

void rtc_init(void);
void rtc_read_time(char *buf, size_t n);

#endif
