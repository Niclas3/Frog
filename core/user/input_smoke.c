#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>
#include <input/mouse.h>

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

static int_32 raw_syscall3(uint_32 number, uint_32 arg1, uint_32 arg2,
                           uint_32 arg3)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                         : "memory");
        return result;
}

static void report(uint_32 id, bool passed)
{
        raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(void)
{
        raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}

static bool check_nonblocking_device(const char *path, void *buffer,
                                     uint_32 size)
{
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) path,
                                 O_RDONLY | O_NONBLOCK);

        if (fd < 0)
                return false;
        return raw_syscall3(SYS_READ, fd, (uint_32) buffer, size) == -EAGAIN &&
               raw_syscall1(SYS_CLOSE, fd) == 0;
}

void _start(void)
{
        static const char keyboard_path[] = "/dev/input/event0";
        static const char mouse_path[] = "/dev/input/event1";
        mouse_device_packet_t packet;
        uint_8 key = 0;
        int_32 fd;
        bool passed;

        passed = check_nonblocking_device(keyboard_path, &key, sizeof(key)) &&
                 check_nonblocking_device(mouse_path, &packet, sizeof(packet));
        report(FROG_TEST_INPUT_NONBLOCK, passed);
        if (!passed) {
                finish();
        }

        fd = raw_syscall2(SYS_OPEN, (uint_32) keyboard_path, O_RDONLY);
        passed = fd >= 0 &&
                 raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_KEYBOARD_READY) == 0 &&
                 raw_syscall3(SYS_READ, fd, (uint_32) &key, sizeof(key)) == 1 &&
                 key == 'a' && raw_syscall1(SYS_CLOSE, fd) == 0;
        report(FROG_TEST_INPUT_KEYBOARD, passed);
        if (!passed) {
                if (fd >= 0)
                        raw_syscall1(SYS_CLOSE, fd);
                finish();
        }

        fd = raw_syscall2(SYS_OPEN, (uint_32) mouse_path, O_RDONLY);
        passed = fd >= 0 &&
                 raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_MOUSE_MOVE_READY) == 0 &&
                 raw_syscall3(SYS_READ, fd, (uint_32) &packet,
                              sizeof(packet)) == sizeof(packet) &&
                 packet.magic == MOUSE_MAGIC && packet.x_difference == 7 &&
                 packet.y_difference == 0 && packet.buttons == 0;
        report(FROG_TEST_INPUT_MOUSE_MOVE, passed);
        if (!passed) {
                if (fd >= 0)
                        raw_syscall1(SYS_CLOSE, fd);
                finish();
        }

        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_MOUSE_BUTTON_READY) == 0 &&
                 raw_syscall3(SYS_READ, fd, (uint_32) &packet,
                              sizeof(packet)) == sizeof(packet) &&
                 packet.magic == MOUSE_MAGIC && packet.x_difference == 0 &&
                 packet.y_difference == 0 &&
                 packet.buttons == LEFT_CLICK &&
                 raw_syscall1(SYS_CLOSE, fd) == 0;
        report(FROG_TEST_INPUT_MOUSE_BUTTON, passed);
        finish();
}
