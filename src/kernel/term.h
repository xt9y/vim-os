#pragma once

#include <stdint.h>
#include <stddef.h>

#define TERM_MAX_LINES 1024
#define TERM_INPUT_MAX 256

struct term_line {
    char *text;
    int len;
};

struct terminal {
    uint32_t x, y, w, h;
    uint32_t fg, bg;
    struct term_line lines[TERM_MAX_LINES];
    int line_count;
    char input[TERM_INPUT_MAX];
    int input_len;
};

struct terminal *term_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void term_destroy(struct terminal *t);
void term_write(struct terminal *t, const char *s);
void term_putchar(struct terminal *t, char c);
void term_clear(struct terminal *t);
void term_render(struct terminal *t);
