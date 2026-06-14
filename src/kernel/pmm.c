#include "pmm.h"
#include "serial.h"
#include <limine.h>
#include <stdint.h>
#include <stddef.h>

#define FRAME_SIZE    4096

static uint8_t *bitmap = NULL;
static uint64_t total_frames = 0;
static uint64_t hhdm = 0;

static inline void bitmap_set(uint64_t frame)
{
    bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint64_t frame)
{
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline int bitmap_test(uint64_t frame)
{
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

void pmm_init(void *memmap_response, uint64_t hhdm_offset)
{
    struct limine_memmap_response *r = memmap_response;
    hhdm = hhdm_offset;

    uint64_t highest = 0;
    for (uint64_t i = 0; i < r->entry_count; i++) {
        struct limine_memmap_entry *e = r->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            uint64_t end = e->base + e->length;
            if (end > highest) highest = end;
        }
    }

    total_frames = highest / FRAME_SIZE;
    if (total_frames == 0) total_frames = 1;
    uint64_t bitmap_size = (total_frames + 7) / 8;

    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < r->entry_count; i++) {
        struct limine_memmap_entry *e = r->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size
            && e->base >= 0x100000 && !bitmap_phys)
            bitmap_phys = e->base;
    }

    if (!bitmap_phys) {
        serial_printf("ERROR: PMM: no region fits bitmap (%u bytes)\n", (unsigned)bitmap_size);
        for (;;) __asm__("hlt");
    }

    bitmap = (uint8_t *)(bitmap_phys + hhdm);

    serial_printf("LOG: PMM: total_mem=%u MiB, frames=%u, bitmap=%u bytes at phys=0x%x\n",
                  (unsigned)(highest / 1048576), (unsigned)total_frames,
                  (unsigned)bitmap_size, (unsigned)bitmap_phys);

    for (uint64_t i = 0; i < bitmap_size; i++)
        bitmap[i] = 0xFF;

    for (uint64_t i = 0; i < r->entry_count; i++) {
        struct limine_memmap_entry *e = r->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start = (e->base + FRAME_SIZE - 1) / FRAME_SIZE;
            uint64_t end   = (e->base + e->length) / FRAME_SIZE;
            if (end > total_frames) end = total_frames;
            for (uint64_t f = start; f < end; f++)
                bitmap_clear(f);
        } else {
            uint64_t start = e->base / FRAME_SIZE;
            uint64_t end   = (e->base + e->length + FRAME_SIZE - 1) / FRAME_SIZE;
            if (start >= total_frames) continue;
            if (end > total_frames) end = total_frames;
            for (uint64_t f = start; f < end; f++)
                bitmap_set(f);
        }
    }

    uint64_t b_start = bitmap_phys / FRAME_SIZE;
    uint64_t b_end   = (bitmap_phys + bitmap_size + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint64_t f = b_start; f < b_end; f++)
        bitmap_set(f);

    uint64_t protected_end = 0x100000 / FRAME_SIZE;
    for (uint64_t f = 0; f < protected_end; f++)
        bitmap_set(f);

    uint64_t free_frames = 0;
    for (uint64_t f = 0; f < total_frames; f++)
        if (!bitmap_test(f)) free_frames++;

    serial_printf("LOG: PMM: %u free frames (%u MiB)\n",
                  (unsigned)free_frames, (unsigned)(free_frames * 4 / 1024));
}

uint64_t pmm_alloc(void)
{
    for (uint64_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            return f * FRAME_SIZE;
        }
    }
    serial_printf("ERROR: PMM: out of memory\n");
    return 0;
}

void pmm_free(uint64_t phys_addr)
{
    if (!phys_addr) return;
    uint64_t f = phys_addr / FRAME_SIZE;
    if (f >= total_frames) return;
    bitmap_clear(f);
}
