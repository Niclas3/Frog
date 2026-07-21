#include <asm/page.h>
#include <global.h>

#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/fb.h>
#include <frog/kernel.h>
#include <frog/memory.h>
#include <frog/mman.h>
#include <frog/phys_resource.h>
#include <frog/refcount.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/uaccess.h>
#include <frog/vm.h>
#include <kernel/assert.h>
#include <kernel/bus.h>
#include <kernel/chardev.h>
#include <kernel/dev.h>
#include <kernel/device.h>
#include <kernel/framebuffer.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <video/video.h>

#define BOOT_HANDOFF_LOW_START 0x0500ULL
#define BOOT_HANDOFF_LOW_END   0x100000ULL
#define FRAMEBUFFER_MAP_LIMIT  (16U * 1024U * 1024U)

struct pc_framebuffer_state {
        bool snapshot_done;
        int snapshot_result;
        bool registration_attempted;
        bool registered;
        bool chardev_registered;
        int major;
        uint_32 live_mapping_objects;
        vbe_info_t controller;
        vbe_mode_info_t mode;
        struct frog_fb_info info;
        unsigned long long aperture_length;
        struct device device;
        struct phys_resource aperture;
};

static struct pc_framebuffer_state pc_framebuffer;

static int_32 pc_framebuffer_open(struct inode *inode, struct file *file);
static int_32 pc_framebuffer_close(struct file *file);
static int_32 pc_framebuffer_ioctl(struct file *file, uint_32 request,
                                  void *argp);
static int_32 pc_framebuffer_mmap(struct file *file, struct vm_area *vma);

static const struct file_operations pc_framebuffer_fops = {
        .open = pc_framebuffer_open,
        .close = pc_framebuffer_close,
        .ioctl = pc_framebuffer_ioctl,
        .mmap = pc_framebuffer_mmap,
};

static void pc_framebuffer_mapping_close(struct vm_mapping *mapping)
{
        struct pc_framebuffer_state *state = mapping->private_data;

        ASSERT(state != NULL && mapping->device == &state->device &&
               mapping->resource == &state->aperture &&
               mapping->file != NULL);
        lock_fetch(&state->device.lock);
        ASSERT(state->live_mapping_objects > 0);
        state->live_mapping_objects--;
        lock_release(&state->device.lock);

        (void) file_put(mapping->file);
        phys_resource_put(mapping->resource);
        device_put(mapping->device);
}

static const struct vm_operations pc_framebuffer_vm_ops = {
        .close = pc_framebuffer_mapping_close,
};

static int_32 pc_framebuffer_open(struct inode *inode, struct file *file)
{
        if (inode == NULL || file == NULL ||
            (inode->i_dev & 0xffffU) != 0 || file->private_data != NULL ||
            !pc_framebuffer.registered ||
            !pc_framebuffer.chardev_registered)
                return -ENODEV;
        if (!device_get_live(&pc_framebuffer.device))
                return -ENODEV;
        file->private_data = &pc_framebuffer;
        return 0;
}

static int_32 pc_framebuffer_close(struct file *file)
{
        struct pc_framebuffer_state *state;

        if (file == NULL || file->f_op != &pc_framebuffer_fops ||
            file->private_data != &pc_framebuffer)
                return -EINVAL;
        state = file->private_data;
        file->private_data = NULL;
        device_put(&state->device);
        return 0;
}

static int_32 pc_framebuffer_ioctl(struct file *file, uint_32 request,
                                  void *argp)
{
        struct pc_framebuffer_state *state;
        struct frog_fb_info info;

        if (request != FROG_FB_IOCTL_GET_INFO)
                return -ENOTTY;
        if (file == NULL || file->f_op != &pc_framebuffer_fops ||
            file->private_data != &pc_framebuffer)
                return -ENODEV;
        state = file->private_data;
        lock_fetch(&state->device.lock);
        if (!state->registered || state->device.state != DEVICE_LIVE) {
                lock_release(&state->device.lock);
                return -ENODEV;
        }
        info = state->info;
        lock_release(&state->device.lock);
        return copy_to_user(argp, &info, sizeof(info));
}

static int framebuffer_prepare_mmap(struct pc_framebuffer_state *state,
                                    struct file *file,
                                    struct vm_area *vma,
                                    struct mm_struct *mm)
{
        struct vm_mapping *mapping;
        bool resource_pinned = false;
        int result;

        if (state == NULL || file == NULL || vma == NULL || mm == NULL ||
            file->f_op != &pc_framebuffer_fops ||
            file->private_data != state)
                return -ENODEV;
        if ((file->f_flag & O_ACCMODE) != O_RDWR)
                return -EACCES;
        mapping = vma->mapping;
        if (mapping == NULL ||
            mapping->backing_type != VM_BACKING_DEVICE_BORROWED ||
            mapping->state != VM_MAPPING_NEW || mapping->vm_ops != NULL ||
            vma->start != 0 || vma->end != state->info.map_length ||
            vma->prot != (PROT_READ | PROT_WRITE) ||
            vma->flags != MAP_SHARED || vma->page_offset != 0)
                return -EINVAL;
        if (vm_mm_maps_device(mm, &state->device))
                return -EBUSY;
        if (!device_get_live(&state->device))
                return -ENODEV;

        lock_fetch(&state->device.lock);
        if (!state->registered || state->device.state != DEVICE_LIVE) {
                result = -ENODEV;
                goto unlock;
        }
        resource_pinned = phys_resource_get_live(&state->aperture);
        if (!resource_pinned) {
                result = -ENODEV;
                goto unlock;
        }
        if (state->live_mapping_objects == UINT_MAX) {
                result = -EOVERFLOW;
                goto unlock;
        }

        mapping->vm_ops = &pc_framebuffer_vm_ops;
        result = vm_mapping_prepare_device(mapping, file, &state->device,
                                           &state->aperture, state);
        if (result != 0) {
                mapping->vm_ops = NULL;
                goto unlock;
        }
        state->live_mapping_objects++;
        lock_release(&state->device.lock);
        return 0;

unlock:
        lock_release(&state->device.lock);
        if (resource_pinned)
                phys_resource_put(&state->aperture);
        device_put(&state->device);
        return result;
}

static int_32 pc_framebuffer_mmap(struct file *file, struct vm_area *vma)
{
        return framebuffer_prepare_mmap(&pc_framebuffer, file, vma,
                                        running_thread()->mm);
}

static bool handoff_object_valid(uint_32 address, uint_32 size)
{
        unsigned long long end = (unsigned long long) address + size;

        return address >= BOOT_HANDOFF_LOW_START &&
               end <= BOOT_HANDOFF_LOW_END && end >= address;
}

static int framebuffer_handoff_addresses(uint_32 mode_address,
                                         uint_32 controller_address)
{
        if ((mode_address == 0 && controller_address == 0) ||
            (mode_address == 1 && controller_address == 1))
                return -ENODEV;
        if (!handoff_object_valid(mode_address, sizeof(vbe_mode_info_t)) ||
            !handoff_object_valid(controller_address, sizeof(vbe_info_t)))
                return -EINVAL;
        return 0;
}

static uint_32 channel_mask(uint_8 width)
{
        if (width == 0 || width > 8)
                return 0;
        return (1U << width) - 1U;
}

static bool channel_valid(uint_8 width, uint_8 position)
{
        return width > 0 && width <= 8 && position < 32 &&
               width <= 32 - position;
}

static bool channels_overlap(uint_8 first_width, uint_8 first_position,
                             uint_8 second_width, uint_8 second_position)
{
        uint_32 first = channel_mask(first_width) << first_position;
        uint_32 second = channel_mask(second_width) << second_position;

        return (first & second) != 0;
}

static int framebuffer_align_length(unsigned long long visible,
                                    uint_32 *mapped_length)
{
        unsigned long long mapped;

        if (mapped_length == NULL || visible == 0 ||
            visible > 0xffffffffULL - (PAGE_SIZE - 1U))
                return -EOVERFLOW;
        mapped = (visible + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1ULL);
        *mapped_length = (uint_32) mapped;
        return 0;
}

static int framebuffer_normalize(const vbe_info_t *controller,
                                 const vbe_mode_info_t *mode,
                                 struct frog_fb_info *info,
                                 unsigned long long *aperture_length)
{
        unsigned long long aperture;
        unsigned long long aperture_end;
        unsigned long long minimum_pitch;
        unsigned long long visible;
        uint_32 mapped;
        uint_16 pitch;
        uint_8 red_size;
        uint_8 red_position;
        uint_8 green_size;
        uint_8 green_position;
        uint_8 blue_size;
        uint_8 blue_position;
        bool linear_layout;

        if (controller == NULL || mode == NULL || info == NULL ||
            aperture_length == NULL)
                return -EINVAL;
        if (memcmp(controller->signature, "VESA", 4) != 0 ||
            controller->version < 0x0200 || controller->video_memory == 0)
                return -EINVAL;
        if ((mode->mode_attributes & 0x0099U) != 0x0099U ||
            mode->x_resolution == 0 || mode->y_resolution == 0 ||
            mode->number_of_planes != 1 || mode->bits_per_pixel != 32 ||
            mode->memory_model != 6 || mode->physical_base_pointer == 0 ||
            (mode->physical_base_pointer & (PAGE_SIZE - 1U)) != 0)
                return -EINVAL;

        linear_layout = controller->version >= 0x0300;
        if (linear_layout) {
                if (mode->linear_bytes_per_scanline == 0)
                        return -EINVAL;
                pitch = mode->linear_bytes_per_scanline;
                red_size = mode->linear_red_mask_size;
                red_position = mode->linear_red_field_position;
                green_size = mode->linear_green_mask_size;
                green_position = mode->linear_green_field_position;
                blue_size = mode->linear_blue_mask_size;
                blue_position = mode->linear_blue_field_position;
        } else {
                pitch = mode->bytes_per_scanline;
                red_size = mode->red_mask_size;
                red_position = mode->red_field_position;
                green_size = mode->green_mask_size;
                green_position = mode->green_field_position;
                blue_size = mode->blue_mask_size;
                blue_position = mode->blue_field_position;
        }

        minimum_pitch = (unsigned long long) mode->x_resolution * 4U;
        if (pitch < minimum_pitch || !channel_valid(red_size, red_position) ||
            !channel_valid(green_size, green_position) ||
            !channel_valid(blue_size, blue_position) ||
            channels_overlap(red_size, red_position, green_size,
                             green_position) ||
            channels_overlap(red_size, red_position, blue_size,
                             blue_position) ||
            channels_overlap(green_size, green_position, blue_size,
                             blue_position))
                return -EINVAL;

        aperture = (unsigned long long) controller->video_memory << 16;
        visible = (unsigned long long) pitch * mode->y_resolution;
        if (framebuffer_align_length(visible, &mapped) != 0)
                return -EOVERFLOW;
        aperture_end = (unsigned long long) mode->physical_base_pointer +
                       aperture;
        if (aperture_end < mode->physical_base_pointer ||
            aperture_end > 0x100000000ULL)
                return -EOVERFLOW;
        if (mapped > FRAMEBUFFER_MAP_LIMIT || mapped > aperture)
                return -EINVAL;

        info->width = mode->x_resolution;
        info->height = mode->y_resolution;
        info->pitch = pitch;
        info->bits_per_pixel = mode->bits_per_pixel;
        info->red_position = red_position;
        info->red_size = red_size;
        info->green_position = green_position;
        info->green_size = green_size;
        info->blue_position = blue_position;
        info->blue_size = blue_size;
        info->visible_length = (uint_32) visible;
        info->map_length = (uint_32) mapped;
        *aperture_length = aperture;
        return 0;
}

int pc_framebuffer_snapshot_handoff(void)
{
        uint_32 mode_address;
        uint_32 controller_address;
        int result;

        if (pc_framebuffer.snapshot_done)
                return pc_framebuffer.snapshot_result;
        pc_framebuffer.snapshot_done = true;

        mode_address = *(volatile uint_32 *) VBE_MODE_INFO_POINTER;
        controller_address = *(volatile uint_32 *) VBE_INFO_POINTER;
        result = framebuffer_handoff_addresses(mode_address,
                                               controller_address);
        if (result != 0) {
                pc_framebuffer.snapshot_result = result;
                return result;
        }

        memcpy(&pc_framebuffer.mode, (const void *) mode_address,
               sizeof(pc_framebuffer.mode));
        memcpy(&pc_framebuffer.controller, (const void *) controller_address,
               sizeof(pc_framebuffer.controller));
        result = framebuffer_normalize(&pc_framebuffer.controller,
                                       &pc_framebuffer.mode,
                                       &pc_framebuffer.info,
                                       &pc_framebuffer.aperture_length);
        pc_framebuffer.snapshot_result = result;
        return result;
}

int pc_framebuffer_register_aperture(struct bus_type *bus)
{
        int result;

        if (!pc_framebuffer.snapshot_done)
                return -EINVAL;
        if (pc_framebuffer.snapshot_result != 0)
                return pc_framebuffer.snapshot_result;
        if (bus == NULL)
                return -EINVAL;
        if (pc_framebuffer.registration_attempted)
                return -EALREADY;
        pc_framebuffer.registration_attempted = true;
        pc_framebuffer.major = -1;

        device_init(&pc_framebuffer.device, NULL);
        pc_framebuffer.device.name = "pc-framebuffer";
        pc_framebuffer.device.bus = bus;
        phys_resource_init(&pc_framebuffer.aperture);

        lock_fetch(&pc_framebuffer.device.lock);
        result = phys_resource_register(
            &pc_framebuffer.aperture,
            pc_framebuffer.mode.physical_base_pointer,
            pc_framebuffer.aperture_length,
            PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED);
        if (result != 0)
                goto out;
        if (!phys_resource_contains(&pc_framebuffer.aperture,
                                    pc_framebuffer.aperture.start,
                                    pc_framebuffer.info.map_length)) {
                result = -EINVAL;
                goto unregister_resource;
        }

        result = register_device(&pc_framebuffer.device);
        if (result != 0)
                goto unregister_resource;
        pc_framebuffer.registered = true;
        goto out;

unregister_resource:
        if (phys_resource_unregister(&pc_framebuffer.aperture) != 0)
                PANIC("framebuffer resource rollback failed");
out:
        lock_release(&pc_framebuffer.device.lock);
        return result;
}

int pc_framebuffer_register_chardev(void)
{
        int major;
        int result;

        if (!pc_framebuffer.registered ||
            pc_framebuffer.device.state != DEVICE_LIVE)
                return -ENODEV;
        if (pc_framebuffer.chardev_registered)
                return -EALREADY;

        major = register_chrdev(0, &pc_framebuffer_fops);
        if (major < 0)
                return -ENOSPC;
        pc_framebuffer.major = major;
        pc_framebuffer.chardev_registered = true;
        result = devfs_create_node("fb0", DEV_TYPE_CHAR, major, 0);
        if (result != 0) {
                pc_framebuffer.chardev_registered = false;
                pc_framebuffer.major = -1;
                (void) unregister_chrdev(major);
                return result;
        }
        return 0;
}

static int framebuffer_mode_change_allowed(
    struct pc_framebuffer_state *state)
{
        int result = 0;

        if (state == NULL)
                return -EINVAL;
        lock_fetch(&state->device.lock);
        if (!state->registered || state->device.state != DEVICE_LIVE)
                result = -ENODEV;
        else if (state->live_mapping_objects != 0)
                result = -EBUSY;
        lock_release(&state->device.lock);
        return result;
}

int pc_framebuffer_mode_change_allowed(void)
{
        return framebuffer_mode_change_allowed(&pc_framebuffer);
}

int pc_framebuffer_unregister(void)
{
        int major;
        int result;

        if (!pc_framebuffer.registered ||
            !pc_framebuffer.chardev_registered)
                return -ENODEV;
        result = pc_framebuffer_mode_change_allowed();
        if (result != 0)
                return result;
        result = device_begin_unregister(&pc_framebuffer.device);
        if (result != 0)
                return result;

        lock_fetch(&pc_framebuffer.device.lock);
        bool resource_ready =
            pc_framebuffer.aperture.state == PHYS_RESOURCE_REGISTERED &&
            refcount_read(&pc_framebuffer.aperture.refs) == 1;
        lock_release(&pc_framebuffer.device.lock);
        if (!resource_ready) {
                ASSERT(device_cancel_unregister(&pc_framebuffer.device) == 0);
                return -EBUSY;
        }

        major = pc_framebuffer.major;
        result = devfs_remove_node("fb0", DEV_TYPE_CHAR, major, 0);
        if (result != 0) {
                ASSERT(device_cancel_unregister(&pc_framebuffer.device) == 0);
                return result;
        }

        lock_fetch(&pc_framebuffer.device.lock);
        result = phys_resource_unregister(&pc_framebuffer.aperture);
        lock_release(&pc_framebuffer.device.lock);
        if (result != 0)
                PANIC("framebuffer resource teardown failed");
        ASSERT(unregister_chrdev(major) == 0);
        result = device_finish_unregister(&pc_framebuffer.device);
        if (result != 0)
                PANIC("framebuffer device teardown failed");

        pc_framebuffer.chardev_registered = false;
        pc_framebuffer.registered = false;
        pc_framebuffer.major = -1;
        return 0;
}

int pc_framebuffer_get_live(struct framebuffer_ref *ref)
{
        if (ref == NULL)
                return -EINVAL;
        memset(ref, 0, sizeof(*ref));
        if (!pc_framebuffer.registered ||
            !device_get_live(&pc_framebuffer.device))
                return -ENODEV;

        lock_fetch(&pc_framebuffer.device.lock);
        if (!phys_resource_get_live(&pc_framebuffer.aperture)) {
                lock_release(&pc_framebuffer.device.lock);
                device_put(&pc_framebuffer.device);
                return -ENODEV;
        }
        ref->device = &pc_framebuffer.device;
        ref->resource = &pc_framebuffer.aperture;
        ref->info = pc_framebuffer.info;
        lock_release(&pc_framebuffer.device.lock);
        return 0;
}

void pc_framebuffer_put(struct framebuffer_ref *ref)
{
        if (ref == NULL || ref->device == NULL || ref->resource == NULL)
                return;
        phys_resource_put(ref->resource);
        device_put(ref->device);
        memset(ref, 0, sizeof(*ref));
}

#ifdef CONFIG_QEMU_TEST
#include <kernel/qemu_test.h>

static void framebuffer_valid_fixture(vbe_info_t *controller,
                                      vbe_mode_info_t *mode)
{
        memset(controller, 0, sizeof(*controller));
        memset(mode, 0, sizeof(*mode));
        memcpy(controller->signature, "VESA", 4);
        controller->version = 0x0300;
        controller->video_memory = 256;
        mode->mode_attributes = 0x0099;
        mode->bytes_per_scanline = 4096;
        mode->linear_bytes_per_scanline = 4096;
        mode->x_resolution = 1024;
        mode->y_resolution = 768;
        mode->number_of_planes = 1;
        mode->bits_per_pixel = 32;
        mode->memory_model = 6;
        mode->physical_base_pointer = 0xfd000000U;
        mode->linear_red_mask_size = 8;
        mode->linear_red_field_position = 16;
        mode->linear_green_mask_size = 8;
        mode->linear_green_field_position = 8;
        mode->linear_blue_mask_size = 8;
        mode->linear_blue_field_position = 0;
}

static void framebuffer_test_case(const char *name, bool passed, int *failures)
{
        frog_test_case(name, passed);
        if (!passed)
                (*failures)++;
}

int pc_framebuffer_regression_test(void)
{
        vbe_info_t controller;
        vbe_mode_info_t mode;
        struct frog_fb_info info;
        struct phys_resource_registry registry;
        struct phys_resource ram;
        struct phys_resource overlap;
        struct phys_resource invalid;
        unsigned long long aperture;
        uint_32 mapped;
        int failures = 0;
        int result;

        framebuffer_valid_fixture(&controller, &mode);
        result = framebuffer_normalize(&controller, &mode, &info, &aperture);
        framebuffer_test_case(
            "framebuffer.metadata-valid",
            result == 0 && info.visible_length == 4096U * 768U &&
                info.map_length == info.visible_length &&
                aperture == 16ULL * 1024ULL * 1024ULL,
            &failures);

        controller.signature[0] = 'X';
        framebuffer_test_case(
            "framebuffer.metadata-signature",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        controller.version = 0x0102;
        framebuffer_test_case(
            "framebuffer.metadata-version",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        controller.video_memory = 0;
        framebuffer_test_case(
            "framebuffer.metadata-memory",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        mode.linear_bytes_per_scanline = 1024;
        framebuffer_test_case(
            "framebuffer.metadata-pitch",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        mode.linear_green_field_position = 16;
        framebuffer_test_case(
            "framebuffer.metadata-channel",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        mode.physical_base_pointer++;
        framebuffer_test_case(
            "framebuffer.metadata-alignment",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        controller.video_memory = 1;
        framebuffer_test_case(
            "framebuffer.metadata-aperture",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EINVAL,
            &failures);
        framebuffer_valid_fixture(&controller, &mode);
        mode.physical_base_pointer = 0xfff00000U;
        framebuffer_test_case(
            "framebuffer.metadata-physical-overflow",
            framebuffer_normalize(&controller, &mode, &info, &aperture) ==
                -EOVERFLOW,
            &failures);

        framebuffer_test_case("framebuffer.handoff-zero",
                              framebuffer_handoff_addresses(0, 0) == -ENODEV,
                              &failures);
        framebuffer_test_case("framebuffer.handoff-one",
                              framebuffer_handoff_addresses(1, 1) == -ENODEV,
                              &failures);
        framebuffer_test_case("framebuffer.handoff-mixed",
                              framebuffer_handoff_addresses(0, 1) == -EINVAL,
                              &failures);
        framebuffer_test_case(
            "framebuffer.handoff-low-range",
            framebuffer_handoff_addresses(BOOT_HANDOFF_LOW_START - 1U,
                                           BOOT_HANDOFF_LOW_START) == -EINVAL,
            &failures);
        framebuffer_test_case(
            "framebuffer.handoff-high-range",
            framebuffer_handoff_addresses(
                BOOT_HANDOFF_LOW_END - sizeof(vbe_mode_info_t) + 1U,
                BOOT_HANDOFF_LOW_START) == -EINVAL,
            &failures);
        framebuffer_test_case(
            "framebuffer.metadata-align-overflow",
            framebuffer_align_length(0xffffffffULL, &mapped) == -EOVERFLOW,
            &failures);

        phys_resource_registry_init(&registry);
        phys_resource_init(&ram);
        phys_resource_init(&overlap);
        result = phys_resource_registry_register(
            &registry, &ram, 0x00100000ULL, PAGE_SIZE,
            PHYS_RESOURCE_RAM, VM_CACHE_WRITE_BACK);
        framebuffer_test_case("framebuffer.resource-fixture",
                              result == 0, &failures);
        if (result == 0) {
                result = phys_resource_registry_register(
                    &registry, &overlap, 0x00100000ULL, PAGE_SIZE,
                    PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED);
                framebuffer_test_case(
                    "framebuffer.resource-reject-ram",
                    result == -EBUSY &&
                        overlap.state == PHYS_RESOURCE_NEW &&
                        refcount_read(&overlap.refs) == 0,
                    &failures);
                framebuffer_test_case(
                    "framebuffer.mapper-reject-ram",
                    phys_resource_get_live(&ram) &&
                        map_kernel_framebuffer_pinned(&ram, PAGE_SIZE) != 0,
                    &failures);
                if (refcount_read(&ram.refs) == 2)
                        phys_resource_put(&ram);
                framebuffer_test_case(
                    "framebuffer.resource-fixture-cleanup",
                    phys_resource_registry_unregister(&registry, &ram) == 0,
                    &failures);
        }

        framebuffer_test_case(
            "framebuffer.mapper-reject-unregistered",
            map_kernel_framebuffer_pinned(&overlap, PAGE_SIZE) != 0,
            &failures);

        phys_resource_init(&invalid);
        invalid.start = 0xfd000000ULL;
        invalid.end = invalid.start + PAGE_SIZE;
        invalid.type = PHYS_RESOURCE_MMIO;
        invalid.cache_mode = VM_CACHE_UNCACHED;
        invalid.state = PHYS_RESOURCE_REGISTERED;
        refcount_init(&invalid.refs, 1);
        framebuffer_test_case(
            "framebuffer.mapper-reject-unpinned",
            map_kernel_framebuffer_pinned(&invalid, PAGE_SIZE) != 0,
            &failures);
        invalid.cache_mode = VM_CACHE_WRITE_BACK;
        refcount_init(&invalid.refs, 2);
        framebuffer_test_case(
            "framebuffer.mapper-reject-writeback",
            map_kernel_framebuffer_pinned(&invalid, PAGE_SIZE) != 0,
            &failures);
        return failures;
}

int pc_framebuffer_driver_regression_test(void)
{
        struct pc_framebuffer_state *state = &pc_framebuffer;
        struct file *bad_file = NULL;
        struct file *mapped_file = NULL;
        struct file *duplicate_file = NULL;
        struct file *other_file = NULL;
        struct vm_mapping *bad_mapping = NULL;
        struct vm_mapping *mapping = NULL;
        struct vm_mapping *duplicate_mapping = NULL;
        struct vm_mapping *other_mapping = NULL;
        struct vm_area *bad_vma = NULL;
        struct vm_area *vma = NULL;
        struct vm_area *duplicate_vma = NULL;
        struct vm_area *other_vma = NULL;
        struct mm_struct *mm = NULL;
        struct mm_struct *other_mm = NULL;
        struct inode *inode = NULL;
        bool vma_inserted = false;
        bool removal_ok = true;
        int failures = 0;
        int result;

        result = vfs_open_file("/dev/fb0", O_RDWR, &bad_file);
        framebuffer_test_case(
            "framebuffer.driver-open",
            result == 0 && bad_file != NULL &&
                bad_file->private_data == state &&
                refcount_read(&state->device.refs) == 2,
            &failures);
        if (result != 0 || bad_file == NULL)
                return failures;

        inode = bad_file->f_inode;
        framebuffer_test_case(
            "framebuffer.driver-ioctl-errors",
            vfs_ioctl(bad_file, FROG_FB_IOCTL_GET_INFO + 1U, NULL) ==
                    -ENOTTY &&
                vfs_ioctl(bad_file, FROG_FB_IOCTL_GET_INFO, NULL) ==
                    -EFAULT,
            &failures);

        mm = mm_create();
        bad_mapping = vm_mapping_alloc(VM_BACKING_DEVICE_BORROWED, NULL);
        if (bad_mapping != NULL)
                bad_vma = vm_area_alloc(state->info.map_length - PAGE_SIZE,
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        0, bad_mapping);
        result = bad_vma == NULL
                     ? -ENOMEM
                     : framebuffer_prepare_mmap(state, bad_file, bad_vma, mm);
        framebuffer_test_case(
            "framebuffer.driver-mmap-reject",
            mm != NULL && result == -EINVAL &&
                bad_mapping != NULL &&
                bad_mapping->state == VM_MAPPING_NEW &&
                state->live_mapping_objects == 0 &&
                refcount_read(&state->device.refs) == 2 &&
                refcount_read(&state->aperture.refs) == 1,
            &failures);
        kfree(bad_vma);
        vm_mapping_put(bad_mapping);
        (void) vfs_close(bad_file);
        bad_file = NULL;

        result = mm == NULL
                     ? -ENOMEM
                     : vfs_open_file("/dev/fb0", O_RDWR, &mapped_file);
        mapping = vm_mapping_alloc(VM_BACKING_DEVICE_BORROWED, NULL);
        if (mapping != NULL)
                vma = vm_area_alloc(state->info.map_length,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, 0,
                                    mapping);
        if (result == 0 && vma != NULL)
                result = framebuffer_prepare_mmap(state, mapped_file, vma,
                                                  mm);
        framebuffer_test_case(
            "framebuffer.driver-mmap-prepare",
            result == 0 && mapping != NULL &&
                mapping->state == VM_MAPPING_PREPARED &&
                mapping->file == mapped_file &&
                mapping->device == &state->device &&
                mapping->resource == &state->aperture &&
                state->live_mapping_objects == 1 &&
                refcount_read(&state->aperture.refs) == 2 &&
                refcount_read(&state->device.refs) == 3,
            &failures);
        if (mapping != NULL && mapping->state == VM_MAPPING_PREPARED)
                mapped_file = NULL;

        if (result == 0) {
                vma->start = VM_MMAP_START;
                vma->end = VM_MMAP_START + state->info.map_length;
                lock_fetch(&mm->mmap_lock);
                result = vm_area_insert(mm, vma);
                lock_release(&mm->mmap_lock);
                vma_inserted = result == 0;
        }

        if (vma_inserted)
                result = vfs_open_file("/dev/fb0", O_RDWR,
                                       &duplicate_file);
        duplicate_mapping =
            vm_mapping_alloc(VM_BACKING_DEVICE_BORROWED, NULL);
        if (duplicate_mapping != NULL)
                duplicate_vma = vm_area_alloc(
                    state->info.map_length, PROT_READ | PROT_WRITE,
                    MAP_SHARED, 0, duplicate_mapping);
        if (result == 0 && duplicate_vma != NULL)
                result = framebuffer_prepare_mmap(
                    state, duplicate_file, duplicate_vma, mm);
        framebuffer_test_case(
            "framebuffer.driver-mmap-duplicate",
            vma_inserted && result == -EBUSY &&
                duplicate_mapping != NULL &&
                duplicate_mapping->state == VM_MAPPING_NEW &&
                state->live_mapping_objects == 1 &&
                refcount_read(&state->aperture.refs) == 2 &&
                refcount_read(&state->device.refs) == 4,
            &failures);
        if (duplicate_mapping != NULL &&
            duplicate_mapping->state == VM_MAPPING_PREPARED)
                duplicate_file = NULL;
        kfree(duplicate_vma);
        vm_mapping_put(duplicate_mapping);
        if (duplicate_file != NULL)
                (void) vfs_close(duplicate_file);

        other_mm = mm_create();
        result = other_mm == NULL
                     ? -ENOMEM
                     : vfs_open_file("/dev/fb0", O_RDWR, &other_file);
        other_mapping = vm_mapping_alloc(VM_BACKING_DEVICE_BORROWED, NULL);
        if (other_mapping != NULL)
                other_vma = vm_area_alloc(
                    state->info.map_length, PROT_READ | PROT_WRITE,
                    MAP_SHARED, 0, other_mapping);
        if (result == 0 && other_vma != NULL)
                result = framebuffer_prepare_mmap(
                    state, other_file, other_vma, other_mm);
        framebuffer_test_case(
            "framebuffer.driver-mmap-other-mm",
            vma_inserted && result == 0 && other_mapping != NULL &&
                other_mapping->state == VM_MAPPING_PREPARED &&
                state->live_mapping_objects == 2 &&
                refcount_read(&state->aperture.refs) == 3 &&
                refcount_read(&state->device.refs) == 5,
            &failures);
        if (other_mapping != NULL &&
            other_mapping->state == VM_MAPPING_PREPARED)
                other_file = NULL;
        kfree(other_vma);
        vm_mapping_put(other_mapping);
        if (other_file != NULL)
                (void) vfs_close(other_file);
        mm_destroy(other_mm);

        framebuffer_test_case(
            "framebuffer.driver-mapping-busy",
            framebuffer_mode_change_allowed(state) == -EBUSY &&
                pc_framebuffer_unregister() == -EBUSY &&
                device_begin_unregister(&state->device) == -EBUSY &&
                state->device.state == DEVICE_LIVE,
            &failures);

        if (vma_inserted) {
                struct vm_area *removed;

                lock_fetch(&mm->mmap_lock);
                removed = vm_area_remove_exact(mm, vma->start, vma->end);
                lock_release(&mm->mmap_lock);
                removal_ok = removed == vma;
                if (removal_ok)
                        vma_inserted = false;
        }
        if (removal_ok) {
                kfree(vma);
                vm_mapping_put(mapping);
        }
        if (mapped_file != NULL)
                (void) vfs_close(mapped_file);

        framebuffer_test_case(
            "framebuffer.driver-mapping-cleanup",
            removal_ok && !vma_inserted &&
                state->live_mapping_objects == 0 &&
                framebuffer_mode_change_allowed(state) == 0 &&
                refcount_read(&state->aperture.refs) == 1 &&
                refcount_read(&state->device.refs) == 1 &&
                inode != NULL && inode->i_count == 0,
            &failures);
        if (!vma_inserted)
                mm_destroy(mm);
        return failures;
}
#endif
