#include <stdint.h>
#include <stddef.h>
#include <limine.h>

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static char kernel_stack[16384] __attribute__((aligned(16)));

#define hang for (;;) __asm__("hlt")

#include "kernel/serial.h"
#include "kernel/idt.h"
#include "kernel/pit.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "kernel/kb.h"
#include "kernel/cmd.h"
#include "kernel/gfx.h"
#include "kernel/wm.h"
#include "kernel/fs.h"
#include "gdt.h"

static struct limine_framebuffer *fb;

void entry(void) 
{
    uintptr_t stack_top = (uintptr_t)&kernel_stack + sizeof(kernel_stack);
    __asm__ volatile("mov %0, %%rsp" :: "r"(stack_top) : "memory");

    serial_init(); serial_printf("\n\n");
    gdt_init();
    idt_init();

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1Bu)); lo &= ~(1 << 11);
    __asm__ volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0x1Bu));

    pit_init();
    __asm__ volatile("sti");

    serial_printf("LOG: Kernel booted. Stack at %p\n", (void *)stack_top);

    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        serial_printf("ERROR: Limine base revision not supported\n"); hang;
    }
    if (!fb_request.response || fb_request.response->framebuffer_count < 1) {
        serial_printf("ERROR: No framebuffer found\n"); hang;
    }

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    vmm_init(hhdm_request.response->offset);
    heap_init();
    fs_init();
    kb_init();

    fb = fb_request.response->framebuffers[0];
    serial_printf("LOG: Framebuffer: %ux%u %u bpp\n", (unsigned)fb->width, (unsigned)fb->height, (unsigned)fb->bpp);
    gfx_init((void *)fb->address, fb->width, fb->height, fb->pitch);
    wm_init(fb->width, fb->height); wm_new("window"); 
    
    cmd_loop();
}
