#pragma once
#include <stdint.h>

// Segment selectors (ring 0)
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_TSS         0x18

void gdt_init(void);
