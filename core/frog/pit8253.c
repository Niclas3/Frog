#include <frog/piti8253.h>
#include <io.h>

// -----------------------------------------------------------------
//                       Programmable Interval Timer
// -----------------------------------------------------------------
// #define IRQ0_FREQUENCY   100
// #define IRQ0_FREQUENCY   1000
#define IRQ0_FREQUENCY   9000
// #define IRQ0_FREQUENCY   12000
#define INPUT_FREQUENCY  1193180
#define COUNTER0_VALUE   INPUT_FREQUENCY/IRQ0_FREQUENCY
#define COUNTER0_PORT    0x40
#define COUNTER0_NO      0
#define COUTNER_MODE     2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PROT 0x43

static void frequency_set(uint_8 counter_port,
                          uint_8 counter_no,
                          uint_8 rwL,
                          uint_8 counter_mode,
                          uint_16 counter_value)
{
    outb(PIT_CONTROL_PROT, (uint_8) (counter_no << 6 | rwL << 4 | counter_mode << 1));
    outb(counter_port, (uint_8) counter_value);
    outb(counter_port, (uint_8) counter_value >> 8);
}

void init_PIT8253(void)
{
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUTNER_MODE,
                  COUNTER0_VALUE);
}
