#include "rtc.h"
#include "io.h"
#include "config.h"

#define RTC_ADDR 0x70
#define RTC_DATA 0x71

#define REG_SEC   0x00
#define REG_MIN   0x02
#define REG_HOUR  0x04
#define REG_DAY   0x07
#define REG_MON   0x08
#define REG_YEAR  0x09
#define REG_STAT_A 0x0A
#define REG_STAT_B 0x0B

#define UIP 0x80

static int tz_offset;

static uint8_t read_reg(uint8_t reg)
{
    outb(RTC_ADDR, reg);
    return inb(RTC_DATA);
}

static int is_binary(void)
{
    return read_reg(REG_STAT_B) & 4;
}

static uint8_t from_bcd(uint8_t v)
{
    return ((v >> 4) * 10) + (v & 0xF);
}

void rtc_init(void)
{
    tz_offset = config_get_int("timezone");
}

void rtc_read_time(char *buf, size_t n)
{
    if (!buf || n < 17) return;

    uint8_t sec, min, hour, day, mon, year;

    for (;;) {
        while (read_reg(REG_STAT_A) & UIP);

        sec  = read_reg(REG_SEC);
        min  = read_reg(REG_MIN);
        hour = read_reg(REG_HOUR);
        day  = read_reg(REG_DAY);
        mon  = read_reg(REG_MON);
        year = read_reg(REG_YEAR);

        if (!(read_reg(REG_STAT_A) & UIP)) break;
    }

    if (!is_binary()) {
        sec  = from_bcd(sec);
        min  = from_bcd(min);
        hour = from_bcd(hour);
        day  = from_bcd(day);
        mon  = from_bcd(mon);
        year = from_bcd(year);
    }

    hour += tz_offset;
    if (hour >= 24) hour -= 24;

    unsigned y = 2000 + year;

    buf[0]  = '0' + (y / 1000);
    buf[1]  = '0' + ((y / 100) % 10);
    buf[2]  = '0' + ((y / 10) % 10);
    buf[3]  = '0' + (y % 10);
    buf[4]  = '-';
    buf[5]  = '0' + (mon / 10);
    buf[6]  = '0' + (mon % 10);
    buf[7]  = '-';
    buf[8]  = '0' + (day / 10);
    buf[9]  = '0' + (day % 10);
    buf[10] = ' ';
    buf[11] = '0' + (hour / 10);
    buf[12] = '0' + (hour % 10);
    buf[13] = ':';
    buf[14] = '0' + (min / 10);
    buf[15] = '0' + (min % 10);
    buf[16] = '\0';
}
