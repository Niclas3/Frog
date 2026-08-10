#include <asm/page.h>

#include <frog/bitmap.h>
#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/irqflags.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>
#include <kernel/disk_init_loader.h>
#include <kernel/frogfs_root.h>
#include <kernel/qemu_test.h>
#include <kernel/system_root.h>
#include <kernel/vfs.h>

#define LOADER_SERVICE "/dev/pkg/init-loader"

extern struct list_head thread_all_list;

struct loader_namespace_fixture {
        struct dentry *root;
        struct dentry *dev;
        struct dentry *package;
        struct dentry *event0;
        struct inode *event0_inode;
        struct file *server;
        struct file *client;
};

static uint_32 published_user_mm_count(void)
{
        struct list_head *position;
        unsigned long flags;
        uint_32 count = 0;

        local_irq_save(flags);
        list_for_each(position, &thread_all_list) {
                TCB_t *thread = list_entry(position, TCB_t, all_list_tag);

                if (thread->mm != NULL)
                        count++;
        }
        local_irq_restore(flags);
        return count;
}

static int pid_allocator_next_is(pid_t expected)
{
        pid_t pid = fork_pid();

        if (pid != -1)
                thread_release_pid(pid);
        return pid == expected;
}

static int installed_init_is_larger_than_embedded_limit(void)
{
        struct file *file = NULL;
        int passed = vfs_open_file("/sbin/init", O_RDONLY, &file) == 0 &&
                     file != NULL && file->f_inode != NULL &&
                     file->f_inode->i_size > PAGE_SIZE;

        if (file != NULL)
                passed = vfs_close(file) == 0 && passed;
        return passed;
}

static int namespace_fixture_setup(struct loader_namespace_fixture *fixture)
{
        int paths_ready;
        int server_result;
        int client_result = -EINVAL;

        memset(fixture, 0, sizeof(*fixture));
        fixture->root = vfs_lookup("/");
        fixture->dev = vfs_lookup("/dev");
        fixture->package = vfs_lookup("/dev/pkg");
        fixture->event0 = vfs_lookup("/dev/input/event0");
        fixture->event0_inode = fixture->event0 ?
                                    fixture->event0->d_inode : NULL;
        paths_ready = fixture->root && fixture->dev && fixture->package &&
                      fixture->event0 && fixture->event0_inode &&
                      vfs_lookup("/sysroot") == NULL;
        frog_test_case("disk-init-loader.namespace-paths", paths_ready);
        server_result = paths_ready ?
            vfs_open_file(LOADER_SERVICE,
                          O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NONBLOCK,
                          &fixture->server) : -ENOENT;
        frog_test_case("disk-init-loader.packagefs-server-ready",
                       server_result == 0 && fixture->server != NULL);
        if (server_result == 0)
                client_result = vfs_open_file(
                    LOADER_SERVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK,
                    &fixture->client);
        frog_test_case("disk-init-loader.packagefs-client-ready",
                       client_result == 0 && fixture->client != NULL);
        return paths_ready && server_result == 0 && client_result == 0;
}

static int namespace_fixture_usable(
    const struct loader_namespace_fixture *fixture)
{
        struct file *event = NULL;
        uint_8 input;
        int passed = vfs_lookup("/") == fixture->root &&
                     vfs_lookup("/dev") == fixture->dev &&
                     vfs_lookup("/dev/pkg") == fixture->package &&
                     vfs_lookup("/dev/input/event0") == fixture->event0 &&
                     fixture->event0->d_inode == fixture->event0_inode &&
                     vfs_lookup(LOADER_SERVICE) != NULL &&
                     fixture->server != NULL && fixture->client != NULL &&
                     vfs_lookup("/sysroot") == NULL;

        if (passed)
                passed = vfs_open_file("/dev/input/event0",
                                       O_RDONLY | O_NONBLOCK, &event) == 0 &&
                         event != NULL && event->f_dentry == fixture->event0 &&
                         event->f_inode == fixture->event0_inode &&
                         vfs_read(event, &input, 1) == -EAGAIN;
        if (event != NULL)
                passed = vfs_close(event) == 0 && passed;
        return passed;
}

static int namespace_fixture_close(struct loader_namespace_fixture *fixture)
{
        int passed = 1;

        if (fixture->client != NULL) {
                passed = vfs_close(fixture->client) == 0 && passed;
                fixture->client = NULL;
        }
        if (fixture->server != NULL) {
                passed = vfs_close(fixture->server) == 0 && passed;
                fixture->server = NULL;
        }
        return passed;
}

static int failed_load_left_bootstrap_ownership(
    const struct loader_namespace_fixture *fixture)
{
        TCB_t *current = running_thread();

        return current != NULL && current->pid == 1 && current->mm == NULL &&
               pid2thread(1) == current && published_user_mm_count() == 0 &&
               pid_allocator_next_is(2) && namespace_fixture_usable(fixture);
}

static void run_negative_case(
    const char *case_name,
    const char *path,
    int expected,
    enum disk_init_loader_test_failure failure,
    const struct loader_namespace_fixture *fixture)
{
        const char *const argv[] = { path, "negative", NULL };
        pid_t pid = -1;
        int armed = failure == 0 ||
                    disk_init_loader_test_fail_once(failure) == 0;
        int result = armed ? process_execute_init_path(path, argv, &pid) :
                             -EBUSY;

        frog_test_case(case_name,
                       armed && result == expected && pid == -1 &&
                           failed_load_left_bootstrap_ownership(fixture));
}

static uint_32 mapped_page_count(const struct mm_struct *mm)
{
        uint_32 count = 0;
        uint_32 bit_count = mm->user_vaddr.vaddr_bitmap.map_bytes_length * 8U;

        for (uint_32 bit = 0; bit < bit_count; bit++) {
                if (get_value_bitmap(
                        (struct bitmap *) &mm->user_vaddr.vaddr_bitmap, bit))
                        count++;
        }
        return count;
}

static int startup_context_matches(TCB_t *init)
{
        TCB_t *current = running_thread();
        uint_32 *argv;
        unsigned long flags;
        int passed;

        if (init == NULL || init->mm == NULL ||
            init->user_entry != USER_IMAGE_VADDR ||
            init->user_argc != 2 || init->user_stack >= 0xc0000000U ||
            init->user_argv < USER_STACK3_VADDR ||
            init->user_argv >= 0xc0000000U ||
            init->user_stack != (init->user_argv & ~0x0fU) ||
            mapped_page_count(init->mm) < 2)
                return 0;

        local_irq_save(flags);
        page_dir_activate(init);
        argv = (uint_32 *) init->user_argv;
        passed = argv[0] >= USER_STACK3_VADDR &&
                 argv[0] < 0xc0000000U &&
                 argv[1] >= USER_STACK3_VADDR &&
                 argv[1] < 0xc0000000U && argv[2] == 0 &&
                 strcmp((const char *) argv[0], "/sbin/init") == 0 &&
                 strcmp((const char *) argv[1], "disk-init-loader-smoke") ==
                     0 &&
                 *(const uint_8 *) init->user_entry != 0;
        page_dir_activate(current);
        local_irq_restore(flags);
        return passed;
}

void disk_init_loader_regression_run(void)
{
        const char *const init_argv[] = {
            "/sbin/init", "disk-init-loader-smoke", NULL
        };
        struct loader_namespace_fixture fixture;
        struct frogfs_root_result located = frogfs_locate_root();
        struct frogfs_root_activation activated;
        TCB_t *main = running_thread();
        TCB_t *init;
        pid_t pid = -1;
        int result;

        frog_test_case("disk-init-loader.located-exactly-one",
                       located.status == FROGFS_ROOT_FOUND && located.bdev);
        activated = frogfs_activate_root(&located);
        frog_test_case("disk-init-loader.root-active",
                       activated.status == FROGFS_ROOT_ACTIVATED &&
                           activated.error == 0);
        if (activated.status != FROGFS_ROOT_ACTIVATED) {
                frog_test_finish();
                return;
        }

        frog_test_case("disk-init-loader.production-init-over-4k",
                       installed_init_is_larger_than_embedded_limit());
        frog_test_case("disk-init-loader.namespace-fixture",
                       namespace_fixture_setup(&fixture));
        if (!fixture.root || !fixture.server || !fixture.client) {
                frog_test_finish();
                return;
        }

        run_negative_case("disk-init-loader.missing-rollback",
                          "/sbin/missing", -ENOENT, 0, &fixture);
        run_negative_case("disk-init-loader.malformed-rollback",
                          "/loader-fixtures/malformed", -ENOEXEC, 0,
                          &fixture);
        run_negative_case("disk-init-loader.oversized-rollback",
                          "/loader-fixtures/oversized", -E2BIG, 0,
                          &fixture);
        run_negative_case("disk-init-loader.unmappable-rollback",
                          "/loader-fixtures/unmappable", -ENOEXEC, 0,
                          &fixture);
        run_negative_case("disk-init-loader.argument-allocation-rollback",
                          "/sbin/init", -ENOMEM,
                          DISK_INIT_LOADER_FAIL_ARGUMENT_ALLOCATION,
                          &fixture);
        run_negative_case("disk-init-loader.partial-map-rollback",
                          "/sbin/init", -ENOMEM,
                          DISK_INIT_LOADER_FAIL_IMAGE_MAPPING, &fixture);
        run_negative_case("disk-init-loader.pid-swap-publish-rollback",
                          "/sbin/init", -ENOMEM,
                          DISK_INIT_LOADER_FAIL_AFTER_PID_SWAP, &fixture);

        result = process_execute_init_path("/sbin/init", init_argv, &pid);
        init = pid2thread(1);
        frog_test_case("disk-init-loader.production-init-is-pid1",
                       result == 0 && pid == 1 && init != NULL &&
                           init != main && main->pid == 2 &&
                           pid2thread(2) == main &&
                           pid_allocator_next_is(3));
        frog_test_case("disk-init-loader.published-process-contract",
                       init != NULL && init->pid == 1 &&
                           init->parent_pid == -1 &&
                           strcmp(init->name, "init") == 0 &&
                           init->mm != NULL &&
                           published_user_mm_count() == 1);
        frog_test_case("disk-init-loader.entry-stack-argv-contract",
                       startup_context_matches(init));
        frog_test_case("disk-init-loader.namespace-preserved",
                       namespace_fixture_usable(&fixture));
        frog_test_case("disk-init-loader.packagefs-close",
                       namespace_fixture_close(&fixture));
        frog_test_finish();
}
