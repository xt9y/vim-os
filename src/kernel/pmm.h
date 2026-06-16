#pragma once

#include <stdint.h>

void pmm_init(void *memmap_response, uint64_t hhdm_offset);
uint64_t pmm_alloc(void);
void pmm_free(uint64_t phys_addr);
