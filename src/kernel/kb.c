#include "kb.h"
#include "idt.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

#define PS2_DATA  0x60
#define PS2_STAT  0x64
#define RING_SZ   256

static volatile unsigned char ring[RING_SZ];
static volatile int head, tail;
static int shift, ctrl, alt, caps;
static int ext;

// scancode set 1: make code 0x01-0x58 -> ASCII
static const unsigned char normal[128] = {
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

static const unsigned char shifted[128] = {
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

static void ring_put(unsigned char c)
{
    int next = (head + 1) % RING_SZ;
    if (next != tail) {
        ring[head] = c;
        head = next;
    }
}

int kb_getc(void)
{
    if (head == tail) return -1;
    unsigned char c = ring[tail];
    tail = (tail + 1) % RING_SZ;
    return c;
}

__attribute__((interrupt))
void irq_keyboard(struct interrupt_frame *f)
{
    (void)f;
    unsigned char sc = inb(PS2_DATA);

    if (sc == 0xE0) { ext = 1; goto done; }
    if (sc == 0xE1) { ext = 2; goto done; }

    int make = !(sc & 0x80);
    unsigned char code = sc & 0x7F;

    if (ext == 2) { ext = 0; goto done; }

    if (ext) {
        ext = 0;
        if (code == 0x1C && make) ring_put('\n');
        if (code == 0x38) alt = make;
        if (code == 0x1D) ctrl = make;
        goto done;
    }

    if (code == 0x2A || code == 0x36) { shift = make; goto done; }
    if (code == 0x1D) { ctrl = make; goto done; }
    if (code == 0x38) { alt = make; goto done; }
    if (code == 0x3A && make) { caps = !caps; goto done; }

    if (make && code < 128) {
        unsigned char c = shift ? shifted[code] : normal[code];
        if (c >= 'a' && c <= 'z' && caps) c -= 0x20;
        if (c >= 'A' && c <= 'Z' && caps && shift) c += 0x20;
        if (c) ring_put(c);
    }

done:
    pic_eoi(1);
}

void kb_init(void)
{
    head = tail = 0;
    shift = ctrl = alt = caps = ext = 0;
    while (inb(PS2_STAT) & 1) inb(PS2_DATA);
    serial_printf("LOG: KB init done (set 1)\n");
}
