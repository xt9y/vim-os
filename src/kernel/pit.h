#pragma once

#include <stdint.h>

void pit_init(void);
void timer_sleep(uint32_t ms);
uint64_t pit_get_ticks(void);
