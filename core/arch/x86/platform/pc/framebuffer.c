#include <asm/page.h>
#include <global.h>

#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/phys_resource.h>
#include <frog/refcount.h>
#include <frog/string.h>
#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/framebuffer.h>
#include <kernel/panic.h>
#include <video/video.h>

#define BOOT_HANDOFF_LOW_START 0x0500ULL
#define BOOT_HANDOFF_LOW_END   0x100000ULL
#define FRAMEBUFFER_MAP_LIMIT  (16U * 1024U * 1024U)

struct pc_framebuffer_state {
        bool snapshot_done;
        int snapshot_result;
        bool registration_attempted;
        bool registered;
        vbe_info_t controller;
        vbe_mode_info_t mode;
        struct framebuffer_info info;
        unsigned long long aperture_length;
        struct device device;
        struct phys_resource aperture;
};

static struct pc_framebuffer_state pc_framebuffer;

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
                                 struct framebuffer_info *info,
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
        struct framebuffer_info info;
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
#endif
