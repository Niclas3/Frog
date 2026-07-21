#include <frog/errno.h>
#include <frog/fb.h>
#include <frog/fcntl.h>
#include <frog/mman.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define FB_WIDTH       1024U
#define FB_HEIGHT      768U
#define FB_PITCH       (FB_WIDTH * 4U)
#define FB_MAP_LENGTH  (FB_PITCH * FB_HEIGHT)
#define PAGE_SIZE      4096U
#define FB_ADDRESS     0x40000000U

static const char framebuffer_path[] = "/dev/fb0";
static bool all_passed = true;

static int_32 raw_syscall0(uint_32 number)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number)
                         : "memory");
        return result;
}

static int_32 raw_syscall1(uint_32 number, uint_32 arg)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg)
                         : "memory");
        return result;
}

static int_32 raw_syscall2(uint_32 number, uint_32 arg1, uint_32 arg2)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2)
                         : "memory");
        return result;
}

static void report(uint_32 id, bool passed)
{
        if (raw_syscall2(SYS_TEST_REPORT, id, passed) != 0)
                passed = false;
        all_passed = passed && all_passed;
}

static void finish_failure(void) __attribute__((noreturn));
static void finish_failure(void)
{
        raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}

static void process_exit(int_32 status) __attribute__((noreturn));
static void process_exit(int_32 status)
{
        exit(status);
        for (;;)
                __asm__ volatile("pause");
}

static bool wait_status(int_32 pid, int_32 expected)
{
        int_32 status = 0;

        return pid > 0 && (int_32) wait(&status) == pid &&
               status == expected;
}

static int_32 mmap_raw(struct frog_mmap_args *args)
{
        return raw_syscall1(SYS_MMAP, (uint_32) args);
}

static uint_32 rgb(const struct frog_fb_info *info,
                   uint_8 red, uint_8 green, uint_8 blue)
{
        return ((uint_32) red << info->red_position) |
               ((uint_32) green << info->green_position) |
               ((uint_32) blue << info->blue_position);
}

static volatile uint_32 *pixel(volatile uint_8 *framebuffer,
                               const struct frog_fb_info *info,
                               uint_32 x, uint_32 y)
{
        return (volatile uint_32 *)
            (framebuffer + y * info->pitch + x * sizeof(uint_32));
}

static void fill_rect(volatile uint_8 *framebuffer,
                      const struct frog_fb_info *info,
                      uint_32 left, uint_32 top,
                      uint_32 right, uint_32 bottom, uint_32 color)
{
        for (uint_32 y = top; y < bottom; y++) {
                volatile uint_32 *row = pixel(framebuffer, info, left, y);

                for (uint_32 x = left; x < right; x++)
                        *row++ = color;
        }
}

static void draw_expected_base(volatile uint_8 *framebuffer,
                               const struct frog_fb_info *info)
{
        uint_32 red = rgb(info, 255, 0, 0);
        uint_32 green = rgb(info, 0, 255, 0);
        uint_32 blue = rgb(info, 0, 0, 255);

        fill_rect(framebuffer, info, 0, 0, info->width / 3U,
                  info->height, red);
        fill_rect(framebuffer, info, info->width / 3U, 0,
                  2U * info->width / 3U, info->height, green);
        fill_rect(framebuffer, info, 2U * info->width / 3U, 0,
                  info->width, info->height, blue);
}

static bool framebuffer_info_valid(const struct frog_fb_info *info)
{
        return info->width == FB_WIDTH && info->height == FB_HEIGHT &&
               info->pitch == FB_PITCH && info->bits_per_pixel == 32U &&
               info->red_position == 16U && info->red_size == 8U &&
               info->green_position == 8U && info->green_size == 8U &&
               info->blue_position == 0U && info->blue_size == 8U &&
               info->visible_length == FB_MAP_LENGTH &&
               info->map_length == FB_MAP_LENGTH;
}

static bool test_mmap_arguments(int_32 fd, uint_32 length)
{
        struct frog_mmap_args args = {
            .addr = 0,
            .length = length,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_SHARED,
            .fd = fd,
            .offset = 0,
        };
        bool passed = raw_syscall1(SYS_MMAP, 0) == -EFAULT &&
                      raw_syscall1(SYS_MMAP, 0xbffffff0U) == -EFAULT;

        args.addr = PAGE_SIZE;
        passed = mmap_raw(&args) == -EINVAL && passed;
        args.addr = 0;
        args.length = 0;
        passed = mmap_raw(&args) == -EINVAL && passed;
        args.length = length - 1U;
        passed = mmap_raw(&args) == -EINVAL && passed;
        args.length = length;
        args.prot = PROT_READ;
        passed = mmap_raw(&args) == -EINVAL && passed;
        args.prot = PROT_READ | PROT_WRITE;
        args.flags = 0x80000000U;
        passed = mmap_raw(&args) == -EINVAL && passed;
        args.flags = MAP_PRIVATE;
        passed = mmap_raw(&args) == -EOPNOTSUPP && passed;
        return passed;
}

static bool test_read_only_mapping(uint_32 length)
{
        int_32 fd = open(framebuffer_path, O_RDONLY);
        struct frog_mmap_args args = {
            .addr = 0,
            .length = length,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_SHARED,
            .fd = fd,
            .offset = 0,
        };
        bool passed = fd >= 0 && mmap_raw(&args) == -EACCES;

        if (fd >= 0)
                passed = close(fd) == 0 && passed;
        return passed;
}

static bool test_child_first(volatile uint_8 *framebuffer,
                             const struct frog_fb_info *info)
{
        uint_32 magenta = rgb(info, 255, 0, 255);
        int_32 child = (int_32) fork();

        if (child == 0) {
                fill_rect(framebuffer, info, 64, 64, 96, 96, magenta);
                process_exit(*pixel(framebuffer, info, 64, 64) == magenta ?
                             41 : 42);
        }
        return wait_status(child, 41) &&
               *pixel(framebuffer, info, 95, 95) == magenta;
}

static bool test_parent_first(volatile uint_8 *framebuffer,
                              const struct frog_fb_info *info)
{
        volatile uint_32 *control = pixel(framebuffer, info, 128, 64);
        uint_32 release = rgb(info, 1, 2, 3);
        uint_32 yellow = rgb(info, 255, 255, 0);
        int_32 worker;

        *control = 0;
        worker = (int_32) fork();
        if (worker == 0) {
                int_32 grandchild = (int_32) fork();

                if (grandchild == 0) {
                        while (*control != release)
                                __asm__ volatile("pause");
                        fill_rect(framebuffer, info, 128, 64, 160, 96,
                                  yellow);
                        process_exit(52);
                }
                process_exit(grandchild > 0 ? 51 : 53);
        }

        if (!wait_status(worker, 51))
                return false;
        *control = release;
        int_32 status = 0;
        int_32 reparented = wait(&status);

        return reparented > 0 && status == 52 &&
               *pixel(framebuffer, info, 159, 95) == yellow;
}

static bool test_post_unmap_fault(volatile uint_8 *framebuffer,
                                  const struct frog_fb_info *info,
                                  uint_32 length)
{
        volatile uint_32 *shared_pixel = pixel(framebuffer, info, 64, 64);
        uint_32 expected = *shared_pixel;
        int_32 child = (int_32) fork();

        if (child == 0) {
                if (munmap((void *) framebuffer, length) != 0)
                        process_exit(61);
                *shared_pixel = 0;
                process_exit(62);
        }
        return wait_status(child, -EFAULT) && *shared_pixel == expected;
}

void _start(void) __attribute__((section(".text._start"), noreturn));
void _start(void)
{
        struct frog_fb_info info;
        int_32 fd = open(framebuffer_path, O_RDWR);
        bool opened = fd >= 0;

        report(FROG_TEST_VM_MMAP_FD, opened);
        if (!opened)
                finish_failure();

        bool ioctl_pointer =
            ioctl(fd, FROG_FB_IOCTL_GET_INFO, 0) == -EFAULT &&
            ioctl(fd, FROG_FB_IOCTL_GET_INFO, (void *) 0xbffffff0U) == -EFAULT;
        report(FROG_TEST_VM_MMAP_POINTER, ioctl_pointer);

        bool ioctl_ok = ioctl(fd, 0xffffffffU, &info) == -ENOTTY &&
                        ioctl(fd, FROG_FB_IOCTL_GET_INFO, &info) == 0 &&
                        framebuffer_info_valid(&info);
        report(FROG_TEST_VM_USER_READ, ioctl_ok);
        if (!ioctl_ok)
                finish_failure();

        report(FROG_TEST_VM_MMAP_ARGUMENTS,
               test_mmap_arguments(fd, info.map_length));
        report(FROG_TEST_VM_MMAP_ACCESS,
               test_read_only_mapping(info.map_length));

        struct frog_mmap_args args = {
            .addr = 0,
            .length = info.map_length,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_SHARED,
            .fd = fd,
            .offset = 0,
        };
        volatile uint_8 *framebuffer = mmap(
            (void *) 0, info.map_length, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
        bool address_ok = framebuffer == (volatile uint_8 *) FB_ADDRESS;
        report(FROG_TEST_VM_USER_ADDRESS, address_ok);
        if (!address_ok)
                finish_failure();

        report(FROG_TEST_VM_MMAP_BUSY, mmap_raw(&args) == -EBUSY);
        draw_expected_base(framebuffer, &info);
        report(FROG_TEST_VM_USER_WRITE,
               *pixel(framebuffer, &info, 0, 0) == rgb(&info, 255, 0, 0) &&
               *pixel(framebuffer, &info, 512, 0) == rgb(&info, 0, 255, 0) &&
               *pixel(framebuffer, &info, 1023, 0) == rgb(&info, 0, 0, 255));

        bool closed = close(fd) == 0 &&
                      ioctl(fd, FROG_FB_IOCTL_GET_INFO, &info) == -EBADF;
        uint_32 white = rgb(&info, 255, 255, 255);
        fill_rect(framebuffer, &info, 480, 352, 544, 416, white);
        closed = *pixel(framebuffer, &info, 543, 415) == white && closed;
        report(FROG_TEST_VM_FILE_AFTER_CLOSE, closed);

        bool child_first = test_child_first(framebuffer, &info);
        report(FROG_TEST_VM_FORK_SHARED, child_first);
        report(FROG_TEST_VM_EXIT_CHILD_FIRST, child_first);
        report(FROG_TEST_VM_EXIT_PARENT_FIRST,
               test_parent_first(framebuffer, &info));
        report(FROG_TEST_VM_POST_UNMAP_FAULT,
               test_post_unmap_fault(framebuffer, &info, info.map_length));

        bool exact_only =
            raw_syscall2(SYS_MUNMAP, (uint_32) framebuffer, PAGE_SIZE) ==
                -EINVAL &&
            raw_syscall2(SYS_MUNMAP, (uint_32) framebuffer + PAGE_SIZE,
                         info.map_length - PAGE_SIZE) == -EINVAL &&
            raw_syscall2(SYS_MUNMAP, (uint_32) framebuffer + PAGE_SIZE,
                         PAGE_SIZE) == -EINVAL;
        report(FROG_TEST_VM_MUNMAP_EXACT, exact_only);

        bool unmapped = munmap((void *) framebuffer, info.map_length) == 0;
        report(FROG_TEST_VM_CLEANUP, unmapped);
        if (!all_passed)
                finish_failure();

        raw_syscall0(SYS_TEST_SYNC);
        report(FROG_TEST_VM_CLEANUP, false);
        finish_failure();
}
