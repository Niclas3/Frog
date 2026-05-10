#include <frog/bitmap.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/assert.h>
#include <kernel/chardev.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs_ops.h>

static const struct file_operations *chrdev_table[MAX_CHARDEV];
static struct bitmap *chrdev_bitmap;

static uint_32 get_avaliable_major()
{
        int idx = find_block_bitmap(chrdev_bitmap, 1);
        set_value_bitmap(chrdev_bitmap, idx, 1);
        return idx;
}

static uint_32 free_major(int idx){
        set_value_bitmap(chrdev_bitmap, idx, 0);
        return 0;
}

static bool is_avalible_major(uint_32 major)
{
        uint_32 value = get_value_bitmap(chrdev_bitmap, major);
        return !!value;
}

const struct file_operations *get_chardev_fop(int major)
{
        ASSERT(major < MAX_CHARDEV && major >= 0);
        return chrdev_table[major];
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
                        return new_major;
                }
        } else {
                if (is_avalible_major(major)) {
                        chrdev_table[major] = fops;
                        return major;
                } else {
                        WARN("[chardev]: %d is exist", major);
                        return -1;
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
                free_major(major);
        }
        return 0;
}

int chrdev_init(void)
{
        chrdev_bitmap = kmalloc(sizeof(struct bitmap));
        chrdev_bitmap->bits = kmalloc(CEIL(MAX_CHARDEV, 8));
        chrdev_bitmap->map_bytes_length = CEIL(MAX_CHARDEV, 8);
        init_bitmap(chrdev_bitmap);

        return 0;
}
