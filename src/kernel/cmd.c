#include "cmd.h"
#include "kb.h"
#include "serial.h"
#include "shell.h"
#include "term.h"
#include "wm.h"
#include "string.h"
#include "io.h"
#include "fs.h"
#include "acpi.h"
#include "config.h"

void cmd_loop(void)
{
    int cmdline_active = 0;
    char cmdline[256];
    int cmdline_len = 0;

    wm_render();

    for (;;) {
        int c = kb_getc();
        if (c < 0) { __asm__("pause"); continue; }

        if (cmdline_active) {
            if (c == '\n') {
                cmdline[cmdline_len] = '\0';
                const char *cmd = cmdline;
                while (*cmd == ':') cmd++;

                if (strncmp(cmd, "n", 1) == 0) {
                    const char *arg = cmd + 1;
                    while (*arg == ' ') arg++;
                    enum app_type type = APP_TERMINAL;
                    if (*arg) {
                        if (strcmp(arg, "APP_TERMINAL") == 0) type = APP_TERMINAL;
                        if (strcmp(arg, "APP_TEST") == 0) type = APP_TEST;
                    }
                    wm_new(type); 
                } 

                else if (strcmp(cmd, "q") == 0) {
                    if (wm_count() > 1) wm_close_focused();
                } 

                else if (strcmp(cmd, "q!") == 0) {
                    fs_save();
                    acpi_poweroff();
                } 

                else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "reboot") == 0) {
                    fs_save();
                    outb(0x64, 0xFE);
                    __asm__ volatile("cli; hlt");
                } 

                else if (strncmp(cmd, "set ", 4) == 0) {
                    const char *p = cmd + 4;
                    while (*p == ' ') p++;
                    char key[48] = {0}, val[64] = {0};
                    int i;
                    for (i = 0; *p && *p != ' ' && i < 47; i++, p++) key[i] = *p;

                    while (*p == ' ') p++;

                    for (i = 0; *p && i < 63; i++, p++) val[i] = *p;

                    if (*key && *val)
                    {
                        char buf[2048], out[2048];
                        uint32_t size = sizeof(buf) - 1;

                        if (fs_read("config", buf, &size) >= 0) {
                            buf[size] = '\0';
                            char *in = buf;
                            char *o = out;
                            int found = 0;
                            while (*in)
                            {
                                char *nl = strchr(in, '\n');
                                int len = nl ? (int)(nl - in) : (int)strlen(in);

                                if (!found && strncmp(in, "#define", 7) == 0) {
                                    char *ks = in + 7;
                                    while (*ks == ' ' || *ks == '\t') ks++;
                                    int klen = 0;
                                    while (ks[klen] && ks[klen] != ' ' && ks[klen] != '\t') klen++;

                                    if (strncmp(ks, key, klen) == 0 && (int)strlen(key) == klen) 
                                    {
                                        char *pre = "#define "; while (*pre) *o++ = *pre++;
                                        char *k = key; while (*k) *o++ = *k++;
                                        *o++ = ' ';
                                        char *v = val; while (*v) *o++ = *v++;
                                        *o++ = '\n';
                                        found = 1;
                                        in = nl ? nl + 1 : (in + len);
                                        continue;
                                    }
                                }

                                memcpy(o, in, len); o += len;
                                if (nl) *o++ = '\n';
                                in = nl ? nl + 1 : (in + len);
                            }

                            if (!found) {
                                char *pre = "#define "; while (*pre) *o++ = *pre++;
                                char *k = key; while (*k) *o++ = *k++;
                                *o++ = ' ';
                                char *v = val; while (*v) *o++ = *v++;
                                *o++ = '\n';
                            }

                            *o = '\0';
                            fs_write("config", out, o - out);
                            if (strcmp(key, "keyboard_layout") == 0)
                                kb_reload_layout();
                        }
                    }
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
