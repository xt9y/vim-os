#include "acpi.h"
#include "io.h"
#include "serial.h"

struct __attribute__((packed)) rsdp {
    char sig[8];
    uint8_t chk;
    char oem[6];
    uint8_t rev;
    uint32_t rsdt;
    uint32_t len;
    uint64_t xsdt;
    uint8_t xchk;
    uint8_t pad[3];
};

struct __attribute__((packed)) sdt {
    char sig[4];
    uint32_t len;
    uint8_t rev;
    uint8_t chk;
    char oem[6];
    char oem_tbl[8];
    uint32_t oem_rev;
    uint32_t creator;
    uint32_t creator_rev;
};

#define FADT_DSDT_OFF 40
#define FADT_PM1A_CNT_OFF 64

static uint64_t hhdm_off;
static struct rsdp *rsdp_ptr;
static struct {
    uint16_t pm1a;
    uint16_t slp_typa;
    uint16_t slp_typb;
    int valid;
} s5;

static void *phys(uint64_t p)
{
    return (void *)(hhdm_off + p);
}

static void find_s5(uint32_t dsdt_phys)
{
    struct sdt *hdr = (struct sdt *)phys(dsdt_phys);
    uint8_t *aml = (uint8_t *)hdr;
    uint32_t aml_len = hdr->len;

    for (uint32_t i = 0; i < aml_len - 12; i++) {
        if (aml[i] != 0x5F || aml[i+1] != 0x53 || aml[i+2] != 0x35 || aml[i+3] != 0x5F)
            continue;
        uint32_t p = i + 4;
        if (p >= aml_len || aml[p] != 0x12) continue;
        p++;
        if (p >= aml_len) break;

        int pbytes = 1 + ((aml[p] >> 6) & 3);
        p += pbytes;
        if (p >= aml_len) break;

        uint8_t nelem = aml[p++];
        if (nelem < 2) continue;

        for (int e = 0; e < 2 && e < nelem && p < aml_len; e++) {
            uint16_t val = 0;
            if (aml[p] == 0x0A) {
                if (p + 1 >= aml_len) break;
                val = aml[p + 1];
                p += 2;
            } else if (aml[p] == 0x0B) {
                if (p + 2 >= aml_len) break;
                val = aml[p + 1] | (aml[p + 2] << 8);
                p += 3;
            } else if (aml[p] == 0x0C) {
                if (p + 4 >= aml_len) break;
                val = aml[p + 1] | (aml[p + 2] << 8);
                p += 5;
            } else {
                break;
            }
            if (e == 0) s5.slp_typa = val;
            else s5.slp_typb = val;
        }
        s5.valid = 1;
        serial_printf("LOG: ACPI: S5 slp_typa=0x%x slp_typb=0x%x pm1a=0x%x\n",
                      s5.slp_typa, s5.slp_typb, s5.pm1a);
        return;
    }
    serial_printf("LOG: ACPI: _S5 not found in DSDT\n");
}

static void parse_tables(void)
{
    if (!rsdp_ptr) return;
    if (*(uint64_t *)rsdp_ptr->sig != *(uint64_t *)"RSD PTR ") {
        serial_printf("LOG: ACPI: bad RSDP signature\n"); return;
    }

    struct sdt *st;
    uint32_t count;
    int is_xsdt = 0;

    if (rsdp_ptr->rev >= 2 && rsdp_ptr->xsdt) {
        st = (struct sdt *)phys(rsdp_ptr->xsdt);
        if (*(uint32_t *)st->sig != *(uint32_t *)"XSDT") {
            serial_printf("LOG: ACPI: bad XSDT signature\n"); return;
        }
        count = (st->len - sizeof(struct sdt)) / 8;
        is_xsdt = 1;
    } else {
        st = (struct sdt *)phys(rsdp_ptr->rsdt);
        if (*(uint32_t *)st->sig != *(uint32_t *)"RSDT") {
            serial_printf("LOG: ACPI: bad RSDT signature\n"); return;
        }
        count = (st->len - sizeof(struct sdt)) / 4;
    }

    uint32_t fadt_phys = 0;

    if (is_xsdt) {
        uint64_t *ents = (uint64_t *)((uint8_t *)st + sizeof(struct sdt));
        for (uint32_t i = 0; i < count; i++) {
            struct sdt *t = (struct sdt *)phys(ents[i]);
            if (*(uint32_t *)t->sig == *(uint32_t *)"FACP") {
                fadt_phys = (uint32_t)ents[i];
                break;
            }
        }
    } else {
        uint32_t *ents = (uint32_t *)((uint8_t *)st + sizeof(struct sdt));
        for (uint32_t i = 0; i < count; i++) {
            struct sdt *t = (struct sdt *)phys(ents[i]);
            if (*(uint32_t *)t->sig == *(uint32_t *)"FACP") {
                fadt_phys = ents[i];
                break;
            }
        }
    }

    if (!fadt_phys) {
        serial_printf("LOG: ACPI: FADT not found\n"); return;
    }

    uint8_t *fadt = (uint8_t *)phys(fadt_phys);
    s5.pm1a = *(uint16_t *)(fadt + FADT_PM1A_CNT_OFF);
    uint32_t dsdt_phys = *(uint32_t *)(fadt + FADT_DSDT_OFF);

    serial_printf("LOG: ACPI: FADT found, pm1a=0x%x dsdt=0x%x\n", s5.pm1a, dsdt_phys);
    find_s5(dsdt_phys);
}

int acpi_init(uint64_t hhdm)
{
    hhdm_off = hhdm;
    return 0;
}

void acpi_set_rsdp(uint64_t rsdp_addr)
{
    rsdp_ptr = (struct rsdp *)rsdp_addr;
    parse_tables();
}

void acpi_poweroff(void)
{
    if (s5.valid) {
        uint16_t val = s5.slp_typa | (s5.slp_typb << 8) | (1 << 13);
        outw(s5.pm1a, val);
    } else {
        serial_printf("LOG: ACPI: no S5 info, trying legacy ports...\n");
    }

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    hang;
}
