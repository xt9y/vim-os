#include "kb.h"
#include "idt.h"
#include "io.h"
#include "serial.h"
#include "config.h"
#include "string.h"
#include <stdint.h>

#define PS2_DATA  0x60
#define PS2_STAT  0x64
#define RING_SZ   256

static volatile int ring[RING_SZ];
static volatile int head, tail;
static int shift, ctrl, alt, caps;
static int ext;
static const unsigned char *norm, *shft;

/* UEFI ConIn fallback for real hw without PS/2 */
static void *conin;
typedef uint64_t (__attribute__((ms_abi)) *read_key_stroke_t)(void *, void *);

struct efi_key {
    uint16_t scan;
    uint16_t code;
};

static const unsigned char layout_us_normal[128] = {
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

static const unsigned char layout_us_shifted[128] = {
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

static const unsigned char layout_de_normal[128] = {
    0,   0x1B, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', 0xDF, 0xB4, '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
    'o', 'p', 0xFC, '+', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xF6,
    0xE4, '^', 0,   '#', 'y', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '-', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2', '3', '0', '.', 0,   0,   0,
    0
};

static const unsigned char layout_de_shifted[128] = {
    0,   0x1B, '!', '"', 0xA7, '$', '%', '&',
    '/', '(', ')', '=', '?', '`', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
    'O', 'P', 0xDC, '*', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xD6,
    0xC4, 0xB0, 0,   '\'', 'Y', 'X', 'C', 'V',
    'B', 'N', 'M', ';', ':', '_', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2', '3', '0', '.', 0,   0,   0,
    0
};

static void ring_put(int c)
{
    int next = (head + 1) % RING_SZ;
    if (next != tail) {
        ring[head] = c;
        head = next;
    }
}

int kb_getc(void)
{
    if (head != tail) {
        int c = ring[tail];
        tail = (tail + 1) % RING_SZ;
        return c;
    }
    if (conin) {
        static struct efi_key k;
        read_key_stroke_t rk = *(read_key_stroke_t *)(conin + 8);
        if (rk(conin, &k) == 0) {
            if (k.code == 0x0D) return '\n';
            if (k.code == 0x08) return '\b';
            if (k.code) {
                /* Reverse-lookup through US layout to find scan code,
                   then forward-map through configured layout */
                for (int i = 0; i < 128; i++) {
                    if (layout_us_normal[i] == k.code) {
                        unsigned char c = norm[i];
                        if (c) return c;
                        break;
                    }
                    if (layout_us_shifted[i] == k.code) {
                        unsigned char c = shft[i];
                        if (c) return c;
                        break;
                    }
                }
                return k.code;
            }
        }
    }
    return -1;
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
        if (code == 0x1C) {
            ring_put('\n');
        } else {
            int mod = 0;
            if (ctrl) mod |= KB_CTRL;
            if (alt)  mod |= KB_ALT;

            unsigned char c = shift ? shft[code] : norm[code];
            if (c >= 'a' && c <= 'z' && caps) c -= 0x20;
            if (c >= 'A' && c <= 'Z' && caps && shift) c += 0x20;
            if (c >= 0xC0 && caps) {
                if (shift && c >= 0xE0) c -= 0x20;
                else if (!shift && c >= 0xC0 && c < 0xE0) c += 0x20;
            }
            if (c) ring_put(c | mod);
        }
    }

done:
    pic_eoi(1);
}

void kb_set_conin(void *c)
{
    conin = c;
}

void kb_reload_layout(void)
{
    const char *layout = config_get("keyboard_layout");
    if (layout && strcmp(layout, "de") == 0) {
        norm = layout_de_normal;
        shft = layout_de_shifted;
        serial_printf("LOG: KB: German layout\n");
    } else {
        norm = layout_us_normal;
        shft = layout_us_shifted;
        serial_printf("LOG: KB: US layout\n");
    }
}

void kb_init(void)
{
    head = tail = 0;
    shift = ctrl = alt = caps = ext = 0;

    /* ps2 is best-effort: drain with timeout (hw may have no controller) */
    uint8_t stat = inb(PS2_STAT);
    if (stat != 0xFF) {
        for (int i = 0; i < 255 && (inb(PS2_STAT) & 1); i++)
            inb(PS2_DATA);
    }

    kb_reload_layout();
}
