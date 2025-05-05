#include <kernel/device.h>
#include <kernel/driver.h>
#include <device/ps2_ports.h>
#include <frog/interrupt.h>
#include <frog/types.h>

#include <kernel/debug.h>

extern struct bus_type isa_bus;
int ps2_kbd_probe(struct device *dev);

static struct driver ps2_kbd_driver = {.name = "ps2-kbd",
                                       .bus = &isa_bus,
                                       .probe = ps2_kbd_probe};

void ps2_kbd_ISR(void)
{
        INFO("Success ps2 keyboard ISR.");

        uint_16 scan_code = 0x0;
        while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
                scan_code = ps2_read_byte();  // get scan_code
        }
        INFO("kbd scan code %x", scan_code);
        ack(INT_VECTOR_KEYBOARD);
}

int ps2_kbd_probe(struct device *dev)
{
        dev->driver = &ps2_kbd_driver;
        // enable keyboard
        ps2_wait_writeable();
        outb(PS2_COMMAND, KBD_WRITE);
        ps2_wait_writeable();
        outb(PS2_DATA, KBDC_MODE);

        register_r0_intr_handler(INT_VECTOR_KEYBOARD,
                                 (Inthandle_t *) ps2_kbd_ISR);

        return 0;
}

uint_32 ps2_kbd_driver_init(void)
{
        register_driver(&ps2_kbd_driver);
        return 0;
}

