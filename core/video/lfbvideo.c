#include <const.h>
#include <debug.h>
#include <device/devno-base.h>
#include <device/lfbvideo.h>
#include <errno-base.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/dir.h>
#include <device/ide.h>
#include <global.h>
#include <kernel/video.h>
#include <math.h>
#include <string.h>
#include <sys/memory.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;


#define validate(c)
typedef uint_32 size_t;

// globla VBE mode structure
static vbe_mode_info_t vbe_mode;

/**
 * Framebuffer control ioctls.
 * Used by the compositor to get display sizes and by the
 * resolution changer to initiate modesetting.
 */
int_32 ioctl_vid(struct file *file, unsigned long request, void *argp)
{
    switch (request) {
    case IO_VID_WIDTH:
        /* Get framebuffer width */
        validate(argp);
        *((size_t *) argp) = vbe_mode.x_resolution;
        return 0;
    case IO_VID_HEIGHT:
        /* Get framebuffer height */
        validate(argp);
        *((size_t *) argp) = vbe_mode.y_resolution;
        return 0;
    case IO_VID_DEPTH:
        /* Get framebuffer bit depth */
        validate(argp);
        *((size_t *) argp) = vbe_mode.bits_per_pixel;
        return 0;
    case IO_VID_STRIDE:
        /* Get framebuffer scanline stride */
        validate(argp);
        *((size_t *) argp) = vbe_mode.linear_bytes_per_scanline;
        return 0;
    case IO_VID_ADDR:
        /* Map framebuffer into userspace process */
        validate(argp);
        {
            uint_32 lfb_user_offset;
            if ((uint_8 *) argp == 0) {
                /* Pick an address and map it */
                PANIC("Error! need pointer from user spaces!\n");
            } else {
                /* validate((void *) (*(uint_32 *) argp)); */
                lfb_user_offset = (uint_32) *(uint_8**) argp; // buffer
            }
            const uint_32 fbsize_in_bytes =
                vbe_mode.y_resolution * vbe_mode.linear_bytes_per_scanline;
            uint_32 fb_page_count = DIV_ROUND_UP(fbsize_in_bytes, PG_SIZE);
            // For hardware, double size of framebuffer pages just in case
            /* fb_page_count *= 2;  // around 16M */
            for (uint_32 i = 0, fb_start = vbe_mode.physical_base_pointer;
                 i < fb_page_count;
                 i++, fb_start += PG_SIZE, lfb_user_offset += PG_SIZE)
                put_page((void *) fb_start, (void *) fb_start);

             *(uint_8 **) argp = (uint_8*) vbe_mode.physical_base_pointer;
        }
        return 0;
    case IO_VID_SIGNAL:
        /* ioctl to register for a signal (vid device change? idk) on display
         * change */
        /* display_change_recipient = this_core->current_process->id; */
        return 0;
    case IO_VID_SET:
        /* Initiate mode setting */
        validate(argp);
        /* lfb_set_resolution(((struct vid_size *) argp)->width, */
        /*                    ((struct vid_size *) argp)->height); */
        return 0;
    case IO_VID_DRIVER:
        validate(argp);
        /* memcpy(argp, lfb_driver_name, strlen(lfb_driver_name)); */
        return 0;
    case IO_VID_REINIT:
        validate(argp);
        /* return lfb_init(argp); */
        return 0;
    case IO_VID_VBE_MODE:
        {
        memcpy(argp, &vbe_mode, sizeof(struct vbe_mode_info));
        return 0;
        }
    default:
        return -EINVAL;
    }
    return -EINVAL;
}
/* Install framebuffer device */
static void finalize_graphics(const char *driver)
{
    /* lfb_driver_name = driver; */
    /* lfb_device = lfb_video_device_create(); */
    /* lfb_device->length  = lfb_resolution_s * lfb_resolution_y; #<{(| Size is
     * framebuffer size in bytes |)}># */
    /* vfs_mount("/dev/fb0", lfb_device); */
    sys_mount_device("/dev/fb0", DNOLFB, &vbe_mode);
}

int_32 open_lfb(struct partition *part, uint_32 inode_nr, uint_8 flags)
{
    lock_fetch(&g_ft_lock);
    int_32 gidx = occupy_file_table_slot();
    if (gidx == -1) {
        // TODO:
        // kprint("Not enough global file table slots. when open file");
        return -1;
    }
    g_file_table[gidx].fd_inode = inode_open(part, inode_nr);
    g_file_table[gidx].fd_pos = 0;
    g_file_table[gidx].fd_flag = flags;
    lock_release(&g_ft_lock);
    if ((flags & O_RDONLY) == O_RDONLY) {
        return install_thread_fd(gidx);
    } else {
        return -1;
    }
}

/**
 * create a char type file
 *
 * @return reture a global file table index
 *****************************************************************************/
int_32 lfbvideo_create(struct partition *part,
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
    new_f_inode->i_dev = DNOLFB;
    // 2. new dir_entry
    struct dir_entry new_entry;
    new_dir_entry(name, inode_nr, FT_CHAR, &new_entry);
    uint_32 fd_idx = 0;
    // 3.get file slot form global file_table
    /* lock_fetch(&g_ft_lock); */
    /* // global file table index */
    /* uint_32 fd_idx = occupy_file_table_slot(); */
    /* if (fd_idx == -1) { */
    /*     // TODO: */
    /*     //  kprint("Not enough slot at file table."); */
    /*     // Recover! Need recover inode bitmap set */
    /*     //        ! free new_f_inode */
    /*     rollback_step = 2; */
    /*     goto roll_back; */
    /* } */
    /*  */
    /* g_file_table[fd_idx].fd_pos = 0; */
    /* g_file_table[fd_idx].fd_inode = new_f_inode; */
    /* g_file_table[fd_idx].fd_inode->i_lock = false; */
    /* lock_release(&g_ft_lock); */

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

    // 5. flush parent inode
    //  flush_dir_entry will change parent directory inode size
    /* memset(buf, 0, 1024);  // 1024 for 2 sectors */
    /* flush_inode(part, parent_d->inode, buf); */
    /* 6. flush new inode */
    /* memset(buf, 0, 1024);  // 1024 for 2 sectors */
    /* flush_inode(part, new_f_inode, buf); */

    // 7. flush inode bitmap and zones bitmap
    /* flush_bitmap(part, INODE_BITMAP, inode_nr); */


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

int_32 lfb_init(char *argp)
{
    // alloc 2d graphics memory
    /* vbe_mode = **((vbe_mode_info_t **) VBE_MODE_INFO_POINTER); */
    memcpy(&vbe_mode, *((struct vbe_mode_info **) VBE_MODE_INFO_POINTER),
           sizeof(vbe_mode));

    finalize_graphics("qume");
    /* const uint_32 fbsize_in_bytes = */
    /*     vbe_mode.y_resolution * vbe_mode.linear_bytes_per_scanline; */
    /* uint_32 fb_page_count = DIV_ROUND_UP(fbsize_in_bytes, PG_SIZE); */
    /* // For hardware, double size of framebuffer pages just in case */
    /* fb_page_count *= 2;  // around 16M */
    /* for (uint_32 i = 0, fb_start = vbe_mode.physical_base_pointer; */
    /*      i < fb_page_count; i++, fb_start += PG_SIZE) */
    /*     put_page((void *) fb_start, (void *) fb_start); */

    // test VBE info
    vbe_info_t *vbe_info = *((struct vbe_info_structure **) VBE_INFO_POINTER);
    if (!strcmp("VESA", vbe_info->signature) && vbe_info != NULL) {
        // has vbe_info and right signature
        uint_32 mode = vbe_info->video_modes_pointer;
        uint_32 vers = vbe_info->version;
        switch (vbe_info->version) {
        case 0x100:
            // VBE 1.0
            break;
        case 0x200:
            // VBE 2.0
            break;
        case 0x300:
            // VBE 3.0
            break;
        default:
            // VBE unknow
            break;
        }
    }
}
