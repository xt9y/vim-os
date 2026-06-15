#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

int acpi_init(uint64_t hhdm);
void acpi_set_rsdp(uint64_t rsdp);
void acpi_poweroff(void);

#endif
