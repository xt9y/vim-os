#pragma once

#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILES 64
#define FS_NAME_LEN 64
#define FS_SECTOR_SIZE 512

int fs_init(void);
int fs_create(const char *name);
int fs_delete(const char *name);
int fs_write(const char *name, const void *data, uint32_t size);
int fs_read(const char *name, void *buf, uint32_t *size);
int fs_list(char *out, size_t out_size);
int fs_save(void);
int fs_load(void);
