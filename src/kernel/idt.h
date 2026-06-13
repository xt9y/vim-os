#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void idt_init(void);
void pic_eoi(int irq);

#endif
