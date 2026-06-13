#include "serial.h"

#define COM1 0x3F8

static uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int serial_received(void) {
    return (inb(COM1 + 5) & 1) != 0;
}

static int serial_is_transmit_empty(void) {
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) return;
    outb(COM1 + 4, 0x0F);
}

void serial_putchar(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1, (uint8_t)c);
    if (c == '\n')
        serial_putchar('\r');
}

void serial_write(const char *str) {
    for (; *str; str++)
        serial_putchar(*str);
}

void serial_printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            serial_putchar(*fmt);
            continue;
        }
        fmt++;
        switch (*fmt) {
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                serial_write(s);
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                serial_putchar(c);
                break;
            }
            case 'u': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                char buf[16], *p = buf + sizeof(buf);
                *--p = '\0';
                do { *--p = '0' + (n % 10); n /= 10; } while (n);
                serial_write(p);
                break;
            }
            case 'd':
            case 'i': {
                int n = __builtin_va_arg(args, int);
                if (n < 0) { serial_putchar('-'); n = -n; }
                char buf[16], *p = buf + sizeof(buf);
                *--p = '\0';
                do { *--p = '0' + (n % 10); n /= 10; } while (n);
                serial_write(p);
                break;
            }
            case 'x':
            case 'X': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                char buf[16], *p = buf + sizeof(buf);
                *--p = '\0';
                do { *--p = "0123456789abcdef"[n & 0xF]; n >>= 4; } while (n);
                serial_write(p);
                break;
            }
            case 'p': {
                uint64_t n = (uint64_t)__builtin_va_arg(args, void *);
                serial_write("0x");
                char buf[18], *p = buf + sizeof(buf);
                *--p = '\0';
                for (int i = 0; i < 16; i++) {
                    *--p = "0123456789abcdef"[n & 0xF]; n >>= 4;
                }
                serial_write(p);
                break;
            }
            default:
                serial_putchar(*fmt);
                break;
        }
    }
    __builtin_va_end(args);
}
