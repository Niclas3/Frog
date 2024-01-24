#include <device/cmos.h>
#include <io.h>
#include <const.h>
#include <sys/int.h>

uint_32 tsc_mhz = 3000;
uint_32 tsc_basis_time = 0;
uint_32 arch_boot_time = 0;


#define RTC_HOUR_MODE_12H 0x02

#define RTC_SEC_REG 0x00
#define RTC_MIN_REG 0x02
#define RTC_HOUR_REG 0x04
#define RTC_DAY_REG 0x07
#define RTC_MONTH_REG 0x08
#define RTC_YEAR_REG 0x09
#define RTC_B_REG 0x0B

enum { cmos_address = 0x70, cmos_data = 0x71 };

/**
 * @brief Poorly convert years to Unix timestamps.
 *
 * @param years Years since 2000
 * @returns Seconds since the Unix epoch, maybe...
 */
static uint_32 secs_of_years(int years)
{
    uint_32 days = 0;
    years += 2000;
    while (years > 1969) {
        days += 365;
        if (years % 4 == 0) {
            if (years % 100 == 0) {
                if (years % 400 == 0) {
                    days++;
                }
            } else {
                days++;
            }
        }
        years--;
    }
    return days * 86400;
}

/**
 * @brief How long was a month in a given year?
 *
 * Tries to do leap year stuff for February.
 *
 * @param months 1~12 calendar month
 * @param year   Years since 2000
 * @return Number of seconds in that month.
 */
static uint_32 secs_of_month(int months, int year)
{
    year += 2000;

    uint_32 days = 0;
    switch (months) {
    case 11:
        days += 30; /* fallthrough */
    case 10:
        days += 31; /* fallthrough */
    case 9:
        days += 30; /* fallthrough */
    case 8:
        days += 31; /* fallthrough */
    case 7:
        days += 31; /* fallthrough */
    case 6:
        days += 30; /* fallthrough */
    case 5:
        days += 31; /* fallthrough */
    case 4:
        days += 30; /* fallthrough */
    case 3:
        days += 31; /* fallthrough */
    case 2:
        days += 28;
        if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0))) {
            days++;
        } /* fallthrough */
    case 1:
        days += 31; /* fallthrough */
    default:
        break;
    }
    return days * 86400;
}

static int get_update_in_progress_flag()
{
    outb(cmos_address, 0x0A);
    return (inb(cmos_data) & 0x80);
}

static unsigned char get_RTC_register(int reg)
{
    outb(cmos_address, reg);
    return inb(cmos_data);
}

/**
 * CMOS time to a Unix timestamp.
 *
 * Reads BCD data from the RTC CMOS and does some dumb
 * math to convert the display time to a Unix timestamp.
 *
 * CMOS register define at http://www.bioscentral.com/misc/cmosmap.htm
 *
 * @return Current Unix time
 */
uint_32 read_rtc(void)
{
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char registerB;

    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned int year;

    // Note: This uses the "read registers until you get the same values twice
    // in a row" technique
    //       to avoid getting dodgy/inconsistent values due to RTC updates

    while (get_update_in_progress_flag())
        ;  // Make sure an update isn't in progress

    second = get_RTC_register(RTC_SEC_REG);
    minute = get_RTC_register(RTC_MIN_REG);
    hour = get_RTC_register(RTC_HOUR_REG);
    day = get_RTC_register(RTC_DAY_REG);
    month = get_RTC_register(RTC_MONTH_REG);
    year = get_RTC_register(RTC_YEAR_REG);

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;

        while (get_update_in_progress_flag())
            ;  // Make sure an update isn't in progress

        second = get_RTC_register(RTC_SEC_REG);
        minute = get_RTC_register(RTC_MIN_REG);
        hour = get_RTC_register(RTC_HOUR_REG);
        day = get_RTC_register(RTC_DAY_REG);
        month = get_RTC_register(RTC_MONTH_REG);
        year = get_RTC_register(RTC_YEAR_REG);
    } while ((last_second != second) || (last_minute != minute) ||
             (last_hour != hour) || (last_day != day) ||
             (last_month != month) || (last_year != year));

    registerB = get_RTC_register(RTC_B_REG);

    // Convert BCD to binary values if necessary
    // test is not Enable square wave
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    // Bit 2 = 24 hour clock (0 = 24 hour mode (default), 1 = 12 hour mode)
    if (!(registerB & RTC_HOUR_MODE_12H) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }
    uint_32 time = secs_of_years(year - 1) +
                   secs_of_month(month - 1, year) +
                   (day - 1) * 86400 +
                   hour * 3600 +
                   minute * 60 +
                   second + 0;
    return time;
}

/**
 * @brief Helper to read timestamp counter
 *
 * tsc_value[0] = lo
 * tsc_value[1] = hi
 */
static inline void read_tsc(uint_32 *tsc_value)
{
    uint_32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    tsc_value[0] = lo;
    tsc_value[1] = hi;
}

#define SUBSECONDS_PER_SECOND 1000000

/**
 * @brief Subdivide ticks into seconds in subticks.
 */
static void update_ticks(uint_32 ticks,
                         uint_32 *timer_ticks,
                         uint_32 *timer_subticks)
{
    *timer_subticks = ticks;
    *timer_ticks = *timer_subticks / HZ;
    *timer_subticks = (*timer_subticks % HZ) * (SUBSECONDS_PER_SECOND / HZ);
}

/**
 * wall clock time.
 *
 * Note that while the kernel version of this takes a *z option that is
 * supposed to have timezone information, we don't actually use  it,
 * and I'm pretty sure it's NULL everywhere?
 */
extern volatile uint_32 ticks;
int sys_gettimeofday(struct timeval *t, void *z)
{
    /* uint_32 tsc[2] = {0}; */
    /* read_tsc(tsc); */
    uint_32 timer_ticks, timer_subticks;
    enum intr_status old_status = intr_disable();
    update_ticks(ticks, &timer_ticks, &timer_subticks);
    intr_set_status(old_status);
    // TODO:
    //  tsc is 64bits value
    /* update_ticks(tsc[0] / tsc_mhz, &timer_ticks, &timer_subticks); */
    t->tv_sec = arch_boot_time + timer_ticks;
    t->tv_usec = timer_subticks;
    return 0;
}

/**
 * Set the system clock time
 *
 * TODO:
 * not implement
 *
 * TODO: A lot of this time stuff needs to be made more generic,
 *       it's shared pretty directly with aarch64...
 */
int sys_settimeofday(struct timeval *t, void *z)
{
    return 0;
}

/**
 * Initializes boot time, system time aka rtc
 */
void clock_init(void)
{
    arch_boot_time = read_rtc();
    /* uint_32 end_lo; */
    /* uint_32 end_hi; */
    /* uint_32 start_lo, start_hi; */
    /* __asm__ volatile( */
    /*     #<{(| Disables and sets gating for channel 2 |)}># */
    /*     "inb   $0x61, %%al\n" */
    /*     "andb  $0xDD, %%al\n" */
    /*     "orb   $0x01, %%al\n" */
    /*     "outb  %%al, $0x61\n" */
    /*     #<{(| Configure channel 2 to one-shot, next two bytes are low/high |)}># */
    /*     "movb  $0xB2, %%al\n" #<{(| 0b10110010 |)}># */
    /*     "outb  %%al, $0x43\n" */
    /*     #<{(| 0x__9b |)}># */
    /*     "movb  $0x9B, %%al\n" */
    /*     "outb  %%al, $0x42\n" */
    /*     "inb   $0x60, %%al\n" */
    /*     #<{(|  0x2e__ |)}># */
    /*     "movb  $0x2E, %%al\n" */
    /*     "outb  %%al, $0x42\n" */
    /*     #<{(| Re-enable |)}># */
    /*     "inb   $0x61, %%al\n" */
    /*     "andb  $0xDE, %%al\n" */
    /*     "outb  %%al, $0x61\n" */
    /*     #<{(| Pulse high |)}># */
    /*     "orb   $0x01, %%al\n" */
    /*     "outb  %%al, $0x61\n" */
    /*     #<{(| Read TSC and store in vars |)}># */
    /*     "rdtsc\n" */
    /*     "movl  %%eax, %2\n" */
    /*     "movl  %%edx, %3\n" */
    /*     #<{(| In QEMU and VirtualBox, this seems to flip low. */
    /*      * On real hardware and VMware it flips high. |)}># */
    /*     "inb   $0x61, %%al\n" */
    /*     "andb  $0x20, %%al\n" */
    /*     "jz   2f\n" */
    /*     #<{(| Loop until output goes low? |)}># */
    /*     "1:\n" */
    /*     "inb   $0x61, %%al\n" */
    /*     "andb  $0x20, %%al\n" */
    /*     "jnz   1b\n" */
    /*     "rdtsc\n" */
    /*     "jmp   3f\n" */
    /*     #<{(| Loop until output goes high |)}># */
    /*     "2:\n" */
    /*     "inb   $0x61, %%al\n" */
    /*     "andb  $0x20, %%al\n" */
    /*     "jz   2b\n" */
    /*     "rdtsc\n" */
    /*     "3:\n" */
    /*     : "=a"(end_lo), "=d"(end_hi), "=r"(start_lo), "=r"(start_hi)); */
    /*  */
    /* // TODO: */
    /* // end and start are 64bits value */
    /* uint_32 end = end_lo; */
    /* uint_32 start = start_lo; */
    /* tsc_mhz = (end - start) / 10000; */
    /* if (tsc_mhz == 0) */
    /*     tsc_mhz = 2000; #<{(| uh oh |)}># */
    /* tsc_basis_time = start / tsc_mhz; */
}
