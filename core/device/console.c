#include <device/console.h>
#include <device/devno-base.h>
#include <device/ide.h>
#include <errno-base.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <debug.h>
#include <gua/2d_graphics.h>
#include <print.h>
#include <stdio.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/semaphore.h>

struct lock gl_console_lock;

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

static uint_32 font_color = FSK_RED;
void console_put_char(uint_8 c)
{
    lock_fetch(&gl_console_lock);
    put_char(c);
    lock_release(&gl_console_lock);
}

void console_put_hex(int_32 num)
{
    lock_fetch(&gl_console_lock);
    put_int(num);
    lock_release(&gl_console_lock);
}

void console_put_str(char *str)
{
    lock_fetch(&gl_console_lock);
    put_str(str);
    lock_release(&gl_console_lock);
}

struct enter_stack {
    uint_32 prev_line_y;
    uint_32 prev_line_x;
};

static struct enter_stack *line_stack;
static uint_32 stack_pointer;
static gfx_context_t *console_gfx;
void console_init(gfx_context_t *ctx)
{
    lock_init(&gl_console_lock);
    console_gfx = ctx;
    sys_mount_device("/dev/tty0", DNOCONSOLE, NULL);
    line_stack = sys_malloc(sizeof(struct enter_stack) * 100);
}

static uint_32 base_x = 0;
static uint_32 base_y = 40;
static uint_32 margin = 0;
static uint_32 font_sz = 8;
static uint_32 line_pixels = 1000;
void console_write(void *buf, uint_32 len)
{
    lock_fetch(&gl_console_lock);
    char *c = buf;
    if (*c == '\b') {
        point_t lt = {.X = base_x, .Y = base_y};
        point_t rd = {.X = base_x + font_sz, .Y = base_y + font_sz * 2};
        gfx_add_clip(console_gfx, base_x, base_y, rd.X, rd.Y);
        fill_rect_solid(console_gfx, lt, rd, FSK_ROSY_BROWN | 0xff000000);
        if (base_x <= 0) {
            base_y -= (font_sz * 2);
            if (stack_pointer != 0) {
                stack_pointer--;
            }
            base_x = line_stack[stack_pointer].prev_line_x;
        } else {
            base_x -= font_sz;
        }
    } else if (*c == '\r' || *c == '\n') {
        line_stack[stack_pointer].prev_line_x = base_x;
        line_stack[stack_pointer].prev_line_y = base_y;
        stack_pointer++;
        base_x = 0;
        base_y += (2 * font_sz);
    } else {
        base_x += (font_sz + margin);
        if (base_x % line_pixels == 0) {
            base_y += (2 * font_sz);
            base_x = 0;
        }
        gfx_add_clip(console_gfx, base_x, base_y, font_sz, font_sz * 2);
        draw_2d_gfx_string(console_gfx, font_sz, base_x, base_y, font_color,
                           buf, len);
    }
    flip(console_gfx);
    gfx_clear_clip(console_gfx);
    gfx_free_clip(console_gfx);
    lock_release(&gl_console_lock);
}

int_32 ioctl_console(struct file *file, unsigned long request, void *argp)
{
    switch (request) {
    case IO_CONSOLE_SET: {
        validate(argp);
        console_gfx = *(gfx_context_t **) argp;
        return 0;
    }
    case IO_CONSOLE_COLOR: {
        validate(argp);
        font_color = *(int_32*) argp;
        return 0;
                           }
    default:
        return -EINVAL;
    }
    return -EINVAL;
}

int_32 console_create(struct partition *part,
                      struct dir *parent_d,
                      char *name,
                      void *target)
{
    uint_8 rollback_step = 0;
    char *buf = sys_malloc(1024);
    if (!buf) {
        //  kprint("Not enough memory for io buf");
        return -1;
    }

    // 1. Need new inode aka create a inode (inode_open())
    uint_32 inode_nr = inode_bitmap_alloc(part);
    if (inode_nr == -1) {
        // TODO:
        //  kprint("Not enough inode bitmap position.");
        return -1;
    }
    ASSERT(inode_nr != -1);
    struct inode *new_f_inode = sys_malloc(sizeof(struct inode));
    if (!new_f_inode) {
        // TODO:
        //  kprint("Not enough memory for inode .");
        // Recover! Need recover inode bitmap set
        rollback_step = 1;
        goto roll_back;
    }
    new_inode(inode_nr, new_f_inode);

    new_f_inode->i_mode = FT_CHAR << 11;
    new_f_inode->i_zones[0] = (uint_32 *) target;
    new_f_inode->i_count++;
    new_f_inode->i_dev = DNOCONSOLE;
    // 2. new dir_entry
    struct dir_entry new_entry;
    new_dir_entry(name, inode_nr, FT_CHAR, &new_entry);
    // 3.get file slot form global file_table
    lock_fetch(&g_ft_lock);
    // global file table index
    uint_32 fd_idx = occupy_file_table_slot();
    if (fd_idx == -1) {
        // TODO:
        //  kprint("Not enough slot at file table.");
        // Recover! Need recover inode bitmap set
        //        ! free new_f_inode
        rollback_step = 2;
        goto roll_back;
    }

    g_file_table[fd_idx].fd_pos = 0;
    g_file_table[fd_idx].fd_inode = new_f_inode;
    g_file_table[fd_idx].fd_inode->i_lock = false;
    lock_release(&g_ft_lock);

    // 4. flush dir_entry to parents directory
    // search new entry first
    int_32 failed = search_dir_entry(part, name, parent_d, &new_entry);
    if (!failed) {
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    } else {
        if (flush_dir_entry(part, parent_d, &new_entry, buf)) {
            // TODO:
            //  kprint("Failed at flush directory entry");
            rollback_step = 3;
            // Recover! Need recover inode bitmap set
            //        ! free new_f_inode
            //        ! clear g_file_table[fd_idx]
            goto roll_back;
        }
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    }

roll_back:
    switch (rollback_step) {
    case 3:
        memset(&g_file_table[fd_idx], 0, sizeof(struct file));
    case 2:
        sys_free(new_f_inode);
    case 1:
        set_value_bitmap(&part->inode_bitmap, inode_nr, 0);
        break;
    }
    sys_free(buf);
    return -1;
}
