#include <frog/fcntl.h>
#include <frog/errno.h>
#include <frog/mman.h>
#include <frog/packagefs.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

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
        (void) raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(void)
{
        (void) raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}

void _start(void)
{
        static const char service_path[] = "/dev/pkg/red";
        static const char open_path[] = "/dev/pkg/open";
        static const char missing_path[] = "/dev/pkg/missing";
        static const char max_name_path[] =
            "/dev/pkg/1234567890123456789012345678901";
        static const char long_name_path[] =
            "/dev/pkg/12345678901234567890123456789012";
        static const char traversal_path[] = "/dev/pkg/../escape";
        static const char exec_path[] = "/test/exec-target";
        static const char exec_arg0[] = "cloexec-target";
        uint_8 outgoing_storage[FROG_PKG_RECORD_MAX];
        struct frog_pkg_record *outgoing =
            (struct frog_pkg_record *) outgoing_storage;
        uint_8 incoming_storage[FROG_PKG_RECORD_MAX];
        struct frog_pkg_record *incoming =
            (struct frog_pkg_record *) incoming_storage;
        int_32 server;
        int_32 client1;
        int_32 client2;
        int_32 temporary;
        int_32 child;
        int_32 status;
        uint_32 peer1 = 0;
        uint_32 peer2 = 0;
        struct frog_mmap_args mmap_args;
        int_32 limit_fds[FROG_PKG_CLIENT_MAX];
        char limit_path[] = "/dev/pkg/a";
        const char *exec_argv[2] = {exec_arg0, NULL};
        bool passed;

        server = raw_syscall2(
            SYS_OPEN, (uint_32) service_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client1 = raw_syscall2(SYS_OPEN, (uint_32) service_path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client2 = raw_syscall2(SYS_OPEN, (uint_32) service_path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = server >= 0 && client1 >= 0 && client2 >= 0;
        report(FROG_TEST_PACKAGEFS_BIND_CONNECT, passed);

        outgoing->peer_id = 0;
        outgoing->event = FROG_PKG_DATA;
        outgoing->payload_size = 1;
        outgoing->payload[0] = 'A';
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE + 1U) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == 1 && incoming->payload[0] == 'A';
        if (passed)
                peer1 = incoming->peer_id;

        outgoing->payload[0] = 'B';
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client2, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE + 1U) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == 1 && incoming->payload[0] == 'B';
        if (passed)
                peer2 = incoming->peer_id;

        outgoing->peer_id = peer2;
        outgoing->payload[0] = 'Y';
        passed = passed && peer1 != 0 && peer2 != 0 && peer1 != peer2 &&
                 raw_syscall3(SYS_WRITE, server, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE + 1U) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 raw_syscall3(SYS_READ, client1, (uint_32) incoming,
                              sizeof(incoming_storage)) == -EAGAIN &&
                 raw_syscall3(SYS_READ, client2, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->peer_id == 0 &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == 1 && incoming->payload[0] == 'Y';
        report(FROG_TEST_PACKAGEFS_DIRECTED_ROUTING, passed);

        outgoing->peer_id = 0;
        outgoing->event = FROG_PKG_DATA;
        outgoing->payload_size = 0;
        passed = raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) ==
                     FROG_PKG_HEADER_SIZE &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE &&
                 incoming->peer_id == peer1 &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == 0;

        outgoing->payload_size = FROG_PKG_PAYLOAD_MAX;
        for (uint_32 index = 0; index < FROG_PKG_PAYLOAD_MAX; index++)
                outgoing->payload[index] = (uint_8) index;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              sizeof(outgoing_storage)) ==
                     sizeof(outgoing_storage) &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     sizeof(incoming_storage) &&
                 incoming->peer_id == peer1 &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == FROG_PKG_PAYLOAD_MAX &&
                 incoming->payload[0] == 0 &&
                 incoming->payload[FROG_PKG_PAYLOAD_MAX - 1U] == 0xffU;

        outgoing->payload_size = FROG_PKG_PAYLOAD_MAX + 1U;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -EMSGSIZE;
        outgoing->payload_size = 1;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -EINVAL;
        outgoing->payload_size = 0;
        outgoing->event = FROG_PKG_DISCONNECT;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -EINVAL;
        outgoing->event = FROG_PKG_DATA;
        outgoing->peer_id = peer1;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -EINVAL;
        outgoing->peer_id = 0;
        passed = passed &&
                 raw_syscall3(SYS_WRITE, server, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -EINVAL;
        report(FROG_TEST_PACKAGEFS_RECORD_BOUNDARIES, passed);

        outgoing->peer_id = 0;
        outgoing->payload_size = 1;
        outgoing->payload[0] = 'S';
        passed = passed &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE + 1U) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              FROG_PKG_HEADER_SIZE) == -EMSGSIZE &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->peer_id == peer1 &&
                 incoming->payload_size == 1 &&
                 incoming->payload[0] == 'S';
        report(FROG_TEST_PACKAGEFS_READ_PRESERVES_RECORD, passed);

        outgoing->peer_id = 0;
        outgoing->event = FROG_PKG_DATA;
        outgoing->payload_size = 1;
        outgoing->payload[0] = 'F';
        passed = raw_syscall2(SYS_OPEN, 0xc0000000U,
                              O_RDWR | O_CLOEXEC) == -EFAULT &&
                 raw_syscall3(SYS_WRITE, client1, 0xc0000000U,
                              FROG_PKG_HEADER_SIZE + 1U) == -EFAULT &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE + 1U) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 raw_syscall3(SYS_READ, server, 0xc0000000U,
                              sizeof(incoming_storage)) == -EFAULT &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->peer_id == peer1 && incoming->payload[0] == 'F' &&
                 raw_syscall3(SYS_SEEK, server, 0, 1) == -ESPIPE;
        mmap_args.addr = 0;
        mmap_args.length = 4096;
        mmap_args.prot = PROT_READ | PROT_WRITE;
        mmap_args.flags = MAP_SHARED;
        mmap_args.fd = server;
        mmap_args.offset = 0;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &mmap_args) ==
                     -EOPNOTSUPP &&
                 passed;
        report(FROG_TEST_PACKAGEFS_INVALID_IO, passed);

        passed = raw_syscall2(
                     SYS_OPEN, (uint_32) open_path,
                     O_CREAT | O_EXCL | O_RDWR) == -EINVAL;
        temporary = raw_syscall2(
            SYS_OPEN, (uint_32) open_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = temporary >= 0 &&
                 raw_syscall2(
                     SYS_OPEN, (uint_32) open_path,
                     O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC) == -EEXIST &&
                 raw_syscall2(SYS_OPEN, (uint_32) open_path,
                              O_RDWR | O_NONBLOCK) == -EINVAL &&
                 passed;
        if (temporary >= 0)
                passed = raw_syscall1(SYS_CLOSE, temporary) == 0 && passed;
        temporary = raw_syscall2(
            SYS_OPEN, (uint_32) max_name_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = temporary >= 0 && passed;
        if (temporary >= 0)
                passed = raw_syscall1(SYS_CLOSE, temporary) == 0 && passed;
        passed = raw_syscall2(
                     SYS_OPEN, (uint_32) long_name_path,
                     O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC) ==
                     -ENAMETOOLONG &&
                 raw_syscall2(
                     SYS_OPEN, (uint_32) traversal_path,
                     O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC) == -EINVAL &&
                 raw_syscall2(SYS_OPEN, (uint_32) missing_path,
                              O_RDWR | O_CLOEXEC) == -ENOENT &&
                 passed;
        report(FROG_TEST_PACKAGEFS_OPEN_CONTRACT, passed);

        outgoing->peer_id = peer2;
        outgoing->event = FROG_PKG_DATA;
        outgoing->payload_size = 0;
        passed = raw_syscall1(SYS_CLOSE, client2) == 0;
        client2 = -1;
        passed = raw_syscall3(SYS_WRITE, server, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -ENOENT &&
                 raw_syscall1(SYS_CLOSE, client1) == 0 && passed;
        client1 = -1;
        outgoing->peer_id = peer1;
        passed = raw_syscall3(SYS_WRITE, server, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -ENOENT &&
                 raw_syscall1(SYS_CLOSE, server) == 0 && passed;
        server = -1;
        passed = raw_syscall2(SYS_OPEN, (uint_32) service_path,
                              O_RDWR | O_CLOEXEC) == -ENOENT && passed;
        server = raw_syscall2(
            SYS_OPEN, (uint_32) service_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client1 = raw_syscall2(SYS_OPEN, (uint_32) service_path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
        outgoing->peer_id = 0;
        outgoing->payload_size = 0;
        passed = server >= 0 && client1 >= 0 &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) ==
                     FROG_PKG_HEADER_SIZE &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE &&
                 incoming->peer_id != 0 && incoming->peer_id != peer1 &&
                 incoming->peer_id != peer2 && passed;
        outgoing->peer_id = peer1;
        passed = raw_syscall3(SYS_WRITE, server, (uint_32) outgoing,
                              FROG_PKG_HEADER_SIZE) == -ENOENT && passed;
        report(FROG_TEST_PACKAGEFS_STALE_GENERATION, passed);

        if (client2 >= 0)
                passed = raw_syscall1(SYS_CLOSE, client2) == 0 && passed;
        if (client1 >= 0)
                passed = raw_syscall1(SYS_CLOSE, client1) == 0 && passed;
        if (server >= 0)
                passed = raw_syscall1(SYS_CLOSE, server) == 0 && passed;

        passed = true;
        for (uint_32 index = 0; index < FROG_PKG_SERVICE_MAX; index++) {
                limit_path[9] = (char) ('a' + index);
                limit_fds[index] = raw_syscall2(
                    SYS_OPEN, (uint_32) limit_path,
                    O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
                passed = limit_fds[index] >= 0 && passed;
        }
        limit_path[9] = 'q';
        passed = raw_syscall2(
                     SYS_OPEN, (uint_32) limit_path,
                     O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK) ==
                     -ENOSPC &&
                 passed;
        for (uint_32 index = 0; index < FROG_PKG_SERVICE_MAX; index++)
                passed = raw_syscall1(SYS_CLOSE, limit_fds[index]) == 0 &&
                         passed;

        server = raw_syscall2(
            SYS_OPEN, (uint_32) open_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = server >= 0 && passed;
        for (uint_32 index = 0; index < FROG_PKG_CLIENT_MAX; index++) {
                limit_fds[index] = raw_syscall2(
                    SYS_OPEN, (uint_32) open_path,
                    O_RDWR | O_CLOEXEC | O_NONBLOCK);
                passed = limit_fds[index] >= 0 && passed;
        }
        passed = raw_syscall2(SYS_OPEN, (uint_32) open_path,
                              O_RDWR | O_CLOEXEC | O_NONBLOCK) == -ENOSPC &&
                 passed;
        for (uint_32 index = 0; index < FROG_PKG_CLIENT_MAX; index++)
                passed = raw_syscall1(SYS_CLOSE, limit_fds[index]) == 0 &&
                         passed;
        passed = raw_syscall1(SYS_CLOSE, server) == 0 && passed;
        report(FROG_TEST_PACKAGEFS_LIMITS, passed);

        server = raw_syscall2(
            SYS_OPEN, (uint_32) open_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC);
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                temporary = raw_syscall2(SYS_OPEN, (uint_32) open_path,
                                         O_RDWR | O_CLOEXEC);
                outgoing->peer_id = 0;
                outgoing->event = FROG_PKG_DATA;
                outgoing->payload_size = 1;
                outgoing->payload[0] = 'W';
                status = temporary >= 0 &&
                         raw_syscall3(SYS_WRITE, temporary,
                                      (uint_32) outgoing,
                                      FROG_PKG_HEADER_SIZE + 1U) ==
                             FROG_PKG_HEADER_SIZE + 1U
                             ? 17
                             : 18;
                if (temporary >= 0)
                        (void) raw_syscall1(SYS_CLOSE, temporary);
                (void) raw_syscall1(SYS_EXIT, status);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = server >= 0 && child > 0 &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 incoming->event == FROG_PKG_DATA &&
                 incoming->payload_size == 1 &&
                 incoming->payload[0] == 'W' &&
                 raw_syscall1(SYS_WAIT, (uint_32) &status) == child &&
                 status == 17;
        passed = raw_syscall1(SYS_CLOSE, server) == 0 && passed;
        report(FROG_TEST_PACKAGEFS_BLOCKING_WAKE, passed);

        server = raw_syscall2(
            SYS_OPEN, (uint_32) open_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client1 = raw_syscall2(SYS_OPEN, (uint_32) open_path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
        outgoing->peer_id = 0;
        outgoing->event = FROG_PKG_DATA;
        outgoing->payload_size = FROG_PKG_PAYLOAD_MAX;
        uint_32 writes = 0;
        while (writes < 16U &&
               raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                            sizeof(outgoing_storage)) ==
                   sizeof(outgoing_storage))
                writes++;
        passed = server >= 0 && client1 >= 0 && writes > 0 && writes < 16U &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              sizeof(outgoing_storage)) == -EAGAIN &&
                 raw_syscall3(SYS_READ, server, (uint_32) incoming,
                              sizeof(incoming_storage)) ==
                     sizeof(incoming_storage) &&
                 raw_syscall3(SYS_WRITE, client1, (uint_32) outgoing,
                              sizeof(outgoing_storage)) ==
                     sizeof(outgoing_storage);
        passed = raw_syscall1(SYS_CLOSE, client1) == 0 &&
                 raw_syscall1(SYS_CLOSE, server) == 0 && passed;
        report(FROG_TEST_PACKAGEFS_BACKPRESSURE, passed);

        server = raw_syscall2(
            SYS_OPEN, (uint_32) open_path,
            O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                (void) raw_syscall2(SYS_EXECV, (uint_32) exec_path,
                                    (uint_32) exec_argv);
                (void) raw_syscall1(SYS_EXIT, 99);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = server >= 0 && child > 0 &&
                 raw_syscall1(SYS_WAIT, (uint_32) &status) == child &&
                 status == FROG_TEST_PACKAGEFS_CLOEXEC_STATUS;
        client1 = raw_syscall2(SYS_OPEN, (uint_32) open_path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = client1 >= 0 &&
                 raw_syscall1(SYS_CLOSE, client1) == 0 &&
                 raw_syscall1(SYS_CLOSE, server) == 0 && passed;
        report(FROG_TEST_PACKAGEFS_CLOEXEC, passed);
        finish();
}
