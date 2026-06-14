#include "cmd.h"
#include "kb.h"
#include "serial.h"
#include "shell.h"
#include "term.h"
#include "wm.h"
#include "string.h"
#include "io.h"

void cmd_loop(void)
{
    int cmdline_active = 0;
    char cmdline[256];
    int cmdline_len = 0;

    for (;;) {
        int c = kb_getc();
        if (c < 0) { __asm__("pause"); continue; }

        if (cmdline_active) {
            if (c == '\n') {
                cmdline[cmdline_len] = '\0';
                const char *cmd = cmdline;
                while (*cmd == ':') cmd++;
                if (strcmp(cmd, "n") == 0) {
                    wm_new("term");
                } 

                else if (strcmp(cmd, "q") == 0) {
                    if (wm_count() > 1) wm_close_focused();
                } 

                else if (strcmp(cmd, "q!") == 0) {
                    outw(0x604, 0x2000); outw(0xB004, 0x2000);
                    __asm__ volatile("cli; hlt");
                    for (;;);
                }

                cmdline_active = 0;
                cmdline_len = 0;
            } 

            else if (c == 0x1B) {
                cmdline_active = 0;
                cmdline_len = 0;
            } 

            else if (c == '\b' || c == 0x7F) {
                if (cmdline_len > 1) cmdline_len--;
                else { cmdline_active = 0; cmdline_len = 0; }
            } 

            else if ((c & 0xFF) >= ' ' && !(c & (KB_CTRL | KB_ALT))) {
                if (cmdline_len < 255) {
                    cmdline[cmdline_len++] = c;
                    cmdline[cmdline_len] = '\0';
                }
            }
        } else {
            int ch = c & 0xFF;
            int mod = c & ~0xFF;

            if (c == ('j' | KB_CTRL)) {
                wm_focus_next();
            } 

            else if (c == ('k' | KB_CTRL)) {
                wm_focus_prev();
            } 

            else if (c == ':') {
                cmdline_active = 1;
                cmdline_len = 1;
                cmdline[0] = ':';
                cmdline[1] = '\0';
            } 

            else if (c == '\n') {
                int slot = wm_focused_slot();
                struct terminal *t = wm_get_app_data(slot);
                if (t) {
                    shell_exec(t->input, t);
                    t->input_len = 0;
                    t->input[0] = '\0';
                }
            } 

            else if (c == '\b' || c == 0x7F) {
                int slot = wm_focused_slot();
                struct terminal *t = wm_get_app_data(slot);
                if (t && t->input_len > 0) {
                    t->input_len--;
                    t->input[t->input_len] = '\0';
                }
            } 

            else if (ch >= ' ' && !(mod & (KB_CTRL | KB_ALT))) {
                int slot = wm_focused_slot();
                struct terminal *t = wm_get_app_data(slot);
                if (t && t->input_len < TERM_INPUT_MAX - 1) {
                    t->input[t->input_len++] = ch;
                    t->input[t->input_len] = '\0';
                }
            }
        }

        if (cmdline_active) wm_set_cmdline(cmdline, cmdline_len, 1);
        else wm_set_cmdline("", 0, 0);
        wm_render();
    }
}
