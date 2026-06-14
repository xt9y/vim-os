#include "pit.h"
#include "idt.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

#define PIT_CH0     0x40
#define PIT_CMD     0x43
#define PIT_FREQ    1193182
#define TARGET_HZ   100
#define DIVIDER     (PIT_FREQ / TARGET_HZ)

static volatile uint64_t pit_ticks = 0;

void pit_init(void) 
{
    outb(PIT_CMD, 0x34);
    outb(PIT_CH0, DIVIDER & 0xFF);
    outb(PIT_CH0, (DIVIDER >> 8) & 0xFF);
    serial_printf("LOG: PIT init: %u Hz (divider=%u)\n", TARGET_HZ, DIVIDER);
}

uint64_t pit_get_ticks(void) 
{
    return pit_ticks;
}

void timer_sleep(uint32_t ms) 
{
    uint64_t target = pit_ticks + (ms * TARGET_HZ / 1000);
    while (pit_ticks < target)
        __asm__("pause");
}

__attribute__((interrupt))
void irq_timer(struct interrupt_frame *f) 
{
    (void)f;
    pit_ticks++;
    pic_eoi(0);
}

__attribute__((interrupt))
void irq_slave(struct interrupt_frame *f) 
{
    (void)f;
    pic_eoi(8);
}

__attribute__((interrupt))
void irq_spurious(struct interrupt_frame *f) 
{
    (void)f;
    pic_eoi(7);
}
