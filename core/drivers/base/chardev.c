#include <frog/bitmap.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/chardev.h>
#include <kernel/vfs_ops.h>
#include <frog/math.h>
#include <kernel/panic.h>
#include <kernel/debug.h>

static const struct file_operations *chrdev_table[MAX_CHARDEV];
static struct bitmap *chrdev_bitmap;

static uint_32 get_avaliable_major()
{
        return find_block_bitmap(chrdev_bitmap, 1);
}

static bool is_avalible_major(uint_32 major)
{
        uint_32 value = get_value_bitmap(chrdev_bitmap, major);
        return !!value;
}

int register_chrdev(uint_32 major, const struct file_operations *fops)
{
        if (major == 0) {
                uint_32 new_major = get_avaliable_major();
                if (new_major == -1) {
                        WARN("[chardev]: not enough chardev number.");
                        return -1;
                } else {
                        chrdev_table[new_major] = fops;
                }
        } else {
                if (is_avalible_major(major)) {
                        chrdev_table[major] = fops;
                }
        }
        return 0;
}

int unregister_chrdev(uint_32 major)
{
        if (major > MAX_CHARDEV) {
                return -1;
        }
        const struct file_operations *target = chrdev_table[major];
        if (target) {
                chrdev_table[major] = NULL;
        }
        return 0;
}

int chrdev_init(void)
{
        chrdev_bitmap = kmalloc(sizeof(struct bitmap));
        chrdev_bitmap->bits = kmalloc(CEIL(MAX_CHARDEV, 8));
        chrdev_bitmap->map_bytes_length = MAX_CHARDEV;
        init_bitmap(chrdev_bitmap);

        return 0;
}
