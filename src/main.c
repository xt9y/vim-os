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

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static char kernel_stack[16384] __attribute__((aligned(16)));

#define hang for (;;) __asm__("hlt")

#include "kernel/serial.h"
#include "kernel/idt.h"
#include "kernel/pit.h"
#include "gdt.h"

void entry(void) 
{
    uintptr_t stack_top = (uintptr_t)&kernel_stack + sizeof(kernel_stack);
    __asm__ volatile("mov %0, %%rsp" :: "r"(stack_top) : "memory");

    serial_init(); serial_printf("\n\n");
    gdt_init();
    idt_init();

    // Disable local APIC (bit 11 of IA32_APIC_BASE MSR 0x1B)
    // ... so legacy PIC interrupts reach the CPU directly
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

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    serial_printf("LOG: Framebuffer: %ux%u, pitch=%u, bpp=%u\n", 
            (unsigned)fb->width, (unsigned)fb->height, 
            (unsigned)fb->pitch, (unsigned)fb->bpp);

    serial_printf("LOG: Timer test: waiting 1s...\n");
    uint64_t t0 = pit_get_ticks(); timer_sleep(1000); uint64_t dt = pit_get_ticks() - t0;
    serial_printf("LOG: Timer test: %u ticks elapsed (expect ~100)\n", (unsigned)dt);

    volatile uint32_t *pixels = fb->address;
    
    for (size_t y = 0; y < fb->height; y++)
    for (size_t x = 0; x < fb->width; x++)
        pixels[y * (fb->pitch / 4) + x] = 0x2277AA;

    hang;
}
