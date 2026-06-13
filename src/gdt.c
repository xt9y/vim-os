#include "gdt.h"
#include "serial.h"      // so we can log
#include <stddef.h>
#include <stdint.h>

// --- Helper to pack a standard 8-byte GDT entry ---
static uint64_t gdt_make_entry(uint64_t base, uint32_t limit,
                               uint8_t access, uint8_t flags)
{
    uint64_t entry = 0;
    entry  = (limit & 0xFFFF);                    // bytes 0-1
    entry |= (base & 0xFFFF) << 16;               // bytes 2-3
    entry |= ((base >> 16) & 0xFF) << 32;         // byte 4
    entry |= ((uint64_t)access) << 40;
    entry |= ((uint64_t)((limit >> 16) & 0xF)) << 48;
    entry |= ((uint64_t)(flags & 0x0F)) << 52;
    entry |= ((base >> 24) & 0xFF) << 56;         // byte 7
    return entry;
}

// --- 104-byte 64-bit TSS ---
struct tss_struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint32_t reserved1;
    uint32_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb_offset;
} __attribute__((packed));

static struct tss_struct tss;

// [0] null, [1] kernel code, [2] kernel data, [3] TSS low, [4] TSS high
static struct {
    uint64_t entries[5];
} __attribute__((aligned(16))) gdt;

void gdt_init(void) 
{
    for (unsigned i = 0; i < sizeof(tss); i++) ((volatile uint8_t *)&tss)[i] = 0;

    // 2. Fill GDT entries
    // Null descriptor
    gdt.entries[0] = 0;

    // Kernel code 64-bit: base=0, limit=0, access=0x9A, flags=0x20 (L=1)
    gdt.entries[1] = gdt_make_entry(0, 0, 0x9A, 0x2);

    // Kernel data: base=0, limit=0, access=0x92, flags=0
    gdt.entries[2] = gdt_make_entry(0, 0, 0x92, 0x00);

    // TSS descriptor (16 bytes) – base = address of tss, limit = size-1
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    gdt.entries[3] = gdt_make_entry(tss_base, tss_limit, 0x89, 0x00);
    gdt.entries[4] = (tss_base >> 32) & 0xFFFFFFFF;

    // 3. Load GDTR
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr = {
        .limit = sizeof(gdt) - 1,
        .base  = (uint64_t)&gdt,
    };
    __asm__ volatile("lgdt %0" :: "m"(gdtr) : "memory");

    // 4. Reload code segment with a far return
    __asm__ volatile(
        "pushq %0\n"
        "pushq $1f\n"
        "retfq\n"
        "1:"
        :: "i"((uint64_t)GDT_KERNEL_CODE) : "memory");

    // 5. Reload data segment registers
    __asm__ volatile(
        "movw %0, %%ds\n"
        "movw %0, %%es\n"
        "movw %0, %%fs\n"
        "movw %0, %%gs\n"
        "movw %0, %%ss"
        :: "r"((uint16_t)GDT_KERNEL_DATA) : "memory");

    // 6. Load task register
    __asm__ volatile("ltr %0" :: "r"((uint16_t)GDT_TSS) : "memory");

    serial_printf("LOG: GDT: code=%x data=%x tss=%x base=%p\n",
                  GDT_KERNEL_CODE, GDT_KERNEL_DATA, GDT_TSS, (void *)tss_base);
}
