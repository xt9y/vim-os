#include "cmd.h"
#include "kb.h"
#include "serial.h"
#include "io.h"
#include <stdint.h>

#define CMD_BUF 256

static int str_eq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void process(const char *buf, int len)
{
    if (len == 0) return;
    serial_printf("\n");

    if (buf[0] != ':') return;

    if (str_eq(buf, ":q") || str_eq(buf, ":quit")) {
        serial_printf("Shutting down...\n");
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        __asm__ volatile("cli; hlt");
        for (;;);
    }

    serial_printf("  Unknown: %s\n", buf);
}

void cmd_loop(void)
{
    char buf[CMD_BUF];
    int pos = 0;

    serial_printf("Type :q to shutdown\n");

    for (;;) {
        int c = kb_getc();
        if (c < 0) {
            __asm__("pause");
            continue;
        }

        if (c == '\n' || c == '\r') {
            serial_printf("\n");
            buf[pos] = '\0';
            process(buf, pos);
            pos = 0;
        } else if ((c == '\b' || c == 0x7F) && pos > 0) {
            pos--;
        } else if (c >= ' ') {
            serial_printf("%c", c);
            buf[pos++] = c;
        }
    }
}
