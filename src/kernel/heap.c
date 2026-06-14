#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_BASE   0xFFFFF00000000000ULL
#define HEAP_ALIGN  16
#define HEADER_SZ   ((sizeof(struct block) + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1))
#define MIN_BLOCK   (HEADER_SZ + 32)
#define PAGE_SIZE   4096
#define MAGIC       0x48454150424C4B30ULL

struct block {
    size_t size;
    int free;
    uint64_t magic;
    struct block *next;
};

static struct block *heap_start = NULL;
static struct block *free_list = NULL;
static uint64_t heap_brk = 0;

static void heap_extend(size_t min)
{
    size_t pages = (min + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages < 64) pages = 64;

    uint64_t begin = heap_brk;
    for (size_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc();
        vmm_map_page(heap_brk, phys, VMM_PRESENT | VMM_WRITABLE);
        heap_brk += PAGE_SIZE;
    }

    struct block *b = (struct block *)begin;
    b->size = pages * PAGE_SIZE;
    b->free = 1;
    b->magic = MAGIC;
    b->next = free_list;
    free_list = b;
}

void heap_init(void)
{
    heap_brk = HEAP_BASE;
    heap_extend(0);
    heap_start = (struct block *)HEAP_BASE;

    serial_printf("LOG: heap: base=%p size=%u KiB\n",
                  (void *)HEAP_BASE, (unsigned)(heap_brk - HEAP_BASE) / 1024);
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    size = (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
    size_t need = size + HEADER_SZ;
    if (need < MIN_BLOCK) need = MIN_BLOCK;

    struct block **prev = &free_list;
    struct block *cur = free_list;

    while (cur) {
        if (cur->magic != MAGIC) {
            serial_printf("ERROR: heap: corruption at %p magic=0x%x\n",
                          cur, (unsigned)cur->magic);
            for (;;) __asm__("hlt");
        }
        if (cur->size >= need) {
            if (cur->size >= need + MIN_BLOCK) {
                struct block *s = (struct block *)((char *)cur + need);
                s->size = cur->size - need;
                s->free = 1;
                s->magic = MAGIC;
                s->next = cur->next;
                cur->size = need;
                cur->next = s;
            }
            *prev = cur->next;
            cur->free = 0;
            return (void *)((char *)cur + HEADER_SZ);
        }
        prev = &cur->next;
        cur = cur->next;
    }

    heap_extend(need);
    return malloc(size);
}

void free(void *ptr)
{
    if (!ptr) return;

    struct block *b = (struct block *)((char *)ptr - HEADER_SZ);
    if (b->magic != MAGIC) {
        serial_printf("ERROR: heap: invalid free %p magic=0x%x\n",
                      ptr, (unsigned)b->magic);
        for (;;) __asm__("hlt");
    }
    if (b->free) {
        serial_printf("ERROR: heap: double free %p\n", ptr);
        for (;;) __asm__("hlt");
    }

    b->free = 1;

    struct block *next = (struct block *)((char *)b + b->size);
    if ((char *)next < (char *)heap_brk && next->magic == MAGIC && next->free) {
        struct block **p = &free_list;
        while (*p && *p != next) p = &(*p)->next;
        if (*p) *p = next->next;
        b->size += next->size;
    }

    if (heap_start && heap_start != b) {
        struct block *prev = heap_start;
        while ((char *)prev < (char *)b) {
            struct block *expected = (struct block *)((char *)prev + prev->size);
            if (expected == b && prev->free) {
                struct block **p = &free_list;
                while (*p && *p != prev) p = &(*p)->next;
                if (*p) *p = prev->next;
                prev->size += b->size;
                b = prev;
                break;
            }
            if (prev->size == 0) break;
            prev = (struct block *)((char *)prev + prev->size);
        }
    }

    b->next = free_list;
    free_list = b;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        unsigned char *cp = p;
        for (size_t i = 0; i < total; i++) cp[i] = 0;
    }
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }

    struct block *b = (struct block *)((char *)ptr - HEADER_SZ);
    if (b->magic != MAGIC) {
        serial_printf("ERROR: heap: invalid realloc %p\n", ptr);
        for (;;) __asm__("hlt");
    }

    size_t old = b->size - HEADER_SZ;
    if (size <= old) return ptr;

    void *np = malloc(size);
    if (np) {
        unsigned char *s = ptr, *d = np;
        for (size_t i = 0; i < old; i++) d[i] = s[i];
        free(ptr);
    }
    return np;
}
