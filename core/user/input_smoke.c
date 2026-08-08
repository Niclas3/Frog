#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/poll.h>
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

static int_32 call_wait2(struct pollfd *fds, uint_32 count, int_32 timeout_ms)
{
        return raw_syscall3(SYS_WAIT2, (uint_32) fds, count,
                            (uint_32) timeout_ms);
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

static void prepare_wait(struct pollfd *fds, int_32 keyboard_fd,
                         int_32 mouse_fd)
{
        fds[0].fd = keyboard_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0xffff;
        fds[1].fd = mouse_fd;
        fds[1].events = POLLIN;
        fds[1].revents = 0xffff;
}

static bool empty_scan(struct pollfd *fds, int_32 keyboard_fd,
                       int_32 mouse_fd)
{
        prepare_wait(fds, keyboard_fd, mouse_fd);
        return call_wait2(fds, 2, 0) == 0 && fds[0].revents == 0 &&
               fds[1].revents == 0;
}

static bool wait_one(struct pollfd *fds, int_32 keyboard_fd,
                     int_32 mouse_fd, bool keyboard_ready)
{
        prepare_wait(fds, keyboard_fd, mouse_fd);
        return call_wait2(fds, 2, -1) == 1 &&
               fds[0].revents == (keyboard_ready ? POLLIN : 0) &&
               fds[1].revents == (keyboard_ready ? 0 : POLLIN);
}

static bool level_one(struct pollfd *fds, int_32 keyboard_fd,
                      int_32 mouse_fd, bool keyboard_ready)
{
        prepare_wait(fds, keyboard_fd, mouse_fd);
        return call_wait2(fds, 2, 0) == 1 &&
               fds[0].revents == (keyboard_ready ? POLLIN : 0) &&
               fds[1].revents == (keyboard_ready ? 0 : POLLIN);
}

static void close_and_finish(int_32 keyboard_fd, int_32 mouse_fd)
{
        if (keyboard_fd >= 0)
                (void) raw_syscall1(SYS_CLOSE, keyboard_fd);
        if (mouse_fd >= 0)
                (void) raw_syscall1(SYS_CLOSE, mouse_fd);
        finish();
}

void _start(void)
{
        static const char keyboard_path[] = "/dev/input/event0";
        static const char mouse_path[] = "/dev/input/event1";
        struct pollfd fds[2];
        mouse_device_packet_t packet;
        uint_8 key = 0;
        int_32 keyboard_fd;
        int_32 mouse_fd;
        bool wait_passed;
        bool read_passed;
        bool passed;

        passed = check_nonblocking_device(keyboard_path, &key, sizeof(key)) &&
                 check_nonblocking_device(mouse_path, &packet, sizeof(packet));
        report(FROG_TEST_INPUT_NONBLOCK, passed);
        if (!passed)
                finish();

        keyboard_fd = raw_syscall2(SYS_OPEN, (uint_32) keyboard_path,
                                   O_RDONLY);
        mouse_fd = raw_syscall2(SYS_OPEN, (uint_32) mouse_path, O_RDONLY);
        passed = keyboard_fd >= 0 && mouse_fd >= 0 &&
                 empty_scan(fds, keyboard_fd, mouse_fd);
        report(FROG_TEST_INPUT_WAIT2_EMPTY, passed);
        if (!passed)
                close_and_finish(keyboard_fd, mouse_fd);

        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_KEYBOARD_READY) == 0;
        wait_passed = passed &&
                      wait_one(fds, keyboard_fd, mouse_fd, true) &&
                      level_one(fds, keyboard_fd, mouse_fd, true);
        read_passed = wait_passed &&
                      raw_syscall3(SYS_READ, keyboard_fd, (uint_32) &key,
                                   sizeof(key)) == 1 &&
                      key == 'a';
        passed = wait_passed && read_passed &&
                 empty_scan(fds, keyboard_fd, mouse_fd);
        report(FROG_TEST_INPUT_WAIT2_KEYBOARD, passed);
        report(FROG_TEST_INPUT_KEYBOARD, read_passed);
        if (!passed)
                close_and_finish(keyboard_fd, mouse_fd);

        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_MOUSE_MOVE_READY) == 0;
        wait_passed = passed &&
                      wait_one(fds, keyboard_fd, mouse_fd, false) &&
                      level_one(fds, keyboard_fd, mouse_fd, false);
        read_passed = wait_passed &&
                      raw_syscall3(SYS_READ, mouse_fd, (uint_32) &packet,
                                   sizeof(packet)) == sizeof(packet) &&
                      packet.magic == MOUSE_MAGIC &&
                      packet.x_difference == 7 &&
                      packet.y_difference == 0 && packet.buttons == 0;
        passed = wait_passed && read_passed &&
                 empty_scan(fds, keyboard_fd, mouse_fd);
        report(FROG_TEST_INPUT_WAIT2_MOUSE_MOVE, passed);
        report(FROG_TEST_INPUT_MOUSE_MOVE, read_passed);
        if (!passed)
                close_and_finish(keyboard_fd, mouse_fd);

        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_MOUSE_BUTTON_READY) == 0;
        wait_passed = passed &&
                      wait_one(fds, keyboard_fd, mouse_fd, false) &&
                      level_one(fds, keyboard_fd, mouse_fd, false);
        read_passed = wait_passed &&
                      raw_syscall3(SYS_READ, mouse_fd, (uint_32) &packet,
                                   sizeof(packet)) == sizeof(packet) &&
                      packet.magic == MOUSE_MAGIC &&
                      packet.x_difference == 0 &&
                      packet.y_difference == 0 &&
                      packet.buttons == LEFT_CLICK;
        passed = wait_passed && read_passed &&
                 empty_scan(fds, keyboard_fd, mouse_fd);
        report(FROG_TEST_INPUT_WAIT2_MOUSE_BUTTON, passed);
        report(FROG_TEST_INPUT_MOUSE_BUTTON, read_passed);
        if (!passed)
                close_and_finish(keyboard_fd, mouse_fd);

        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_INPUT_BOTH_READY) == 0;
        prepare_wait(fds, keyboard_fd, mouse_fd);
        wait_passed = passed && call_wait2(fds, 2, -1) == 2 &&
                      fds[0].revents == POLLIN &&
                      fds[1].revents == POLLIN;
        read_passed = wait_passed &&
                      raw_syscall3(SYS_READ, keyboard_fd, (uint_32) &key,
                                   sizeof(key)) == 1 &&
                      key == 'a' &&
                      raw_syscall3(SYS_READ, mouse_fd, (uint_32) &packet,
                                   sizeof(packet)) == sizeof(packet) &&
                      packet.magic == MOUSE_MAGIC &&
                      packet.x_difference == 3 &&
                      packet.y_difference == 0 &&
                      packet.buttons == LEFT_CLICK;
        passed = wait_passed && read_passed &&
                 empty_scan(fds, keyboard_fd, mouse_fd);
        report(FROG_TEST_INPUT_WAIT2_BOTH, passed);
        if (!passed)
                close_and_finish(keyboard_fd, mouse_fd);

        wait_passed = raw_syscall1(SYS_CLOSE, keyboard_fd) == 0;
        read_passed = raw_syscall1(SYS_CLOSE, mouse_fd) == 0;
        passed = wait_passed && read_passed;
        report(FROG_TEST_INPUT_WAIT2_CLOSE, passed);
        finish();
}
