#include <frog/packagefs.h>
#include <frog/errno.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

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

void _start(void)
{
        int_32 server = -1;
        int_32 client = -1;
        uint_8 payload[FROG_PKG_PAYLOAD_MAX];
        struct frog_pkg_message message;
        frog_pkg_peer_id peer_id = 0;
        bool passed;
        int_32 result;
        uint_32 writes;
        int_32 client2;
        int_32 client3;
        frog_pkg_peer_id peers[3];
        struct frog_pkg_delivery_result delivery[3];
        static const char long_name[] =
            "12345678901234567890123456789012";
        server = frog_pkg_bind("userlib", true);
        client = frog_pkg_connect("userlib", true);
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_USERLIB_BIND_CONNECT,
                     server >= 0 && client >= 0);
        passed = frog_pkg_client_send(client, NULL, 0) ==
                     FROG_PKG_HEADER_SIZE &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE &&
                 message.peer_id != 0 && message.event == FROG_PKG_DATA &&
                 message.payload_size == 0;
        if (passed)
                peer_id = message.peer_id;
        payload[0] = 'A';
        passed = passed &&
                 frog_pkg_client_send(client, payload, 1) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 message.peer_id == peer_id &&
                 message.event == FROG_PKG_DATA &&
                 message.payload_size == 1 && message.payload[0] == 'A';
        for (uint_32 index = 0; index < FROG_PKG_PAYLOAD_MAX; index++)
                payload[index] = (uint_8) index;
        passed = passed &&
                 frog_pkg_client_send(client, payload,
                                      FROG_PKG_PAYLOAD_MAX) ==
                     FROG_PKG_RECORD_MAX &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_RECORD_MAX &&
                 message.peer_id == peer_id &&
                 message.event == FROG_PKG_DATA &&
                 message.payload_size == FROG_PKG_PAYLOAD_MAX &&
                 message.payload[0] == 0 &&
                 message.payload[FROG_PKG_PAYLOAD_MAX - 1U] == 0xffU;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_USERLIB_EXACT_RECORDS, passed);

        payload[0] = 'R';
        passed = frog_pkg_server_send(server, peer_id, payload, 1) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 frog_pkg_client_receive(client, &message) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 message.peer_id == 0 && message.event == FROG_PKG_DATA &&
                 message.payload_size == 1 && message.payload[0] == 'R';
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_USERLIB_DIRECTED_REPLY, passed);

        passed = frog_pkg_client_send(client, NULL, 1) == -EINVAL &&
                 frog_pkg_client_send(client, payload,
                                      FROG_PKG_PAYLOAD_MAX + 1U) ==
                     -EMSGSIZE &&
                 frog_pkg_server_send(server, 0, payload, 1) == -EINVAL &&
                 frog_pkg_client_send(-1, NULL, 0) == -EBADF &&
                 frog_pkg_server_receive(server, NULL) == -EINVAL &&
                 frog_pkg_bind("", true) == -EINVAL &&
                 frog_pkg_bind("bad/name", true) == -EINVAL &&
                 frog_pkg_bind(long_name, true) == -ENAMETOOLONG &&
                 frog_pkg_connect("missing-userlib", true) == -ENOENT;
        writes = 0;
        do {
                result = frog_pkg_server_send(server, peer_id, payload,
                                              FROG_PKG_PAYLOAD_MAX);
                if (result == FROG_PKG_RECORD_MAX)
                        writes++;
        } while (result == FROG_PKG_RECORD_MAX && writes < 16U);
        passed = result == -EAGAIN && writes > 0 && writes < 16U && passed;
        passed = frog_pkg_client_receive(client, &message) ==
                     FROG_PKG_RECORD_MAX &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE &&
                 message.peer_id == peer_id &&
                 message.event == FROG_PKG_WRITABLE &&
                 message.payload_size == 0 && passed;
        while (frog_pkg_client_receive(client, &message) > 0)
                ;
        passed = call1(SYS_CLOSE, client) == 0 &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE &&
                 message.peer_id == peer_id &&
                 message.event == FROG_PKG_DISCONNECT &&
                 message.payload_size == 0 &&
                 frog_pkg_server_send(server, peer_id, payload, 1) ==
                     -ENOENT && passed;
        client = -1;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_USERLIB_CONTROL_ERRNO, passed);

        client = frog_pkg_connect("userlib", true);
        client2 = frog_pkg_connect("userlib", true);
        client3 = frog_pkg_connect("userlib", true);
        passed = client >= 0 && client2 >= 0 && client3 >= 0;
        passed = frog_pkg_client_send(client, NULL, 0) ==
                     FROG_PKG_HEADER_SIZE &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE && passed;
        peers[0] = message.peer_id;
        passed = frog_pkg_client_send(client2, NULL, 0) ==
                     FROG_PKG_HEADER_SIZE &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE && passed;
        peers[1] = message.peer_id;
        passed = frog_pkg_client_send(client3, NULL, 0) ==
                     FROG_PKG_HEADER_SIZE &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE && passed;
        peers[2] = message.peer_id;
        writes = 0;
        do {
                result = frog_pkg_server_send(server, peers[0], payload,
                                              FROG_PKG_PAYLOAD_MAX);
                if (result == FROG_PKG_RECORD_MAX)
                        writes++;
        } while (result == FROG_PKG_RECORD_MAX && writes < 16U);
        passed = result == -EAGAIN && passed;
        writes = 0;
        do {
                result = frog_pkg_server_send(server, peers[0], NULL, 0);
                if (result == FROG_PKG_HEADER_SIZE)
                        writes++;
        } while (result == FROG_PKG_HEADER_SIZE &&
                 writes < FROG_PKG_RECORD_MAX);
        passed = result == -EAGAIN && writes < FROG_PKG_RECORD_MAX &&
                 call1(SYS_CLOSE, client2) == 0 &&
                 frog_pkg_server_receive(server, &message) ==
                     FROG_PKG_HEADER_SIZE &&
                 message.peer_id == peers[1] &&
                 message.event == FROG_PKG_DISCONNECT && passed;
        client2 = -1;
        payload[0] = 'B';
        passed = frog_pkg_server_broadcast(server, peers, 3, payload, 1,
                                           delivery) == 0 &&
                 delivery[0].peer_id == peers[0] &&
                 delivery[0].raw_status == -EAGAIN &&
                 delivery[0].classification == FROG_PKG_WOULD_BLOCK &&
                 delivery[1].peer_id == peers[1] &&
                 delivery[1].raw_status == -ENOENT &&
                 delivery[1].classification == FROG_PKG_DISCONNECTED &&
                 delivery[2].peer_id == peers[2] &&
                 delivery[2].raw_status == FROG_PKG_HEADER_SIZE + 1U &&
                 delivery[2].classification == FROG_PKG_DELIVERED &&
                 frog_pkg_client_receive(client3, &message) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 message.payload[0] == 'B' && passed;
        peers[0] = 0;
        peers[1] = peers[2];
        payload[0] = 'C';
        passed = frog_pkg_server_broadcast(server, peers, 2, payload, 1,
                                           delivery) == 0 &&
                 delivery[0].peer_id == 0 &&
                 delivery[0].raw_status == -EINVAL &&
                 delivery[0].classification == FROG_PKG_DELIVERY_ERROR &&
                 delivery[1].peer_id == peers[1] &&
                 delivery[1].raw_status == FROG_PKG_HEADER_SIZE + 1U &&
                 delivery[1].classification == FROG_PKG_DELIVERED &&
                 frog_pkg_client_receive(client3, &message) ==
                     FROG_PKG_HEADER_SIZE + 1U &&
                 message.payload[0] == 'C' &&
                 frog_pkg_server_broadcast(server, NULL, 1, payload, 1,
                                           delivery) == -EINVAL &&
                 frog_pkg_server_broadcast(server, peers,
                                           FROG_PKG_CLIENT_MAX + 1U,
                                           payload, 1, delivery) == -EINVAL &&
                 frog_pkg_server_broadcast(server, peers, 1, NULL, 1,
                                           delivery) == -EINVAL &&
                 frog_pkg_server_broadcast(
                     server, peers, 1, payload, FROG_PKG_PAYLOAD_MAX + 1U,
                     delivery) == -EMSGSIZE && passed;
        (void) call2(SYS_TEST_REPORT,
                     FROG_TEST_PACKAGEFS_USERLIB_BROADCAST_RESULTS, passed);
        if (client >= 0)
                (void) call1(SYS_CLOSE, client);
        if (client2 >= 0)
                (void) call1(SYS_CLOSE, client2);
        if (client3 >= 0)
                (void) call1(SYS_CLOSE, client3);
        if (server >= 0)
                (void) call1(SYS_CLOSE, server);
        (void) call1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}
