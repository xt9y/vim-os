#include "term.h"
#include "gfx.h"
#include "string.h"
#include "heap.h"

struct terminal *term_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    struct terminal *t = malloc(sizeof(struct terminal));
    if (!t) return 0;
    t->x = x; t->y = y; t->w = w; t->h = h;
    t->fg = 0x00FFFFFF;
    t->bg = 0x00000000;
    t->line_count = 0;
    t->input_len = 0;
    t->input[0] = '\0';
    for (int i = 0; i < TERM_MAX_LINES; i++)
        t->lines[i].text = 0;
    return t;
}

void term_destroy(struct terminal *t)
{
    if (!t) return;
    for (int i = 0; i < TERM_MAX_LINES; i++)
        if (t->lines[i].text) free(t->lines[i].text);
    free(t);
}

void term_write(struct terminal *t, const char *s)
{
    if (!t || !s) return;
    while (*s) {
        const char *end = s;
        while (*end && *end != '\n') end++;
        int idx = t->line_count % TERM_MAX_LINES;
        if (t->lines[idx].text) free(t->lines[idx].text);
        int len = (int)(end - s);
        t->lines[idx].text = malloc(len + 1);
        if (t->lines[idx].text) {
            memcpy(t->lines[idx].text, s, len);
            t->lines[idx].text[len] = '\0';
            t->lines[idx].len = len;
        }
        t->line_count++;
        s = (*end == '\n') ? end + 1 : end;
    }
}

void term_putchar(struct terminal *t, char c)
{
    if (!t) return;
    char s[2] = { c, '\0' };
    term_write(t, s);
}

void term_clear(struct terminal *t)
{
    if (!t) return;
    for (int i = 0; i < TERM_MAX_LINES; i++) {
        if (t->lines[i].text) { free(t->lines[i].text); t->lines[i].text = 0; }
        t->lines[i].len = 0;
    }
    t->line_count = 0;
}

void term_render(struct terminal *t)
{
    if (!t) return;
    int rows = (int)(t->h / 16);
    int cols = (int)(t->w / 8);
    if (rows < 1 || cols < 1) return;

    gfx_rect(t->x, t->y, t->w, t->h, t->bg);

    int total = t->line_count;
    int show_rows = rows - 1;

    int start = total > show_rows ? total - show_rows : 0;

    for (int i = 0; i < show_rows && start + i < total; i++) {
        int idx = (start + i) % TERM_MAX_LINES;
        if (t->lines[idx].text)
            gfx_str(t->x, t->y + i * 16, t->lines[idx].text, t->fg, t->bg);
    }

    int input_row = rows - 1;
    gfx_str(t->x, t->y + input_row * 16, t->input, t->fg, t->bg);

    int cx = (t->input_len < cols) ? t->input_len * 8 : (cols - 1) * 8;
    gfx_rect(t->x + cx, t->y + input_row * 16, 1, 16, t->fg);
}
