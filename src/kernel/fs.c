#include "fs.h"
#include "string.h"
#include "heap.h"
#include "serial.h"

static struct {
    char name[FS_NAME_LEN];
    uint8_t *data;
    uint32_t size;
    int used;
} files[FS_MAX_FILES];

int fs_init(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++)
        files[i].used = 0;
    serial_printf("LOG: FS: in-memory filesystem ready (%d files max)\n", FS_MAX_FILES);
    return 0;
}

static int find_file(const char *name)
{
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && strcmp(files[i].name, name) == 0)
            return i;
    return -1;
}

static int find_free(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!files[i].used) return i;
    return -1;
}

int fs_create(const char *name)
{
    if (!name || !*name) return -1;
    if (find_file(name) >= 0) return -1;
    int i = find_free();
    if (i < 0) return -1;
    strncpy(files[i].name, name, FS_NAME_LEN - 1);
    files[i].name[FS_NAME_LEN - 1] = '\0';
    files[i].data = 0;
    files[i].size = 0;
    files[i].used = 1;
    return 0;
}

int fs_delete(const char *name)
{
    int i = find_file(name);
    if (i < 0) return -1;
    if (files[i].data) free(files[i].data);
    files[i].data = 0;
    files[i].size = 0;
    files[i].used = 0;
    return 0;
}

int fs_write(const char *name, const void *data, uint32_t size)
{
    int i = find_file(name);
    if (i < 0) {
        if (fs_create(name) < 0) return -1;
        i = find_file(name);
    }
    if (files[i].data) free(files[i].data);
    files[i].data = malloc(size ? size : 1);
    if (!files[i].data && size) return -1;
    if (size) memcpy(files[i].data, data, size);
    files[i].size = size;
    return (int)size;
}

int fs_read(const char *name, void *buf, uint32_t *size)
{
    int i = find_file(name);
    if (i < 0) return -1;
    if (!buf || !size) return -1;
    uint32_t n = *size < files[i].size ? *size : files[i].size;
    memcpy(buf, files[i].data, n);
    *size = n;
    return (int)n;
}

int fs_list(char *out, size_t out_size)
{
    if (!out || out_size == 0) return -1;
    int pos = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;
        int n = strlen(files[i].name);
        if (pos + n + 2 > (int)out_size) break;
        memcpy(out + pos, files[i].name, n);
        pos += n;
        out[pos++] = '\n';
    }
    if (pos > 0) pos--;
    out[pos] = '\0';
    return pos;
}
