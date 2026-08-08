#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

static int_32 call0(uint_32 nr)
{
        int_32 result;
        __asm__ volatile("int $0x93" : "=a"(result) : "a"(nr) : "memory");
        return result;
}

static int_32 call1(uint_32 nr, uint_32 a)
{
        int_32 result;
        __asm__ volatile("int $0x93" : "=a"(result) : "a"(nr), "b"(a) : "memory");
        return result;
}

static int_32 call2(uint_32 nr, uint_32 a, uint_32 b)
{
        int_32 result;
        __asm__ volatile("int $0x93" : "=a"(result) : "a"(nr), "b"(a), "c"(b) : "memory");
        return result;
}

static int_32 call3(uint_32 nr, uint_32 a, uint_32 b, uint_32 c)
{
        int_32 result;
        __asm__ volatile("int $0x93" : "=a"(result) : "a"(nr), "b"(a), "c"(b), "d"(c) : "memory");
        return result;
}

void _start(void)
{
        static const char path[] = "/dev/pkg/lifecycle";
        uint_8 storage[FROG_PKG_HEADER_SIZE + 1U];
        uint_8 large[FROG_PKG_RECORD_MAX];
        struct frog_pkg_record *record = (struct frog_pkg_record *) storage;
        struct frog_pkg_record *big = (struct frog_pkg_record *) large;
        int_32 server;
        int_32 client;
        bool passed;
        int_32 result;
        struct pollfd descriptor;
        uint_32 peer;
        uint_32 peer1;
        int_32 client2;
        int_32 child;
        int_32 status;

        passed = call1(SYS_TESTSYSCALL,
                       FROG_TEST_PACKAGEFS_LIFECYCLE_SNAPSHOT) == 0;
        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);

        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 1;
        record->payload[0] = 'D';
        passed = server >= 0 && client >= 0 && passed;
        passed = passed &&
                 call3(SYS_WRITE, client, (uint_32) record, sizeof(storage)) ==
                     (int_32) sizeof(storage) && call1(SYS_CLOSE, client) == 0;
        result = call3(SYS_READ, server, (uint_32) record, sizeof(storage));
        passed = passed && result == (int_32) sizeof(storage) &&
                 record->event == FROG_PKG_DATA && record->payload[0] == 'D';
        result = call3(SYS_READ, server, (uint_32) record,
                       FROG_PKG_HEADER_SIZE);
        passed = passed && result == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT &&
                 record->payload_size == 0;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_DATA_DISCONNECT_ORDER, passed);

        passed = call3(SYS_READ, server, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == -EAGAIN;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_DISCONNECT_EXACTLY_ONCE, passed);

        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = client >= 0 && call1(SYS_CLOSE, client) == 0 &&
                 call3(SYS_READ, server, (uint_32) record,
                       FROG_PKG_HEADER_SIZE - 1U) == -EMSGSIZE &&
                 call3(SYS_READ, server, 0,
                       FROG_PKG_HEADER_SIZE) == -EFAULT &&
                 call3(SYS_READ, server, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT &&
                 record->payload_size == 0;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_BAD_CONTROL_READ_PRESERVES, passed);

        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client2 = call2(SYS_OPEN, (uint_32) path,
                        O_RDWR | O_CLOEXEC | O_NONBLOCK);
        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 0;
        passed = client >= 0 && client2 >= 0 &&
                 call1(SYS_CLOSE, client) == 0 &&
                 call3(SYS_WRITE, client2, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DATA &&
                 call1(SYS_CLOSE, client2) == 0 &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_CONTROL_NOT_STARVED, passed);

        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 0;
        passed = client >= 0 &&
                 call3(SYS_WRITE, client, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE;
        peer = record->peer_id;
        record->peer_id = peer;
        record->payload_size = 1;
        record->payload[0] = 'S';
        passed = passed &&
                 call3(SYS_WRITE, server, (uint_32) record,
                       sizeof(storage)) == (int_32) sizeof(storage) &&
                 call1(SYS_CLOSE, server) == 0;
        server = -1;
        descriptor.fd = client;
        descriptor.events = POLLIN | POLLOUT;
        descriptor.revents = 0;
        passed = passed &&
                 call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == (POLLIN | POLLHUP) &&
                 call3(SYS_READ, client, (uint_32) record,
                       sizeof(storage)) == (int_32) sizeof(storage) &&
                 record->event == FROG_PKG_DATA &&
                 record->payload[0] == 'S';
        descriptor.revents = 0;
        passed = passed &&
                 call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLHUP &&
                 call3(SYS_READ, client, (uint_32) record,
                       sizeof(storage)) == 0 &&
                 call3(SYS_WRITE, client, (uint_32) record,
                       sizeof(storage)) == -EPIPE &&
                 call1(SYS_CLOSE, client) == 0;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_SERVER_CLOSE_DRAIN, passed);

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client2 = call2(SYS_OPEN, (uint_32) path,
                        O_RDWR | O_CLOEXEC | O_NONBLOCK);
        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 0;
        passed = server >= 0 && client >= 0 && client2 >= 0 &&
                 call3(SYS_WRITE, client, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE;
        peer1 = record->peer_id;
        record->peer_id = 0;
        passed = passed &&
                 call3(SYS_WRITE, client2, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE;

        big->peer_id = peer1;
        big->event = FROG_PKG_DATA;
        big->payload_size = 100;
        passed = passed &&
                 call3(SYS_WRITE, server, (uint_32) big,
                       FROG_PKG_HEADER_SIZE + 100U) ==
                     FROG_PKG_HEADER_SIZE + 100U;
        big->payload_size = FROG_PKG_PAYLOAD_MAX;
        for (uint_32 index = 0; index < 3U; index++)
                passed = call3(SYS_WRITE, server, (uint_32) big,
                               FROG_PKG_RECORD_MAX) == FROG_PKG_RECORD_MAX &&
                         passed;
        passed = call3(SYS_WRITE, server, (uint_32) big,
                       FROG_PKG_RECORD_MAX) == -EAGAIN && passed;
        big->payload_size = 900;
        passed = call3(SYS_WRITE, server, (uint_32) big,
                       FROG_PKG_HEADER_SIZE + 900U) == -EAGAIN && passed;

        passed = call3(SYS_READ, client, (uint_32) large,
                       sizeof(large)) == FROG_PKG_HEADER_SIZE + 100U && passed;
        descriptor.fd = server;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        passed = call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 0 &&
                 descriptor.revents == 0 && passed;
        passed = call3(SYS_READ, client, (uint_32) large,
                       sizeof(large)) == FROG_PKG_RECORD_MAX && passed;
        descriptor.revents = 0;
        passed = call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLIN && passed;
        passed = call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->peer_id == peer1 &&
                 record->event == FROG_PKG_WRITABLE &&
                 record->payload_size == 0 &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == -EAGAIN &&
                 call3(SYS_READ, client2, (uint_32) record,
                       sizeof(storage)) == -EAGAIN && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_WRITABLE_ARM_COALESCE, passed);
        (void) call1(SYS_CLOSE, client);
        (void) call1(SYS_CLOSE, client2);
        (void) call1(SYS_CLOSE, server);
        server = -1;

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        child = call0(SYS_FORK);
        if (child == 0) {
                (void) call1(SYS_CLOSE, client);
                (void) call1(SYS_EXIT, 21);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = server >= 0 && client >= 0 && child > 0 &&
                 call1(SYS_WAIT, (uint_32) &status) == child && status == 21 &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == -EAGAIN &&
                 call1(SYS_CLOSE, client) == 0 &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == -EAGAIN;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_FORK_FINAL_REF, passed);
        (void) call1(SYS_CLOSE, server);

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        child = call0(SYS_FORK);
        if (child == 0) {
                client = call2(SYS_OPEN, (uint_32) path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
                (void) call1(SYS_EXIT, client >= 0 ? 22 : 23);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = server >= 0 && child > 0 &&
                 call1(SYS_WAIT, (uint_32) &status) == child && status == 22 &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == -EAGAIN;
        (void) call2(SYS_TEST_REPORT, FROG_TEST_PACKAGEFS_PROCESS_EXIT,
                     passed);
        (void) call1(SYS_CLOSE, server);
        server = -1;

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = server >= 0;
        for (uint_32 index = 0; index < FROG_PKG_CLIENT_MAX; index++) {
                client = call2(SYS_OPEN, (uint_32) path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
                passed = client >= 0 && call1(SYS_CLOSE, client) == 0 &&
                         passed;
        }
        passed = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK) == -ENOSPC && passed;
        descriptor.fd = server;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        passed = call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLIN &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT && passed;
        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        passed = client >= 0 && call1(SYS_CLOSE, client) == 0 && passed;
        for (uint_32 index = 0; index < FROG_PKG_CLIENT_MAX; index++)
                passed = call3(SYS_READ, server, (uint_32) record,
                               sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                         record->event == FROG_PKG_DISCONNECT && passed;
        passed = call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == -EAGAIN && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_TOMBSTONE_LIMIT, passed);

        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        descriptor.fd = client;
        descriptor.events = POLLIN | POLLOUT;
        descriptor.revents = 0;
        passed = client >= 0 &&
                 call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLOUT;
        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 0;
        passed = call3(SYS_WRITE, client, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE && passed;
        descriptor.fd = server;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        passed = call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLIN &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DATA &&
                 call1(SYS_CLOSE, client) == 0 && passed;
        descriptor.revents = 0;
        passed = call3(SYS_WAIT2, (uint_32) &descriptor, 1, 0) == 1 &&
                 descriptor.revents == POLLIN &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE &&
                 record->event == FROG_PKG_DISCONNECT && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_LIFECYCLE_WAIT2_MASKS, passed);
        (void) call1(SYS_CLOSE, server);
        server = -1;

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
        client = call2(SYS_OPEN, (uint_32) path, O_RDWR | O_CLOEXEC);
        child = call0(SYS_FORK);
        if (child == 0) {
                (void) call1(SYS_CLOSE, server);
                result = call3(SYS_READ, client, (uint_32) record,
                               sizeof(storage));
                (void) call1(SYS_EXIT, result == 0 ? 25 : 26);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = server >= 0 && client >= 0 && child > 0 &&
                 call3(SYS_WAIT2, 0, 0, 10) == 0 &&
                 call1(SYS_CLOSE, server) == 0 &&
                 call1(SYS_WAIT, (uint_32) &status) == child && status == 25 &&
                 call1(SYS_CLOSE, client) == 0;

        server = call2(SYS_OPEN, (uint_32) path,
                       O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC);
        client = call2(SYS_OPEN, (uint_32) path,
                       O_RDWR | O_CLOEXEC | O_NONBLOCK);
        record->peer_id = 0;
        record->event = FROG_PKG_DATA;
        record->payload_size = 0;
        passed = server >= 0 && client >= 0 &&
                 call3(SYS_WRITE, client, (uint_32) record,
                       FROG_PKG_HEADER_SIZE) == FROG_PKG_HEADER_SIZE &&
                 call3(SYS_READ, server, (uint_32) record,
                       sizeof(storage)) == FROG_PKG_HEADER_SIZE && passed;
        big->peer_id = record->peer_id;
        big->event = FROG_PKG_DATA;
        big->payload_size = FROG_PKG_PAYLOAD_MAX;
        for (uint_32 index = 0; index < 3U; index++)
                passed = call3(SYS_WRITE, server, (uint_32) big,
                               FROG_PKG_RECORD_MAX) == FROG_PKG_RECORD_MAX &&
                         passed;
        child = call0(SYS_FORK);
        if (child == 0) {
                (void) call1(SYS_CLOSE, client);
                result = call3(SYS_WRITE, server, (uint_32) big,
                               FROG_PKG_RECORD_MAX);
                (void) call1(SYS_EXIT, result == -ENOENT ? 27 : 28);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = child > 0 && call3(SYS_WAIT2, 0, 0, 10) == 0 &&
                 call1(SYS_CLOSE, client) == 0 &&
                 call1(SYS_WAIT, (uint_32) &status) == child && status == 27 &&
                 call1(SYS_CLOSE, server) == 0 && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_BLOCKED_CLOSE_RACES, passed);

        passed = true;
        for (uint_32 index = 0; index < 16U; index++) {
                server = call2(
                    SYS_OPEN, (uint_32) path,
                    O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK);
                client = call2(SYS_OPEN, (uint_32) path,
                               O_RDWR | O_CLOEXEC | O_NONBLOCK);
                passed = server >= 0 && client >= 0 &&
                         call1(SYS_CLOSE, server) == 0 &&
                         call1(SYS_CLOSE, client) == 0 && passed;
        }
        passed = call1(SYS_TESTSYSCALL,
                       FROG_TEST_PACKAGEFS_LIFECYCLE_VERIFY) == 0 && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_REBIND_LEAK_BASELINE, passed);
        if (server >= 0)
                (void) call1(SYS_CLOSE, server);
        (void) call1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}
