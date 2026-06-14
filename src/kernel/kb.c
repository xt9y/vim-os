#include "kb.h"
#include "idt.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

#define PS2_DATA  0x60
#define PS2_STAT  0x64
#define PS2_CMD   0x64

#define RING_SZ   256

static volatile char ring[RING_SZ];
static volatile int ring_head, ring_tail;

static int shift, ctrl, alt, caps;

// scancode set 1: make code 0x01-0x58 -> ASCII, 0 = unhandled
static const unsigned char sc_ascii[128] = {
    0,   0x1B, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2', '3', '0', '.', 0,   0,   0,
    0
};

static const unsigned char sc_shift[128] = {
    0,   0x1B, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2', '3', '0', '.', 0,   0,   0,
    0
};

static void ring_put(char c)
{
    int next = (ring_head + 1) % RING_SZ;
    if (next != ring_tail) {
        ring[ring_head] = c;
        ring_head = next;
    }
}

int kb_getc(void)
{
    if (ring_head == ring_tail) return -1;
    char c = ring[ring_tail];
    ring_tail = (ring_tail + 1) % RING_SZ;
    return c;
}

static int ext;

__attribute__((interrupt))
void irq_keyboard(struct interrupt_frame *f)
{
    (void)f;
    uint8_t sc = inb(PS2_DATA);

    if (sc == 0xE0) { ext = 1; goto done; }
    if (sc == 0xE1) { ext = 2; goto done; }

    int make = !(sc & 0x80);
    uint8_t code = sc & 0x7F;

    if (ext == 2) { ext = 0; goto done; }

    if (ext) {
        ext = 0;
        if (code == 0x1C) { if (make) ring_put('\n'); }
        if (code == 0x38) { alt = make; }
        if (code == 0x1D) { ctrl = make; }
        goto done;
    }

    if (code == 0x2A || code == 0x36) { shift = make; goto done; }
    if (code == 0x1D) { ctrl = make; goto done; }
    if (code == 0x38) { alt = make; goto done; }
    if (code == 0x3A && make) { caps = !caps; goto done; }

    if (make && code < 128) {
        unsigned char c = shift ? sc_shift[code] : sc_ascii[code];
        if (c >= 'a' && c <= 'z' && caps) c -= 0x20;
        if (c >= 'A' && c <= 'Z' && caps && shift) c += 0x20;
        if (c) ring_put(c);
    }

done:
    pic_eoi(1);
}

void kb_init(void)
{
    ext = 0;
    ring_head = ring_tail = 0;
    shift = ctrl = alt = caps = 0;

    // Flush any stale data
    while (inb(PS2_STAT) & 1) inb(PS2_DATA);

    // Disable, flush, enable first PS/2 port
    outb(PS2_CMD, 0xAD);
    while (inb(PS2_STAT) & 1) inb(PS2_DATA);
    outb(PS2_CMD, 0xAE);

    // Set scancode set 1
    while (inb(PS2_STAT) & 2);
    outb(PS2_DATA, 0xF0);
    while (inb(PS2_STAT) & 2);
    outb(PS2_DATA, 0x01);

    serial_printf("LOG: PS/2 keyboard init OK\n");
}
