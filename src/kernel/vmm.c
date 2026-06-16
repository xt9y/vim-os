#include "vmm.h"
#include "pmm.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define PT_ENTRIES 512
#define PAGE_SIZE  4096

#define PTE_PHYS_MASK 0x000FFFFFFFFFFFFFULL
#define PTE_ADDR(e)   ((e) & PTE_PHYS_MASK & ~0xFFFULL)

static uint64_t hhdm = 0;
static uint64_t *pml4 = NULL;

static inline unsigned idx_pml4(uint64_t v) { return (v >> 39) & 0x1FF; }
static inline unsigned idx_pdp(uint64_t v)  { return (v >> 30) & 0x1FF; }
static inline unsigned idx_pd(uint64_t v)   { return (v >> 21) & 0x1FF; }
static inline unsigned idx_pt(uint64_t v)   { return (v >> 12) & 0x1FF; }

static inline int present(uint64_t e) { return e & VMM_PRESENT; }
static inline int huge(uint64_t e)    { return e & (1 << 7); }

static uint64_t *table_at(uint64_t phys)
{
    return (uint64_t *)(phys + hhdm);
}

static uint64_t ensure_table(uint64_t *parent, unsigned index, uint64_t flags)
{
    uint64_t e = parent[index];
    if (present(e) && !huge(e))
        return PTE_ADDR(e);

    uint64_t phys = pmm_alloc();
    if (!phys) {
        serial_printf("ERROR: VMM: pmm_alloc failed\n");
        hang;
    }
    uint64_t *virt = table_at(phys);
    for (int i = 0; i < PT_ENTRIES; i++)
        virt[i] = 0;

    parent[index] = phys | (flags & (VMM_PRESENT | VMM_WRITABLE | VMM_USER));
    return phys;
}

void vmm_init(uint64_t hhdm_offset)
{
    hhdm = hhdm_offset;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    pml4 = table_at(cr3);

    serial_printf("LOG: VMM: CR3=0x%x, PML4 at %p\n", (unsigned)cr3, pml4);
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t l4 = ensure_table(pml4, idx_pml4(virt), flags);
    uint64_t l3 = ensure_table(table_at(l4), idx_pdp(virt), flags);
    uint64_t l2 = ensure_table(table_at(l3), idx_pd(virt), flags);

    uint64_t *pt = table_at(l2);
    unsigned i = idx_pt(virt);

    pt[i] = (phys & ~0xFFFULL) | flags | VMM_PRESENT;

    __asm__ volatile("invlpg %0" :: "m"(*(volatile char *)virt) : "memory");
}

void vmm_unmap_page(uint64_t virt)
{
    uint64_t l4e = pml4[idx_pml4(virt)];
    if (!present(l4e) || huge(l4e)) return;
    uint64_t l3e = table_at(PTE_ADDR(l4e))[idx_pdp(virt)];
    if (!present(l3e) || huge(l3e)) return;
    uint64_t l2e = table_at(PTE_ADDR(l3e))[idx_pd(virt)];
    if (!present(l2e) || huge(l2e)) return;

    uint64_t *pt = table_at(PTE_ADDR(l2e));
    pt[idx_pt(virt)] = 0;

    __asm__ volatile("invlpg %0" :: "m"(*(volatile char *)virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virt)
{
    uint64_t l4e = pml4[idx_pml4(virt)];
    if (!present(l4e)) return 0;
    if (huge(l4e)) return 0;

    uint64_t l3e = table_at(PTE_ADDR(l4e))[idx_pdp(virt)];
    if (!present(l3e)) return 0;
    if (huge(l3e)) return PTE_ADDR(l3e) | (virt & 0x3FFFFFFF);

    uint64_t l2e = table_at(PTE_ADDR(l3e))[idx_pd(virt)];
    if (!present(l2e)) return 0;
    if (huge(l2e)) return PTE_ADDR(l2e) | (virt & 0x1FFFFF);

    uint64_t pte = table_at(PTE_ADDR(l2e))[idx_pt(virt)];
    if (!present(pte)) return 0;

    return PTE_ADDR(pte) | (virt & 0xFFF);
}
