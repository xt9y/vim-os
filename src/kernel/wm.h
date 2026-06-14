#ifndef WM_H
#define WM_H

#include <stdint.h>

#define WM_MAX 8

struct window {
    char title[64];
    uint32_t bg;
    uint32_t x, y, w, h;
};

void wm_init(uint32_t screen_w, uint32_t screen_h);
int  wm_new(const char *title);
void wm_close_focused(void);
void wm_focus_next(void);
void wm_focus_prev(void);
void wm_render(void);
int  wm_count(void);
void wm_focused_rect(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
struct window wm_focused_win(void);

#endif
