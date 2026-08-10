#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>
#include <kernel/frogfs.h>
#include <kernel/qemu_test.h>
#include <kernel/root_switch.h>
#include <kernel/vfs.h>

#define ROOT_SWITCH_DEVICE       "/dev/sdbp1"
#define ROOT_SWITCH_SERVICE      "/dev/pkg/root-switch"
#define ROOT_SWITCH_USER_INPUT   USER_IMAGE_VADDR
#define ROOT_SWITCH_USER_OUTPUT  (USER_IMAGE_VADDR + 2048U)

struct root_switch_fixture {
        struct vfs_root_switch_test_snapshot mounts;
        struct dentry *old_root;
        struct dentry *bootstrap_only;
        struct dentry *staged_root;
        struct dentry *dev_root;
        struct dentry *package_root;
        struct dentry *device_dentry;
        struct dentry *service_dentry;
        struct inode *device_inode;
        struct file *device;
        struct file *server;
        struct file *client;
        void *server_state;
        void *client_state;
        struct mm_struct *test_mm;
};

static int close_file(struct file **file)
{
        int result;

        if (file == NULL || *file == NULL)
                return 0;
        result = vfs_close(*file);
        *file = NULL;
        return result;
}

static struct frog_pkg_record *test_record(uint_32 address)
{
        return (struct frog_pkg_record *) address;
}

static uint_32 make_record(uint_32 address, uint_32 peer_id,
                           const char payload[3])
{
        struct frog_pkg_record *record = test_record(address);

        memset(record, 0, FROG_PKG_HEADER_SIZE + 3U);
        record->peer_id = peer_id;
        record->event = FROG_PKG_DATA;
        record->payload_size = 3U;
        memcpy(record->payload, payload, 3U);
        return FROG_PKG_HEADER_SIZE + 3U;
}

static int record_matches(uint_32 address, uint_32 peer_id,
                          const char payload[3])
{
        struct frog_pkg_record *record = test_record(address);

        return record->peer_id == peer_id &&
               record->event == FROG_PKG_DATA &&
               record->payload_size == 3U &&
               memcmp(record->payload, payload, 3U) == 0;
}

static int install_test_user_page(struct root_switch_fixture *fixture)
{
        TCB_t *current = running_thread();

        if (current == NULL || current->mm != NULL)
                return 0;
        fixture->test_mm = process_create_user_mm();
        if (fixture->test_mm == NULL ||
            vm_user_map_owned_page(fixture->test_mm,
                                   ROOT_SWITCH_USER_INPUT) != 0)
                return 0;
        current->mm = fixture->test_mm;
        page_dir_activate(current);
        return 1;
}

static int package_connection_ready(struct root_switch_fixture *fixture,
                                    uint_32 server_mask)
{
        return fixture->server != NULL && fixture->client != NULL &&
               fixture->server->private_data == fixture->server_state &&
               fixture->client->private_data == fixture->client_state &&
               fixture->server->f_dentry == fixture->service_dentry &&
               fixture->client->f_dentry == fixture->service_dentry &&
               (vfs_poll(fixture->server, NULL) & server_mask) ==
                   server_mask &&
               (vfs_poll(fixture->client, NULL) & POLLOUT) != 0;
}

static int old_namespace_usable(struct root_switch_fixture *fixture)
{
        return vfs_lookup("/") == fixture->old_root &&
               vfs_lookup("/bootstrap-only") == fixture->bootstrap_only &&
               vfs_lookup("/sysroot") == fixture->staged_root &&
               vfs_lookup("/dev") == fixture->dev_root &&
               vfs_lookup("/dev/pkg") == fixture->package_root &&
               vfs_lookup("/dev/input/event0") == fixture->device_dentry &&
               vfs_lookup(ROOT_SWITCH_SERVICE) == fixture->service_dentry &&
               vfs_root_switch_test_snapshot_unchanged(&fixture->mounts) &&
               package_connection_ready(fixture, POLLIN);
}

static int setup_fixture(struct root_switch_fixture *fixture)
{
        char device_byte = 0;
        uint_32 record_size;

        memset(fixture, 0, sizeof(*fixture));
        fixture->old_root = vfs_lookup("/");
        if (fixture->old_root == NULL ||
            vfs_mkdir_path("/bootstrap-only") != 0 ||
            vfs_mkdir_path("/sysroot") != 0 || frogfs_init() != 0 ||
            vfs_mount("/sysroot", "frogfs", 0,
                      ROOT_SWITCH_DEVICE, NULL) != 0)
                return 0;

        fixture->bootstrap_only = vfs_lookup("/bootstrap-only");
        fixture->staged_root = vfs_lookup("/sysroot");
        fixture->dev_root = vfs_lookup("/dev");
        fixture->package_root = vfs_lookup("/dev/pkg");
        fixture->device_dentry = vfs_lookup("/dev/input/event0");
        if (fixture->bootstrap_only == NULL || fixture->staged_root == NULL ||
            fixture->dev_root == NULL || fixture->package_root == NULL ||
            fixture->device_dentry == NULL ||
            vfs_lookup("/sysroot/dev") == NULL ||
            !vfs_root_switch_test_snapshot(&fixture->mounts) ||
            !install_test_user_page(fixture))
                return 0;

        fixture->device_inode = fixture->device_dentry->d_inode;
        if (fixture->device_inode == NULL ||
            vfs_open_file("/dev/input/event0", O_RDONLY | O_NONBLOCK,
                          &fixture->device) != 0 ||
            vfs_read(fixture->device, &device_byte, 1) != -EAGAIN ||
            vfs_open_file(ROOT_SWITCH_SERVICE,
                          O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC |
                              O_NONBLOCK,
                          &fixture->server) != 0 ||
            vfs_open_file(ROOT_SWITCH_SERVICE,
                          O_RDWR | O_CLOEXEC | O_NONBLOCK,
                          &fixture->client) != 0)
                return 0;

        fixture->service_dentry = vfs_lookup(ROOT_SWITCH_SERVICE);
        fixture->server_state = fixture->server->private_data;
        fixture->client_state = fixture->client->private_data;
        record_size = make_record(ROOT_SWITCH_USER_INPUT, 0, "C2S");
        return fixture->service_dentry != NULL &&
               vfs_write(fixture->client,
                         (void *) ROOT_SWITCH_USER_INPUT,
                         record_size) == (int) record_size &&
               package_connection_ready(fixture, POLLIN);
}

static int precommit_failures_preserve_namespace(
    struct root_switch_fixture *fixture)
{
        for (enum vfs_root_switch_test_checkpoint checkpoint =
                 VFS_ROOT_SWITCH_TEST_FAIL_STAGED_ROOT;
             checkpoint <= VFS_ROOT_SWITCH_TEST_FAIL_TOPOLOGY;
             checkpoint++) {
                vfs_root_switch_test_fail_at(checkpoint);
                if (vfs_switch_root_once() != -EIO ||
                    !old_namespace_usable(fixture))
                        return 0;
        }
        return 1;
}

static int package_round_trip_after_switch(
    struct root_switch_fixture *fixture)
{
        struct frog_pkg_record *received;
        uint_32 record_size = FROG_PKG_HEADER_SIZE + 3U;
        uint_32 peer_id;

        memset((void *) ROOT_SWITCH_USER_OUTPUT, 0, record_size);
        if (vfs_read(fixture->server,
                     (void *) ROOT_SWITCH_USER_OUTPUT,
                     record_size) != (int) record_size)
                return 0;
        received = test_record(ROOT_SWITCH_USER_OUTPUT);
        peer_id = received->peer_id;
        if (peer_id == 0 ||
            !record_matches(ROOT_SWITCH_USER_OUTPUT, peer_id, "C2S"))
                return 0;

        record_size = make_record(ROOT_SWITCH_USER_INPUT, peer_id, "S2C");
        if (vfs_write(fixture->server,
                      (void *) ROOT_SWITCH_USER_INPUT,
                      record_size) != (int) record_size)
                return 0;
        memset((void *) ROOT_SWITCH_USER_OUTPUT, 0, record_size);
        return vfs_read(fixture->client,
                        (void *) ROOT_SWITCH_USER_OUTPUT,
                        record_size) == (int) record_size &&
               record_matches(ROOT_SWITCH_USER_OUTPUT, 0, "S2C");
}

static int device_identity_after_switch(struct root_switch_fixture *fixture)
{
        char byte = 0;
        struct file *reopened = NULL;
        int passed = vfs_lookup("/dev/input/event0") ==
                         fixture->device_dentry &&
                     fixture->device_dentry->d_inode ==
                         fixture->device_inode &&
                     vfs_read(fixture->device, &byte, 1) == -EAGAIN &&
                     vfs_open_file("/dev/input/event0",
                                   O_RDONLY | O_NONBLOCK,
                                   &reopened) == 0;

        if (reopened != NULL) {
                passed = reopened->f_dentry == fixture->device_dentry &&
                         reopened->f_inode == fixture->device_inode && passed;
                passed = close_file(&reopened) == 0 && passed;
        }
        return passed;
}

static int switched_namespace_is_complete(
    struct root_switch_fixture *fixture)
{
        return vfs_lookup("/") == fixture->staged_root &&
               vfs_lookup("/bin/compositor") != NULL &&
               vfs_lookup("/dev") == fixture->dev_root &&
               vfs_lookup("/dev/pkg") == fixture->package_root &&
               vfs_lookup(ROOT_SWITCH_SERVICE) == fixture->service_dentry &&
               vfs_lookup("/sysroot") == NULL &&
               vfs_lookup("/bootstrap-only") == NULL &&
               vfs_root_switch_test_snapshot_committed(&fixture->mounts) &&
               package_connection_ready(fixture, 0) &&
               device_identity_after_switch(fixture);
}

static int release_fixture(struct root_switch_fixture *fixture)
{
        TCB_t *current = running_thread();
        int released = close_file(&fixture->client) == 0;

        released = close_file(&fixture->server) == 0 && released;
        released = close_file(&fixture->device) == 0 && released;
        if (current == NULL || current->mm != fixture->test_mm)
                return 0;
        current->mm = NULL;
        page_dir_activate(current);
        mm_release_address_space(fixture->test_mm);
        fixture->test_mm = NULL;
        return released;
}

void vfs_root_switch_regression_run(void)
{
        struct root_switch_fixture fixture;
        int setup = setup_fixture(&fixture);

        frog_test_case("root-switch.fixture", setup);
        if (!setup) {
                frog_test_finish();
                return;
        }
        frog_test_case("root-switch.precommit-rollback",
                       precommit_failures_preserve_namespace(&fixture));
        frog_test_case("root-switch.commit", vfs_switch_root_once() == 0);
        frog_test_case("root-switch.namespace",
                       switched_namespace_is_complete(&fixture));
        frog_test_case("root-switch.package-round-trip",
                       package_round_trip_after_switch(&fixture));
        frog_test_case("root-switch.single-use",
                       vfs_switch_root_once() == -EALREADY);
        frog_test_case("root-switch.open-objects-close",
                       release_fixture(&fixture));
        frog_test_finish();
}
