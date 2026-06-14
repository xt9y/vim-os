#include "shell.h"
#include "term.h"
#include "wm.h"
#include "fs.h"
#include "string.h"
#include "serial.h"

#define MAX_ARGS 16

static void cmd_help(struct terminal *term, int argc, char **argv)
{
    (void)argc; (void)argv;
    term_write(term, "builtins: help, clear, ps, write, cat, ls, rm\nvim-cmds: :q, :q!, :n");
}

static void cmd_clear(struct terminal *term, int argc, char **argv)
{
    (void)argc; (void)argv;
    term_clear(term);
}

static void cmd_ps(struct terminal *term, int argc, char **argv)
{
    (void)argc; (void)argv;
    char buf[64];
    term_write(term, "windows:");
    for (int i = 0; i < WM_MAX; i++) {
        if (!wm_slot_used(i)) continue;
        int n = 0;
        buf[n++] = ' ';
        buf[n++] = '0' + i;
        buf[n++] = ':';
        buf[n++] = ' ';
        const char *title = wm_window_title(i);
        while (*title && n < 63) buf[n++] = *title++;
        buf[n] = '\0';
        term_write(term, buf);
    }
}

static void cmd_write(struct terminal *term, int argc, char **argv)
{
    if (argc < 3) { term_write(term, "usage: write <name> <content>"); return; }
    if (fs_write(argv[1], argv[2], strlen(argv[2])) < 0)
        term_write(term, "write failed");
}

static void cmd_cat(struct terminal *term, int argc, char **argv)
{
    if (argc < 2) { term_write(term, "usage: cat <name>"); return; }
    char buf[512];
    uint32_t size = sizeof(buf) - 1;
    if (fs_read(argv[1], buf, &size) < 0)
        { term_write(term, "file not found"); return; }
    buf[size] = '\0';
    term_write(term, buf);
}

static void cmd_ls(struct terminal *term, int argc, char **argv)
{
    (void)argc; (void)argv;
    char buf[512];
    if (fs_list(buf, sizeof(buf)) < 0)
        { term_write(term, "no files"); return; }
    if (!buf[0]) { term_write(term, "no files"); return; }
    term_write(term, buf);
}

static void cmd_rm(struct terminal *term, int argc, char **argv)
{
    if (argc < 2) { term_write(term, "usage: rm <name>"); return; }
    if (fs_delete(argv[1]) < 0)
        term_write(term, "delete failed");
}

static const struct {
    const char *name;
    void (*func)(struct terminal *, int, char **);
} builtins[] = {
    { "help",  cmd_help  },
    { "clear", cmd_clear },
    { "ps",    cmd_ps    },
    { "write", cmd_write },
    { "cat",   cmd_cat   },
    { "ls",    cmd_ls    },
    { "rm",    cmd_rm    },
    { 0, 0 }
};

int shell_exec(const char *cmd, struct terminal *term)
{
    if (!cmd || !*cmd) return 0;

    const char *p = cmd;
    while (*p == ' ') p++;
    if (!*p) return 0;

    char *argv[MAX_ARGS];
    int argc = 0;
    char buf[256];
    int bi = 0;

    for (int i = 0; i < 256; i++) buf[i] = '\0';
    int in = 0;
    for (const char *c = p; *c && argc < MAX_ARGS; c++) 
    {
        if (*c == ' ') {
            if (in) {
                buf[bi++] = '\0';
                in = 0;
            }
        } 

        else {
            if (!in) { argv[argc++] = buf + bi; in = 1; }
            buf[bi++] = *c;
        }
    }

    if (in) { buf[bi] = '\0'; }

    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(argv[0], builtins[i].name) == 0) {
            builtins[i].func(term, argc, argv);
            return 0;
        }
    }

    term_write(term, "unknown command");
    return -1;
}
