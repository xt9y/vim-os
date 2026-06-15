#ifndef WM_H
#define WM_H

#include <stdint.h>

#define WM_MAX 8

enum app_type {
    APP_NONE = 0,
    APP_TEST,
    APP_TERMINAL,
};

struct window {
    char title[64];
    uint32_t bg;
    enum app_type app;
    void *app_data;
};

void wm_init(uint32_t screen_w, uint32_t screen_h);
int  wm_new(enum app_type type);
void wm_close_focused(void);
void wm_focus_next(void);
void wm_focus_prev(void);
void wm_render(void);
int  wm_count(void);
int  wm_focused_slot(void);
void *wm_get_app_data(int slot);
enum app_type wm_get_app_type(int slot);
int wm_slot_used(int slot);
const char *wm_window_title(int slot);
void wm_set_cmdline(const char *text, int len, int active);

#endif
