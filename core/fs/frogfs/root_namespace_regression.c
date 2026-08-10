#include <frog/errno.h>
#include <frog/fb.h>
#include <frog/fcntl.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/printk.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>
#include <input/mouse.h>
#include <kernel/frogfs.h>
#include <kernel/frogfs_root.h>
#include <kernel/qemu_test.h>
#include <kernel/system_root.h>
#include <kernel/vfs.h>

#ifndef CONFIG_FROG_TEST_ROOT_NAMESPACE_WRITABLE
#define ROOT_NAMESPACE_SERVICE "/dev/pkg/root-namespace"
#define TEST_USER_PAGE USER_IMAGE_VADDR
#define TEST_PKG_INPUT (TEST_USER_PAGE + 1024U)
#define TEST_PKG_OUTPUT (TEST_USER_PAGE + 2048U)
#define TEST_FB_INFO (TEST_USER_PAGE + 3072U)

struct namespace_fixture {
        struct dentry *bootstrap_only;
        struct dentry *dev_root;
        struct dentry *package_root;
        struct dentry *event[2];
        struct dentry *framebuffer;
        struct inode *event_inode[2];
        struct inode *framebuffer_inode;
        struct file *event_file[2];
        struct file *framebuffer_file;
        struct file *server;
        struct file *client;
        struct dentry *service;
        void *server_state;
        void *client_state;
        struct mm_struct *test_mm;
};

static int close_file(struct file **file)
{
        int result;

        if (!file || !*file)
                return 0;
        result = vfs_close(*file);
        *file = NULL;
        return result;
}

static int install_user_page(struct namespace_fixture *fixture)
{
        TCB_t *current = running_thread();

        if (!current || current->mm)
                return 0;
        fixture->test_mm = process_create_user_mm();
        if (!fixture->test_mm ||
            vm_user_map_owned_page(fixture->test_mm, TEST_USER_PAGE) != 0)
                return 0;
        current->mm = fixture->test_mm;
        page_dir_activate(current);
        return 1;
}

static uint_32 make_record(uint_32 address, uint_32 peer_id,
                           const char payload[3])
{
        struct frog_pkg_record *record =
            (struct frog_pkg_record *) address;

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
        struct frog_pkg_record *record =
            (struct frog_pkg_record *) address;

        return record->peer_id == peer_id &&
               record->event == FROG_PKG_DATA &&
               record->payload_size == 3U &&
               memcmp(record->payload, payload, 3U) == 0;
}

static int setup_fixture(struct namespace_fixture *fixture)
{
        uint_8 input_buffer[sizeof(mouse_device_packet_t)];
        uint_32 size;

        memset(fixture, 0, sizeof(*fixture));
        if (vfs_mkdir_path("/bootstrap-only") != 0 ||
            !install_user_page(fixture))
                return 0;
        fixture->bootstrap_only = vfs_lookup("/bootstrap-only");
        fixture->dev_root = vfs_lookup("/dev");
        fixture->package_root = vfs_lookup("/dev/pkg");
        fixture->event[0] = vfs_lookup("/dev/input/event0");
        fixture->event[1] = vfs_lookup("/dev/input/event1");
        fixture->framebuffer = vfs_lookup("/dev/fb0");
        if (!fixture->bootstrap_only || !fixture->dev_root ||
            !fixture->package_root || !fixture->event[0] ||
            !fixture->event[1] || !fixture->framebuffer)
                return 0;
        fixture->event_inode[0] = fixture->event[0]->d_inode;
        fixture->event_inode[1] = fixture->event[1]->d_inode;
        fixture->framebuffer_inode = fixture->framebuffer->d_inode;
        if (!fixture->event_inode[0] || !fixture->event_inode[1] ||
            !fixture->framebuffer_inode)
                return 0;
        for (int index = 0; index < 2; index++) {
                if (vfs_open_file(index ? "/dev/input/event1" :
                                           "/dev/input/event0",
                                  O_RDONLY | O_NONBLOCK,
                                  &fixture->event_file[index]) != 0 ||
                    vfs_read(fixture->event_file[index], input_buffer,
                             index ? sizeof(mouse_device_packet_t) : 1U) !=
                        -EAGAIN)
                        return 0;
        }
        if (vfs_open_file("/dev/fb0", O_RDWR,
                          &fixture->framebuffer_file) != 0 ||
            vfs_open_file(ROOT_NAMESPACE_SERVICE,
                          O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC |
                              O_NONBLOCK,
                          &fixture->server) != 0 ||
            vfs_open_file(ROOT_NAMESPACE_SERVICE,
                          O_RDWR | O_CLOEXEC | O_NONBLOCK,
                          &fixture->client) != 0)
                return 0;
        fixture->service = vfs_lookup(ROOT_NAMESPACE_SERVICE);
        fixture->server_state = fixture->server->private_data;
        fixture->client_state = fixture->client->private_data;
        size = make_record(TEST_PKG_INPUT, 0, "C2S");
        return fixture->service &&
               vfs_write(fixture->client, (void *) TEST_PKG_INPUT, size) ==
                   (int) size;
}

static int read_elf(const char *path)
{
        struct file *file = NULL;
        uint_8 magic[4] = {0};
        int passed = vfs_open_file(path, O_RDONLY, &file) == 0;

        if (passed)
                passed = vfs_read(file, magic, sizeof(magic)) == 4 &&
                         magic[0] == 0x7f && magic[1] == 'E' &&
                         magic[2] == 'L' && magic[3] == 'F';
        if (file)
                passed = close_file(&file) == 0 && passed;
        return passed;
}

static int open_fails(const char *path, uint_32 flags, int expected)
{
        struct file *file = NULL;
        int result = vfs_open_file(path, flags, &file);

        if (file)
                close_file(&file);
        return result == expected;
}

static int readonly_mutations_are_denied(void)
{
        return open_fails("/bin/compositor", O_WRONLY, -EROFS) &&
               open_fails("/bin/compositor", O_WRONLY | O_TRUNC,
                          -EROFS) &&
               open_fails("/readonly-new", O_CREAT | O_WRONLY, -EROFS) &&
               vfs_mkdir_path("/readonly-dir") == -EROFS &&
               vfs_unlink_path("/bin/desktop") == -EROFS &&
               vfs_rmdir_path("/etc") == -EROFS;
}

static int package_round_trip(struct namespace_fixture *fixture)
{
        struct frog_pkg_record *received;
        uint_32 size = FROG_PKG_HEADER_SIZE + 3U;
        uint_32 peer_id;

        memset((void *) TEST_PKG_OUTPUT, 0, size);
        if (vfs_read(fixture->server, (void *) TEST_PKG_OUTPUT, size) !=
            (int) size)
                return 0;
        received = (struct frog_pkg_record *) TEST_PKG_OUTPUT;
        peer_id = received->peer_id;
        if (!peer_id ||
            !record_matches(TEST_PKG_OUTPUT, peer_id, "C2S"))
                return 0;
        size = make_record(TEST_PKG_INPUT, peer_id, "S2C");
        if (vfs_write(fixture->server, (void *) TEST_PKG_INPUT, size) !=
            (int) size)
                return 0;
        memset((void *) TEST_PKG_OUTPUT, 0, size);
        return vfs_read(fixture->client, (void *) TEST_PKG_OUTPUT, size) ==
                   (int) size &&
               record_matches(TEST_PKG_OUTPUT, 0, "S2C");
}

static int devices_preserved(struct namespace_fixture *fixture)
{
        struct file *reopened = NULL;
        struct frog_fb_info *info = (struct frog_fb_info *) TEST_FB_INFO;
        uint_8 input_buffer[sizeof(mouse_device_packet_t)];
        int passed = vfs_lookup("/dev") == fixture->dev_root &&
                     vfs_lookup("/dev/pkg") == fixture->package_root &&
                     vfs_lookup(ROOT_NAMESPACE_SERVICE) == fixture->service &&
                     fixture->server->private_data == fixture->server_state &&
                     fixture->client->private_data == fixture->client_state;

        for (int index = 0; index < 2; index++) {
                const char *path = index ? "/dev/input/event1" :
                                           "/dev/input/event0";
                passed = vfs_lookup(path) == fixture->event[index] &&
                         fixture->event[index]->d_inode ==
                             fixture->event_inode[index] &&
                         vfs_read(fixture->event_file[index], input_buffer,
                                  index ? sizeof(mouse_device_packet_t) :
                                          1U) ==
                             -EAGAIN &&
                         vfs_open_file(path, O_RDONLY | O_NONBLOCK,
                                       &reopened) == 0 &&
                         reopened->f_dentry == fixture->event[index] &&
                         reopened->f_inode == fixture->event_inode[index] &&
                         passed;
                passed = close_file(&reopened) == 0 && passed;
        }
        memset(info, 0, sizeof(*info));
        passed = vfs_lookup("/dev/fb0") == fixture->framebuffer &&
                 fixture->framebuffer->d_inode ==
                     fixture->framebuffer_inode &&
                 vfs_ioctl(fixture->framebuffer_file,
                           FROG_FB_IOCTL_GET_INFO, info) == 0 &&
                 info->width != 0 && info->height != 0 &&
                 vfs_open_file("/dev/fb0", O_RDWR, &reopened) == 0 &&
                 reopened->f_dentry == fixture->framebuffer &&
                 reopened->f_inode == fixture->framebuffer_inode && passed;
        passed = close_file(&reopened) == 0 && passed;
        return passed;
}

static int release_fixture(struct namespace_fixture *fixture)
{
        TCB_t *current = running_thread();
        int passed = close_file(&fixture->client) == 0;

        passed = close_file(&fixture->server) == 0 && passed;
        passed = close_file(&fixture->framebuffer_file) == 0 && passed;
        passed = close_file(&fixture->event_file[1]) == 0 && passed;
        passed = close_file(&fixture->event_file[0]) == 0 && passed;
        if (!current || current->mm != fixture->test_mm)
                return 0;
        current->mm = NULL;
        page_dir_activate(current);
        mm_release_address_space(fixture->test_mm);
        fixture->test_mm = NULL;
        return passed;
}

static void activation_failure_rollback_regression_run(void)
{
        struct block_device invalid_bdev;
        struct frogfs_root_result invalid = {
            .status = FROGFS_ROOT_FOUND,
            .bdev = &invalid_bdev,
        };
        struct frogfs_root_activation activation;
        int registration_result;

        memset(&invalid_bdev, 0, sizeof(invalid_bdev));

        int prepared = vfs_mkdir_path("/sysroot") == 0;
        activation = frogfs_activate_root(&invalid);
        registration_result = frogfs_init();
        frog_test_case("root-namespace.mkdir-failure-rolls-back",
                       prepared &&
                           activation.status ==
                               FROGFS_ROOT_ACTIVATE_MKDIR_FAILED &&
                           activation.error == -EEXIST &&
                           activation.cleanup_error == 0 &&
                           vfs_lookup("/sysroot") != NULL &&
                           registration_result == 0);
        frogfs_init_rollback();
        vfs_rmdir_path("/sysroot");

        activation = frogfs_activate_root(&invalid);
        registration_result = frogfs_init();
        frog_test_case("root-namespace.mount-failure-rolls-back",
                       activation.status ==
                               FROGFS_ROOT_ACTIVATE_MOUNT_FAILED &&
                           activation.cleanup_error == 0 &&
                           !vfs_lookup("/sysroot") &&
                           registration_result == 0);
        frogfs_init_rollback();
        if (vfs_lookup("/sysroot"))
                vfs_rmdir_path("/sysroot");
}
#endif

void frogfs_root_namespace_regression_run(void)
{
#ifdef CONFIG_FROG_TEST_ROOT_NAMESPACE_WRITABLE
        struct frogfs_root_result located = frogfs_locate_root();
        struct frogfs_root_activation activated =
            frogfs_activate_root(&located);
        int located_passed = located.status == FROGFS_ROOT_FOUND &&
                             located.bdev;
        int rejected_passed =
            activated.status == FROGFS_ROOT_ACTIVATE_NOT_READ_ONLY &&
            activated.error == -EROFS && activated.bdev == located.bdev &&
            activated.sb && !frogfs_super_is_read_only(activated.sb);
        int not_switched_passed =
            vfs_lookup("/") && vfs_lookup("/dev") &&
            vfs_lookup("/sysroot") && !vfs_lookup("/bin/compositor");

        frog_test_case("root-namespace.writable-root-located",
                       located_passed);
        if (located_passed)
                printk("FROGTEST CASE "
                       "root-namespace.writable-root-located PASS\n");
        frog_test_case(
            "root-namespace.writable-root-rejected",
            rejected_passed);
        if (rejected_passed)
                printk("FROGTEST CASE "
                       "root-namespace.writable-root-rejected PASS\n");
        frog_test_case("root-namespace.writable-root-not-switched",
                       not_switched_passed);
        if (not_switched_passed)
                printk("FROGTEST CASE "
                       "root-namespace.writable-root-not-switched PASS\n");
        frog_test_finish();
#else
        struct namespace_fixture fixture;
        int setup = setup_fixture(&fixture);

        frog_test_case("root-namespace.fixture", setup);
        if (!setup) {
                frog_test_finish();
                return;
        }

        activation_failure_rollback_regression_run();

        struct frogfs_root_result located = frogfs_locate_root();
        frog_test_case("root-namespace.located-exactly-one",
                       located.status == FROGFS_ROOT_FOUND && located.bdev);
        struct frogfs_root_activation activated =
            frogfs_activate_root(&located);
        int active = activated.status == FROGFS_ROOT_ACTIVATED &&
                     activated.error == 0 && activated.bdev == located.bdev &&
                     activated.sb && activated.sb->s_bdev == located.bdev &&
                     frogfs_super_is_read_only(activated.sb);
        frog_test_case("root-namespace.activate-exact-readonly-source",
                       active);
        if (!active) {
                frog_test_finish();
                return;
        }

        frog_test_case("root-namespace.switched-paths",
                       vfs_lookup("/") && !vfs_lookup("/sysroot") &&
                           !vfs_lookup("/bootstrap-only"));
        frog_test_case("root-namespace.installed-read",
                       read_elf("/bin/compositor") &&
                           read_elf("/sbin/init"));
        frog_test_case("root-namespace.readonly-mutations",
                       readonly_mutations_are_denied());
        frog_test_case("root-namespace.devices-preserved",
                       devices_preserved(&fixture));
        frog_test_case("root-namespace.package-round-trip",
                       package_round_trip(&fixture));
        frog_test_case("root-namespace.open-objects-close",
                       release_fixture(&fixture));
        frog_test_finish();
#endif
}
