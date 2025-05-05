#include <frog/types.h>
#include <kernel/device.h>
#include <kernel/driver.h>

#include <device/ps2_ports.h>
#include <frog/interrupt.h>

#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

extern struct bus_type isa_bus;
int ps2_mouse_probe(struct device *dev);

static struct driver ps2_mouse_driver = {
    .name = "ps2-mouse",
    .probe = ps2_mouse_probe,
    .bus = &isa_bus,
};

// 0x2C
void ps2_mouse_ISR(void)
{
        INFO("Success ps2 mouse ISR.");
        uint_16 scan_code = 0x0;
        while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
                scan_code = ps2_read_byte();  // get scan_code
        }
        INFO("ps2 Mouse scan code %x", scan_code);

        /* char scancode = inb(PS2_DATA); */
        ack(INT_VECTOR_PS2_MOUSE);

        // send scancode to make mouse package
        /* handle_ps2_mouse_scancode(scancode); */
        // send scancode to this file queue
        /* handle_mouse_event(scancode); */

        irq_enter();
        irq_exit();
}


int ps2_mouse_probe(struct device *dev)
{
        // When dev and drv match then run this function
        dev->driver = &ps2_mouse_driver;

        register_r0_intr_handler(INT_VECTOR_PS2_MOUSE,
                                 (Inthandle_t *) ps2_mouse_ISR);

        ps2_wait_writeable();
        outb(PS2_COMMAND, PS2_READ_CONFIG);
        uint_8 ctrl = inb(PS2_DATA);
        ctrl |= 0x02;  // open IRQ12(bit1)
        ps2_wait_readable();

        outb(PS2_COMMAND, PS2_WRITE_CONFIG);
        outb(PS2_DATA, ctrl);

        ps2_wait_writeable();
        outb(PS2_COMMAND, MOUSE_WRITE);
        ps2_wait_writeable();
        outb(PS2_DATA, MOUSE_ENABLE);
        ps2_wait_readable();

        return 0;
}

uint_32 ps2_mouse_driver_init(void)
{
        register_driver(&ps2_mouse_driver);
        return 0;
}

/* module_init(ps2_mouse_driver_init); */
