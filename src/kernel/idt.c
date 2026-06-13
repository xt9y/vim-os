#include "idt.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

#define hang for (;;) __asm__("hlt")

// --- IDT entry (16 bytes per entry) ---
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

static struct idt_entry idt[256] __attribute__((aligned(16)));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static void set_idt_entry(int vector, void *handler, uint8_t type) 
{
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low   = addr & 0xFFFF;
    idt[vector].selector     = 0x08;
    idt[vector].ist          = 0;
    idt[vector].attributes   = type;
    idt[vector].offset_mid   = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high  = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved     = 0;
}

// --- PIC (8259A) ---
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

void pic_eoi(int irq) 
{
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

static void pic_remap(void) 
{
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

// --- Exception handler stubs ---

static const char *exc_name[] = {
    "DE", "DB", "NMI", "BP", "OF", "BR", "UD", "NM",
    "DF", "CSO","TS", "NP", "SS", "GP", "PF", "15",
    "MF", "AC", "MC", "XM", "VE", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "30", "31"
};

#define EXC(n) __attribute__((interrupt)) void exc_##n(struct interrupt_frame *f)
#define EXC_ERR(n) __attribute__((interrupt)) void exc_##n(struct interrupt_frame *f, uint64_t err)

EXC(0)  { serial_printf("\n!!! EXC %d: %s", 0, exc_name[0]); hang; }
EXC(1)  { serial_printf("\n!!! EXC %d: %s", 1, exc_name[1]); hang; }
EXC(2)  { serial_printf("\n!!! EXC %d: %s", 2, exc_name[2]); hang; }
EXC(3)  { serial_printf("\n!!! EXC %d: %s", 3, exc_name[3]); hang; }
EXC(4)  { serial_printf("\n!!! EXC %d: %s", 4, exc_name[4]); hang; }
EXC(5)  { serial_printf("\n!!! EXC %d: %s", 5, exc_name[5]); hang; }
EXC(6)  { serial_printf("\n!!! EXC %d: %s", 6, exc_name[6]); hang; }
EXC(7)  { serial_printf("\n!!! EXC %d: %s", 7, exc_name[7]); hang; }
EXC_ERR(8)  { serial_printf("\n!!! EXC %d: %s err=0x%x", 8, exc_name[8], (unsigned)err); hang; }
EXC(9)  { serial_printf("\n!!! EXC %d: %s", 9, exc_name[9]); hang; }
EXC_ERR(10) { serial_printf("\n!!! EXC %d: %s err=0x%x",10, exc_name[10], (unsigned)err); hang; }
EXC_ERR(11) { serial_printf("\n!!! EXC %d: %s err=0x%x",11, exc_name[11], (unsigned)err); hang; }
EXC_ERR(12) { serial_printf("\n!!! EXC %d: %s err=0x%x",12, exc_name[12], (unsigned)err); hang; }
EXC_ERR(13) { serial_printf("\n!!! EXC %d: %s err=0x%x",13, exc_name[13], (unsigned)err); hang; }
EXC_ERR(14) { serial_printf("\n!!! EXC %d: %s err=0x%x",14, exc_name[14], (unsigned)err); hang; }
EXC(15) { serial_printf("\n!!! EXC %d: %s",15, exc_name[15]); hang; }
EXC(16) { serial_printf("\n!!! EXC %d: %s",16, exc_name[16]); hang; }
EXC_ERR(17) { serial_printf("\n!!! EXC %d: %s err=0x%x",17, exc_name[17], (unsigned)err); hang; }
EXC(18) { serial_printf("\n!!! EXC %d: %s",18, exc_name[18]); hang; }
EXC(19) { serial_printf("\n!!! EXC %d: %s",19, exc_name[19]); hang; }
EXC(20) { serial_printf("\n!!! EXC %d: %s",20, exc_name[20]); hang; }
EXC(21) { serial_printf("\n!!! EXC %d: %s",21, exc_name[21]); hang; }
EXC(22) { serial_printf("\n!!! EXC %d: %s",22, exc_name[22]); hang; }
EXC(23) { serial_printf("\n!!! EXC %d: %s",23, exc_name[23]); hang; }
EXC(24) { serial_printf("\n!!! EXC %d: %s",24, exc_name[24]); hang; }
EXC(25) { serial_printf("\n!!! EXC %d: %s",25, exc_name[25]); hang; }
EXC(26) { serial_printf("\n!!! EXC %d: %s",26, exc_name[26]); hang; }
EXC(27) { serial_printf("\n!!! EXC %d: %s",27, exc_name[27]); hang; }
EXC(28) { serial_printf("\n!!! EXC %d: %s",28, exc_name[28]); hang; }
EXC(29) { serial_printf("\n!!! EXC %d: %s",29, exc_name[29]); hang; }
EXC(30) { serial_printf("\n!!! EXC %d: %s",30, exc_name[30]); hang; }
EXC(31) { serial_printf("\n!!! EXC %d: %s",31, exc_name[31]); hang; }

// --- IRQ stubs (forward declarations — implemented in pit.c, keyboard later) ---
__attribute__((interrupt)) void irq_timer(struct interrupt_frame *f);
__attribute__((interrupt)) void irq_keyboard(struct interrupt_frame *f);
__attribute__((interrupt)) void irq_slave(struct interrupt_frame *f);
__attribute__((interrupt)) void irq_spurious(struct interrupt_frame *f);

static void *exc_handlers[32] = {
    exc_0,  exc_1,  exc_2,  exc_3,  exc_4,  exc_5,  exc_6,  exc_7,
    exc_8,  exc_9,  exc_10, exc_11, exc_12, exc_13, exc_14, exc_15,
    exc_16, exc_17, exc_18, exc_19, exc_20, exc_21, exc_22, exc_23,
    exc_24, exc_25, exc_26, exc_27, exc_28, exc_29, exc_30, exc_31,
};

void idt_init(void) 
{
    pic_remap();

    for (int i = 0; i < 32; i++)
        set_idt_entry(i, exc_handlers[i], 0x8E);

    set_idt_entry(0x20, irq_timer,    0x8E);
    set_idt_entry(0x21, irq_keyboard, 0x8E);
    set_idt_entry(0x28, irq_slave,    0x8E);
    set_idt_entry(0x27, irq_spurious, 0x8E);
    set_idt_entry(0x2F, irq_spurious, 0x8E);

    struct idtr idtr = {
        .limit = sizeof(idt) - 1,
        .base  = (uint64_t)&idt,
    };
    __asm__ volatile("lidt %0" :: "m"(idtr) : "memory");

    serial_printf("LOG: IDT loaded (%d entries)\n", 256);
}
