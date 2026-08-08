#include <asm/i8253.h>
#include <asm/io.h>
#include <const.h>
#include <kernel/qemu_test.h>
// -----------------------------------------------------------------
//                       Programmable Interval Timer
// -----------------------------------------------------------------
#define COUNTER0_PORT    0x40
#define COUNTER0_NO      0
#define COUTNER_MODE     2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PROT 0x43

static void counter_bytes(uint_16 counter_value, uint_8 *low, uint_8 *high)
{
    *low = (uint_8) counter_value;
    *high = (uint_8) (counter_value >> 8);
}

static void frequency_set(uint_8 counter_port,
                          uint_8 counter_no,
                          uint_8 rwL,
                          uint_8 counter_mode,
                          uint_16 counter_value)
{
    uint_8 low;
    uint_8 high;

    counter_bytes(counter_value, &low, &high);
    outb(PIT_CONTROL_PROT, (uint_8) (counter_no << 6 | rwL << 4 | counter_mode << 1));
    outb(counter_port, low);
    outb(counter_port, high);
}

void init_PIT8253(void)
{
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUTNER_MODE,
                  PIT_COUNTER0_DIVISOR);
}

#ifdef CONFIG_FROG_TEST_TIME
void i8253_regression_test(void)
{
    uint_8 low;
    uint_8 high;

    counter_bytes(PIT_COUNTER0_DIVISOR, &low, &high);
    frog_test_case("time.pit.programming",
                   PIT_COUNTER0_DIVISOR == 0x04a9U && low == 0xa9U &&
                       high == 0x04U);
}
#endif
