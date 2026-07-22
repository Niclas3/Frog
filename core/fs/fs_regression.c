#include <kernel/fs_regression.h>

#ifdef CONFIG_FROG_TEST_DISK

#include <asm/i386_syscall_common.h>
#include <frog/block.h>
#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <frog/syscall.h>
#include <kernel/frogfs.h>
#include <kernel/qemu_test.h>
#include <kernel/vfs.h>

#define TEST_DEVICE_PATH "/dev/sdbp8"
#define PERSIST_DIR "/test/persist"
#define PERSIST_PATH PERSIST_DIR "/blob"
#define PERSIST_SIZE (12U * 1024U + 37U)

#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
extern const uint_8 _binary_exec_target_elf_start[];
extern const uint_8 _binary_exec_target_elf_end[];
#endif

#if (defined(CONFIG_FROG_TEST_STAGE_PREPARE) +                         \
     defined(CONFIG_FROG_TEST_STAGE_VERIFY) +                          \
     defined(CONFIG_FROG_TEST_STAGE_CORRUPT)) != 1
#error "disk-smoke requires exactly one CONFIG_FROG_TEST_STAGE_* macro"
#endif

static int close_file(struct file **file)
{
        if (!file || !*file)
                return 0;
        int ret = vfs_close(*file);
        *file = NULL;
        return ret;
}

#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
static int install_exec_fixture(void)
{
        const uint_8 *start = _binary_exec_target_elf_start;
        uint_32 size = (uint_32) (_binary_exec_target_elf_end - start);
        struct file *file = NULL;
        int result = vfs_open_file("/test/exec-target",
                                   O_CREAT | O_EXCL | O_WRONLY, &file);
        int passed = result == 0;

        if (passed)
                passed = vfs_write(file, start, size) == (int_32) size;
        if (file != NULL)
                passed = close_file(&file) == 0 && passed;
        if (!passed)
                (void) vfs_unlink_path("/test/exec-target");
        return passed;
}
#endif

static int test_access_modes(void)
{
        static const char payload[] = "access";
        char byte = 0;
        struct file *file = NULL;
        int ret = vfs_open_file("/test/access", O_CREAT | O_EXCL | O_WRONLY,
                                &file);
        if (ret < 0)
                return 0;
        int passed = vfs_read(file, &byte, 1) == -EBADF &&
                     vfs_write(file, payload, sizeof(payload) - 1) ==
                         (int) sizeof(payload) - 1;
        passed = close_file(&file) == 0 && passed;

        ret = vfs_open_file("/test/access", O_RDONLY, &file);
        if (ret < 0)
                passed = 0;
        else {
                passed = vfs_write(file, payload, 1) == -EBADF && passed;
                passed = close_file(&file) == 0 && passed;
        }
        passed = vfs_unlink_path("/test/access") == 0 && passed;

        ret = vfs_open_file("/test/access", O_ACCMODE, &file);
        if (ret >= 0)
                close_file(&file);
        return ret == -EINVAL && passed;
}

static int test_exclusive_create(void)
{
        struct file *file = NULL;
        int ret = vfs_open_file("/test/exclusive",
                                O_CREAT | O_EXCL | O_RDWR, &file);
        if (ret < 0)
                return 0;
        int passed = close_file(&file) == 0;
        ret = vfs_open_file("/test/exclusive",
                            O_CREAT | O_EXCL | O_RDWR, &file);
        if (ret >= 0)
                close_file(&file);
        passed = ret == -EEXIST && passed;
        return vfs_unlink_path("/test/exclusive") == 0 && passed;
}

static int test_append_truncate_and_seek(void)
{
        char buf[3] = {0};
        struct file *file = NULL;
        int ret = vfs_open_file("/test/flags",
                                O_CREAT | O_EXCL | O_RDWR, &file);
        if (ret < 0)
                return 0;
        int passed = vfs_write(file, "A", 1) == 1;
        passed = vfs_lseek(file, -1, SEEK_SET) == -EINVAL && passed;
        passed = close_file(&file) == 0 && passed;

        ret = vfs_open_file("/test/flags", O_WRONLY | O_APPEND, &file);
        if (ret < 0)
                passed = 0;
        else {
                passed = vfs_lseek(file, 0, SEEK_SET) == 0 && passed;
                passed = vfs_write(file, "B", 1) == 1 && passed;
                passed = close_file(&file) == 0 && passed;
        }

        ret = vfs_open_file("/test/flags", O_RDONLY, &file);
        if (ret < 0)
                passed = 0;
        else {
                passed = vfs_read(file, buf, 2) == 2 &&
                         !memcmp(buf, "AB", 2) && passed;
                passed = close_file(&file) == 0 && passed;
        }

        ret = vfs_open_file("/test/flags", O_RDWR | O_TRUNC, &file);
        if (ret < 0)
                passed = 0;
        else {
                passed = vfs_read(file, buf, 1) == 0 && passed;
                passed = close_file(&file) == 0 && passed;
        }
        return vfs_unlink_path("/test/flags") == 0 && passed;
}

static int test_open_unlink_recreate(void)
{
        struct file *file = NULL;
        int ret = vfs_open_file("/test/unlink", O_CREAT | O_EXCL | O_RDWR,
                                &file);
        if (ret < 0)
                return 0;
        int passed = vfs_unlink_path("/test/unlink") == -EBUSY;
        passed = close_file(&file) == 0 && passed;
        passed = vfs_unlink_path("/test/unlink") == 0 && passed;

        ret = vfs_open_file("/test/unlink", O_CREAT | O_EXCL | O_RDWR,
                            &file);
        if (ret < 0)
                return 0;
        passed = close_file(&file) == 0 && passed;
        return vfs_unlink_path("/test/unlink") == 0 && passed;
}

static int test_directory_lifecycle(void)
{
        int passed = vfs_mkdir_path("/test/dircase") == 0;
        passed = vfs_mkdir_path("/test/dircase/child") == 0 && passed;
        passed = vfs_rmdir_path("/test/dircase") == -ENOTEMPTY && passed;
        passed = vfs_rmdir_path("/test/dircase/child") == 0 && passed;
        passed = vfs_rmdir_path("/test/dircase") == 0 && passed;
        return passed;
}

static int test_tmpfs_open_rmdir(void)
{
        int passed = vfs_mkdir_path("/vfsbusy") == 0;
        struct file *file = NULL;
        int ret = vfs_open_file("/vfsbusy", O_RDONLY | O_DIRECTORY, &file);
        passed = ret == 0 && passed;
        if (ret == 0) {
                passed = vfs_rmdir_path("/vfsbusy") == -EBUSY && passed;
                passed = close_file(&file) == 0 && passed;
        }
        passed = vfs_rmdir_path("/vfsbusy") == 0 && passed;
        return passed;
}

static int rejected_open(const char *path, uint_32 flags)
{
        struct file *file = NULL;
        int ret = vfs_open_file(path, flags, &file);
        if (ret >= 0)
                close_file(&file);
        return ret < 0;
}

static int test_path_boundaries(void)
{
        char component[FILE_NAME_MAX + 3];
        component[0] = '/';
        for (uint_32 idx = 1; idx <= FILE_NAME_MAX + 1; idx++)
                component[idx] = 'a';
        component[FILE_NAME_MAX + 2] = '\0';

        char *overlong = kmalloc(PATH_NAME_MAX + 2);
        if (!overlong)
                return 0;
        for (uint_32 idx = 0; idx <= PATH_NAME_MAX; idx++)
                overlong[idx] = idx % 2 ? 'a' : '/';
        overlong[PATH_NAME_MAX + 1] = '\0';

        int passed = rejected_open("relative", O_RDONLY);
        passed = rejected_open("/test//bad", O_CREAT | O_RDWR) && passed;
        passed = rejected_open("/test/./bad", O_CREAT | O_RDWR) && passed;
        passed = rejected_open("/test/bad/", O_CREAT | O_RDWR) && passed;
        passed = rejected_open(component, O_CREAT | O_RDWR) && passed;
        passed = rejected_open(overlong, O_CREAT | O_RDWR) && passed;
        passed = rejected_open("/test/abcdefghijklmnop",
                               O_CREAT | O_RDWR) && passed;
        kfree(overlong);
        return passed;
}

static int test_metadata_failure_rollback(void)
{
        struct file *file = NULL;
        frogfs_test_fail_io_after(1, 1,
                                  FROGFS_TEST_IO_WRITE |
                                      FROGFS_TEST_IO_METADATA);
        int ret = vfs_open_file("/test/fault", O_CREAT | O_EXCL | O_RDWR,
                                &file);
        frogfs_test_clear_io_failpoint();
        if (ret >= 0)
                close_file(&file);

        int passed = ret == -EIO && !vfs_lookup("/test/fault");
        ret = vfs_open_file("/test/fault", O_CREAT | O_EXCL | O_RDWR,
                            &file);
        if (ret < 0)
                return 0;
        passed = close_file(&file) == 0 && passed;
        return vfs_unlink_path("/test/fault") == 0 && passed;
}

static uint_8 persistence_byte(uint_32 offset)
{
        return (uint_8) ((offset * 37U + (offset >> 3) + 0x5aU) & 0xffU);
}

static int write_persistent_file(void)
{
        if (vfs_mkdir_path(PERSIST_DIR) < 0)
                return 0;
        uint_8 *buf = kmalloc(PERSIST_SIZE);
        if (!buf)
                return 0;
        for (uint_32 idx = 0; idx < PERSIST_SIZE; idx++)
                buf[idx] = persistence_byte(idx);

        struct file *file = NULL;
        int ret = vfs_open_file(PERSIST_PATH,
                                O_CREAT | O_EXCL | O_RDWR, &file);
        int passed = ret == 0;
        if (passed)
                passed = vfs_write(file, buf, PERSIST_SIZE) ==
                         (int) PERSIST_SIZE;
        if (file)
                passed = close_file(&file) == 0 && passed;
        kfree(buf);
        return passed;
}

static int verify_persistent_file(void)
{
        uint_8 *buf = kmalloc(PERSIST_SIZE);
        if (!buf)
                return 0;
        memset(buf, 0, PERSIST_SIZE);

        struct file *file = NULL;
        int ret = vfs_open_file(PERSIST_PATH, O_RDONLY, &file);
        int passed = ret == 0;
        if (passed)
                passed = file->f_inode->i_size == PERSIST_SIZE &&
                         vfs_read(file, buf, PERSIST_SIZE) ==
                             (int) PERSIST_SIZE;
        if (passed) {
                for (uint_32 idx = 0; idx < PERSIST_SIZE; idx++) {
                        if (buf[idx] != persistence_byte(idx)) {
                                passed = 0;
                                break;
                        }
                }
        }
        uint_8 extra = 0;
        if (passed)
                passed = vfs_read(file, &extra, 1) == 0;
        if (file)
                passed = close_file(&file) == 0 && passed;
        kfree(buf);
        return passed;
}

static int corrupt_super_magic(void)
{
        struct dentry *device = vfs_lookup(TEST_DEVICE_PATH);
        if (!device || !device->d_inode)
                return 0;
        struct block_device *bdev =
            get_block_device(device->d_inode->i_dev);
        if (!bdev)
                return 0;

        uint_8 *sector = kmalloc(512);
        uint_8 *check = kmalloc(512);
        if (!sector || !check) {
                if (sector)
                        kfree(sector);
                if (check)
                        kfree(check);
                return 0;
        }

        int passed = bio_read(bdev, bdev->bd_start_lba, sector, 1) == 0;
        if (passed) {
                *(uint_32 *) sector = 0;
                passed = bio_write(bdev, bdev->bd_start_lba, sector, 1) == 0;
        }
        if (passed) {
                memset(check, 0, 512);
                passed = bio_read(bdev, bdev->bd_start_lba, check, 1) == 0 &&
                         *(uint_32 *) check == 0;
        }
        kfree(check);
        kfree(sector);
        return passed;
}

static int corrupted_device_is_present(void)
{
        struct dentry *device = vfs_lookup(TEST_DEVICE_PATH);
        if (!device || !device->d_inode || device->d_type != FT_BLOCK)
                return 0;
        struct block_device *bdev =
            get_block_device(device->d_inode->i_dev);
        if (!bdev)
                return 0;
        uint_8 *sector = kmalloc(512);
        if (!sector)
                return 0;
        int passed = bio_read(bdev, bdev->bd_start_lba, sector, 1) == 0 &&
                     *(uint_32 *) sector == 0;
        kfree(sector);
        return passed;
}

const char *fs_regression_profile(void)
{
#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
        return "disk-smoke.prepare";
#elif defined(CONFIG_FROG_TEST_STAGE_VERIFY)
        return "disk-smoke.verify";
#else
        return "disk-smoke.corrupt";
#endif
}

int fs_regression_mount_flags(void)
{
#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
        return FROGFS_MOUNT_FORMAT;
#else
        return 0;
#endif
}

void fs_regression_run_kernel(int init_result,
                              int mount_result,
                              int rollback_result)
{
#ifdef CONFIG_FROG_TEST_STAGE_CORRUPT
        frog_test_case("frogfs.corrupt-mount-rejected",
                       init_result == 0 && mount_result < 0 &&
                           rollback_result == 0 &&
                           corrupted_device_is_present());
#else
        int mounted = init_result == 0 && mount_result == 0;
        frog_test_case("frogfs.mount", mounted);
        if (!mounted)
                return;
#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
        frog_test_case("exec.fixture-install", install_exec_fixture());
        frog_test_case("frogfs.flags-access", test_access_modes());
        frog_test_case("frogfs.exclusive-create", test_exclusive_create());
        frog_test_case("frogfs.append-truncate-seek",
                       test_append_truncate_and_seek());
        frog_test_case("frogfs.open-unlink-recreate",
                       test_open_unlink_recreate());
        frog_test_case("frogfs.mkdir-rmdir", test_directory_lifecycle());
        frog_test_case("vfs.tmpfs-open-rmdir", test_tmpfs_open_rmdir());
        frog_test_case("frogfs.path-boundaries", test_path_boundaries());
        frog_test_case("frogfs.metadata-rollback",
                       test_metadata_failure_rollback());
        frog_test_case("frogfs.indirect-persist-write",
                       write_persistent_file());
#else
        frog_test_case("frogfs.cold-persist-read",
                       verify_persistent_file());
        frog_test_case("frogfs.corrupt-super-magic", corrupt_super_magic());
#endif
#endif
}

static int wait_for_child(pid_t expected_pid, int expected_status)
{
        int_32 status = -1;
        pid_t waited = wait(&status);
        return waited == expected_pid && status == expected_status;
}

static int test_child_close_parent_uses_fd(void)
{
        int fd = open("/test/forka", O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return 0;
        int passed = fd == 0 && write(fd, "A", 1) == 1 &&
                     lseek(fd, 0, SEEK_SET) == 0;
        pid_t pid = fork();
        if (pid == 0) {
                int child_ok = close(fd) == 0;
                exit(child_ok ? 0 : 1);
                for (;;) {
                }
        }
        if (pid == -1)
                passed = 0;
        else {
                passed = wait_for_child(pid, 0) && passed;
                char value = 0;
                passed = read(fd, &value, 1) == 1 && value == 'A' && passed;
        }
        passed = close(fd) == 0 && passed;
        return unlink("/test/forka") == 0 && passed;
}

static int test_parent_close_child_uses_fd(void)
{
        int fd = open("/test/forkb", O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return 0;
        int passed = fd == 0 && write(fd, "B", 1) == 1 &&
                     lseek(fd, 0, SEEK_SET) == 0;
        pid_t pid = fork();
        if (pid == 0) {
                char value = 0;
                int child_ok = read(fd, &value, 1) == 1 && value == 'B';
                child_ok = close(fd) == 0 && child_ok;
                exit(child_ok ? 0 : 1);
                for (;;) {
                }
        }
        passed = close(fd) == 0 && passed;
        if (pid == -1)
                passed = 0;
        else
                passed = wait_for_child(pid, 0) && passed;
        return unlink("/test/forkb") == 0 && passed;
}

void fs_regression_run_user(void)
{
#ifdef CONFIG_FROG_TEST_STAGE_PREPARE
        frog_test_case("fd.fork-child-close",
                       test_child_close_parent_uses_fd());
        frog_test_case("fd.fork-parent-close",
                       test_parent_close_child_uses_fd());
#endif
}

#endif
