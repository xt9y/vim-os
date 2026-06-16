#pragma once

#include <stdint.h>
#include <stddef.h>

void gfx_init(void *fb, uint32_t w, uint32_t h, uint32_t pitch);
void gfx_clear(uint32_t color);
void gfx_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void gfx_str(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg);
