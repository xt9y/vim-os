#include "gfx.h"
#include "font8x16.h"

static struct {
    volatile uint32_t *fb;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} gfx;

void gfx_init(void *fb, uint32_t w, uint32_t h, uint32_t pitch) 
{
    gfx.fb     = (volatile uint32_t *)fb;
    gfx.width  = w;
    gfx.height = h;
    gfx.pitch  = pitch;
}

void gfx_clear(uint32_t color) 
{
    for (uint32_t y = 0; y < gfx.height; y++)
    for (uint32_t x = 0; x < gfx.width; x++)
        gfx.fb[(y * gfx.pitch / 4) + x] = color;
}

void gfx_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) 
{
    if (x >= gfx.width || y >= gfx.height) return;
    if (x + w > gfx.width)  w = gfx.width - x;
    if (y + h > gfx.height) h = gfx.height - y;

    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            gfx.fb[((y + row) * gfx.pitch / 4) + (x + col)] = color;
}

void gfx_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) 
{
    uint8_t (*glyph)[16] = &font8x16[(uint8_t)c];
    for (uint32_t row = 0; row < 16 && (y + row) < gfx.height; row++) {
        uint8_t bits = (*glyph)[row];
        for (uint32_t col = 0; col < 8 && (x + col) < gfx.width; col++) {
            size_t idx = ((y + row) * gfx.pitch / 4) + (x + col);
            gfx.fb[idx] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void gfx_str(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg) 
{
    while (*s) {
        if (*s == '\n') {
            x  = 0;
            y += 16;
            s++;
            continue;
        }
        gfx_char(x, y, *s, fg, bg);
        x += 8;
        s++;
    }
}
