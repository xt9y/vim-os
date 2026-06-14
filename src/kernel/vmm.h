#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_PRESENT  (1 << 0)
#define VMM_WRITABLE (1 << 1)
#define VMM_USER     (1 << 2)
#define VMM_NX       (1ULL << 63)

void vmm_init(uint64_t hhdm_offset);
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);

#endif
