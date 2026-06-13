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

#define hang for (;;) __asm__("hlt")

void entry(void) 
{
    if (!LIMINE_BASE_REVISION_SUPPORTED) hang;
    if (!fb_request.response || fb_request.response->framebuffer_count < 1) hang;

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    volatile uint32_t *pixels = fb->address;

    for (size_t y = 0; y < fb->height; y++)
    for (size_t x = 0; x < fb->width; x++)
        pixels[y * (fb->pitch / 4) + x] = 0x2277AA;

    hang;
}
