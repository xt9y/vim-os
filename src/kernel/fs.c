#include "fs.h"
#include "string.h"
#include "heap.h"
#include "serial.h"
#include "ata.h"

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
    fs_save();
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
    fs_save();
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

#define DIR_LBA 1
#define DIR_SECTORS 8
#define DATA_LBA (DIR_LBA + DIR_SECTORS)

struct __attribute__((packed)) dir_entry {
    char name[48];
    uint32_t size;
    uint32_t start;
    uint8_t used;
    uint8_t pad[7];
};

int fs_save(void)
{
    uint8_t *sb = malloc(FS_SECTOR_SIZE);
    uint8_t *dir = malloc(FS_SECTOR_SIZE * DIR_SECTORS);
    if (!sb || !dir) { free(sb); free(dir); return -1; }

    memset(sb, 0, FS_SECTOR_SIZE);
    sb[0] = 'O'; sb[1] = 'S'; sb[2] = 'F'; sb[3] = 'S';

    memset(dir, 0, FS_SECTOR_SIZE * DIR_SECTORS);
    struct dir_entry *entries = (struct dir_entry *)dir;
    uint32_t next_lba = DATA_LBA;
    int file_count = 0;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;
        strncpy(entries[file_count].name, files[i].name, 47);
        entries[file_count].name[47] = '\0';
        entries[file_count].size = files[i].size;
        entries[file_count].start = next_lba;
        entries[file_count].used = 1;
        int sectors = files[i].size ? (files[i].size + 511) / 512 : 1;
        if (ata_write(next_lba, files[i].data ? files[i].data : (uint8_t *)"", sectors) < 0)
            { free(sb); free(dir); return -1; }
        next_lba += sectors;
        file_count++;
    }

    if (ata_write(0, sb, 1) < 0) { free(sb); free(dir); return -1; }
    if (ata_write(DIR_LBA, dir, DIR_SECTORS) < 0) { free(sb); free(dir); return -1; }

    free(sb); free(dir);
    serial_printf("LOG: FS: saved %d files to disk\n", file_count);
    return 0;
}

int fs_load(void)
{
    uint8_t *sb = malloc(FS_SECTOR_SIZE);
    uint8_t *dir = malloc(FS_SECTOR_SIZE * DIR_SECTORS);
    if (!sb || !dir) { free(sb); free(dir); return -1; }

    if (ata_read(0, sb, 1) < 0) { free(sb); free(dir); serial_printf("LOG: FS: no disk, starting empty\n"); return -1; }
    if (sb[0] != 'O' || sb[1] != 'S' || sb[2] != 'F' || sb[3] != 'S')
        { free(sb); free(dir); serial_printf("LOG: FS: fresh disk, starting empty\n"); return -1; }

    if (ata_read(DIR_LBA, dir, DIR_SECTORS) < 0) { free(sb); free(dir); return -1; }

    struct dir_entry *entries = (struct dir_entry *)dir;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!entries[i].used) continue;
        int slot = find_free();
        if (slot < 0) break;
        strncpy(files[slot].name, entries[i].name, FS_NAME_LEN - 1);
        files[slot].name[FS_NAME_LEN - 1] = '\0';
        files[slot].size = entries[i].size;
        files[slot].used = 1;

        if (entries[i].size > 0) {
            files[slot].data = malloc(entries[i].size);
            if (files[slot].data) {
                int sectors = (entries[i].size + 511) / 512;
                uint8_t *tmp = malloc(sectors * FS_SECTOR_SIZE);
                if (tmp) {
                    ata_read(entries[i].start, tmp, sectors);
                    memcpy(files[slot].data, tmp, entries[i].size);
                    free(tmp);
                }
            }
        } else {
            files[slot].data = 0;
        }
    }

    free(sb); free(dir);
    serial_printf("LOG: FS: loaded from disk\n");
    return 0;
}
