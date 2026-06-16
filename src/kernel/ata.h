#pragma once

#include <stdint.h>

int ata_probe(void);
int ata_read(uint32_t lba, void *buf, int count);
int ata_write(uint32_t lba, const void *buf, int count);
