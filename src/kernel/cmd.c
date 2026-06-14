#include "cmd.h"
#include "kb.h"
#include "serial.h"
#include "io.h"
#include "wm.h"
#include <stdint.h>

#define CMD_BUF 256

static char buf[CMD_BUF];
static int pos = 0;

static void render(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *buf);

static int str_eq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void process()
{
    if (pos == 0) return;
    serial_printf("\n");

    if (buf[0] != ':') return;

    if (str_eq(buf, ":q") || str_eq(buf, ":quit")) {
        if (wm_count() > 1) {
            serial_printf("LOG: HOTKEY: WINDOW: closing window\n");
            wm_close_focused();
        } else {
            serial_printf("LOG: HOTKEY: Shutting down...\n");
            outw(0x604, 0x2000); outw(0xB004, 0x2000);
            __asm__ volatile("cli; hlt");
            for (;;);
        }
    }

    else if (str_eq(buf, ":n") || str_eq(buf, ":new")) {
        int ret = wm_new("root");
        if (ret < 0) serial_printf("LOG: HOTKEY: WINDOW: max windows\n");
        else         serial_printf("LOG: HOTKEY: WINDOW: new window\n");
    }

    else serial_printf("LOG: HOTKEY: Unknown: %s\n", buf);
}

void cmd_loop(void)
{
    for (;;) {
        int c = kb_getc();
        if (c < 0) { __asm__("pause"); continue; }

        if (pos == 0) {
            if (c == 'j') { wm_focus_next(); }
            else if (c == 'k') { wm_focus_prev(); }
            else if (c == ':') { serial_printf(":"); buf[pos++] = ':'; }
        } else {
            if (c == '\n') {
                serial_printf("\n");
                buf[pos] = '\0';
                process();
                pos = 0; buf[0] = '\0';
            } else if ((c == '\b' || c == 0x7F) && pos > 1) {
                pos--;
            } else if (c == '\b' || c == 0x7F) {
                pos = 0; buf[0] = '\0';
            } else if (c >= ' ' && pos < CMD_BUF - 1) {
                serial_printf("%c", c);
                buf[pos++] = c;
            }
        }

        uint32_t fx, fy, fw, fh;
        wm_focused_rect(&fx, &fy, &fw, &fh);
        wm_render(); render(fx, fy, fw, fh, buf);
    }
}
