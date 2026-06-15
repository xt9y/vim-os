#include "ata.h"
#include "io.h"

#define ATA_DATA     0x1F0
#define ATA_SEC_CNT  0x1F2
#define ATA_LBA_LO   0x1F3
#define ATA_LBA_MID  0x1F4
#define ATA_LBA_HI   0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_CMD      0x1F7
#define ATA_STATUS   0x1F7

#define CMD_READ     0x20
#define CMD_WRITE    0x30

#define BSY  0x80
#define DRQ  0x08
#define ERR  0x01

int ata_probe(void)
{
    uint8_t st = inb(ATA_STATUS);
    if (st == 0xFF) return -1;
    return 0;
}

static int wait_bsy(void)
{
    for (int i = 0; i < 10000000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (!(st & BSY)) {
            if (st & ERR) return -1;
            return 0;
        }
    }
    return -1;
}

static int wait_drq(void)
{
    for (int i = 0; i < 10000000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (!(st & BSY)) {
            if (st & ERR) return -1;
            if (st & DRQ) return 0;
        }
    }
    return -1;
}

int ata_read(uint32_t lba, void *buf, int count)
{
    if (ata_probe() < 0) return -1;
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SEC_CNT, count);
    outb(ATA_LBA_LO,  lba);
    outb(ATA_LBA_MID, lba >> 8);
    outb(ATA_LBA_HI,  lba >> 16);
    outb(ATA_CMD, CMD_READ);

    for (int s = 0; s < count; s++) {
        if (wait_drq() < 0) return -1;
        insw(ATA_DATA, (uint8_t *)buf + s * 512, 256);
    }
    if (wait_bsy() < 0) return -1;
    return 0;
}

int ata_write(uint32_t lba, const void *buf, int count)
{
    if (ata_probe() < 0) return -1;
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SEC_CNT, count);
    outb(ATA_LBA_LO,  lba);
    outb(ATA_LBA_MID, lba >> 8);
    outb(ATA_LBA_HI,  lba >> 16);
    outb(ATA_CMD, CMD_WRITE);

    for (int s = 0; s < count; s++) {
        if (wait_drq() < 0) return -1;
        outsw(ATA_DATA, (const uint8_t *)buf + s * 512, 256);
    }
    if (wait_bsy() < 0) return -1;
    return 0;
}
