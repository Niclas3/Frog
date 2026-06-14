#include <frog/types.h>
#include "./print.h"

#define VGA_VIRT_BASE 0xC00B8000UL
#define COLS 80
#define ROWS 25
#define ATTR 0x07

#define VGA_REG_CMD 0x3D4
#define VGA_REG_DATA 0x3D5
#define VGA_CURSOR_HIGH 0x0E
#define VGA_CURSOR_LOW 0x0F

static inline void outb(uint_16 port, uint_8 val)
{
        __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint_8 inb(uint_16 port)
{
        uint_8 val;
        __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
        return val;
}

static uint_16 cursor_get(void)
{
        outb(VGA_REG_CMD, VGA_CURSOR_HIGH);
        uint_8 high = inb(VGA_REG_DATA);
        outb(VGA_REG_CMD, VGA_CURSOR_LOW);
        uint_8 low = inb(VGA_REG_DATA);
        return ((uint_16) high << 8) | low;
}

static void cursor_set(uint_16 pos)
{
        outb(VGA_REG_CMD, VGA_CURSOR_HIGH);
        outb(VGA_REG_DATA, (uint_8) ((pos >> 8) & 0xFF));
        outb(VGA_REG_CMD, VGA_CURSOR_LOW);
        outb(VGA_REG_DATA, (uint_8) (pos & 0xFF));
}

void put_char(uint_8 c)
{
        volatile uint_16 *vga = (volatile uint_16 *) VGA_VIRT_BASE;
        uint_16 pos = cursor_get();

        switch (c) {
        case '\n':
        case '\r':
                pos = (uint_16) ((pos / COLS + 1) * COLS);
                break;
        case '\b':
                if (pos > 0) {
                        pos--;
                        vga[pos] = ((uint_16) ATTR << 8) | ' ';
                }
                break;
        default:
                vga[pos] = ((uint_16) ATTR << 8) | c;
                pos++;
                break;
        }

        if (pos >= ROWS * COLS) {
                for (uint_32 i = 0; i < ROWS * COLS; i++)
                        vga[i] = ((uint_16) ATTR << 8) | ' ';
                pos = 0;
        }

        cursor_set(pos);
}

void put_str(char *s)
{
        while (*s) {
                put_char((uint_8) *s);
                s++;
        }
}

void cls_screen(void)
{
        volatile uint_16 *vga = (volatile uint_16 *) VGA_VIRT_BASE;
        for (uint_32 i = 0; i < ROWS * COLS; i++)
                vga[i] = ((uint_16) ATTR << 8) | ' ';
        cursor_set(0);
}

/*
 * VGA self-test: bypass put_char entirely. Write each of the 2000 cells
 * directly to VGA memory with a unique pattern, then read each back and
 * verify. On any mismatch, leave a visible marker at the top-left corner
 * encoding the failing position; on success leave "VGA OK <count>" in
 * green at the top-left.
 *
 * This isolates whether the problem is:
 *   (a) raw VGA memory at 0xC00B8000 (mapping / page table) — if writes
 *       don't read back correctly anywhere, the linear address itself
 *       isn't right
 *   (b) cursor I/O port reads (0x3D4/0x3D5) — caller of put_char relies
 *       on these; this test doesn't, so if (a) passes but put_char still
 *       fails, the bug is in cursor handling
 *   (c) something in put_char's control flow
 */
void vga_self_test(void)
{
        volatile uint_16 *vga = (volatile uint_16 *) VGA_VIRT_BASE;
        uint_32 total = ROWS * COLS;
        uint_32 ok_count = 0;
        int_32 first_fail = -1;
        uint_8 wrote = 0, got = 0;

        /* Clear first so leftover boot text doesn't confuse the test */
        for (uint_32 i = 0; i < total; i++)
                vga[i] = ((uint_16) 0x07 << 8) | ' ';

        for (uint_32 pos = 0; pos < total; pos++) {
                uint_8 c = (uint_8) ('!' + (pos % 90));
                vga[pos] = ((uint_16) 0x07 << 8) | c;
                uint_16 rb = vga[pos];
                uint_8 rb_c = (uint_8) (rb & 0xFF);
                if (rb_c == c) {
                        ok_count++;
                } else if (first_fail < 0) {
                        first_fail = (int_32) pos;
                        wrote = c;
                        got = rb_c;
                }
        }

        /* Clear again so the report is visible at the top */
        for (uint_32 i = 0; i < total; i++)
                vga[i] = ((uint_16) 0x07 << 8) | ' ';

        if (first_fail < 0) {
                /* Success: green "VGA OK NNNN" at row 0 */
                const char *msg = "VGA OK 2000/2000";
                uint_32 i = 0;
                while (msg[i]) {
                        vga[i] = ((uint_16) 0x0A << 8) | (uint_8) msg[i];
                        i++;
                }
        } else {
                /* Failure: red "VGA FAIL @<pos> w=X r=Y" */
                const char *msg = "VGA FAIL pos=";
                uint_32 i = 0;
                while (msg[i]) {
                        vga[i] = ((uint_16) 0x0C << 8) | (uint_8) msg[i];
                        i++;
                }
                /* Encode first_fail as 4 hex digits */
                for (int d = 3; d >= 0; d--) {
                        uint_8 nibble = (uint_8) ((first_fail >> (d * 4)) & 0xF);
                        uint_8 ch = nibble < 10
                                        ? (uint_8) ('0' + nibble)
                                        : (uint_8) ('A' + (nibble - 10));
                        vga[i++] = ((uint_16) 0x0C << 8) | ch;
                }
                vga[i++] = ((uint_16) 0x0C << 8) | ' ';
                vga[i++] = ((uint_16) 0x0C << 8) | 'w';
                vga[i++] = ((uint_16) 0x0C << 8) | '=';
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + (wrote / 100) % 10);
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + (wrote / 10) % 10);
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + wrote % 10);
                vga[i++] = ((uint_16) 0x0C << 8) | ' ';
                vga[i++] = ((uint_16) 0x0C << 8) | 'r';
                vga[i++] = ((uint_16) 0x0C << 8) | '=';
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + (got / 100) % 10);
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + (got / 10) % 10);
                vga[i++] = ((uint_16) 0x0C << 8) | (uint_8) ('0' + got % 10);
        }

        /* Park cursor off the report row */
        cursor_set(COLS * 2);
        (void) ok_count;
}
