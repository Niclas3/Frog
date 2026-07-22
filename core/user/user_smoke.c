#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define USER_SMOKE_MAGIC 0x46524f47U
#define USER_SEEK_SET 1

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

#if defined(USER_SMOKE_PROCESS) || defined(USER_SMOKE_DISK_PREPARE)
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
#endif

static void report(uint_32 id, bool passed)
{
        raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(uint_32 value)
{
        raw_syscall1(SYS_TESTSYSCALL, value);
        for (;;)
                __asm__ volatile("pause");
}

static bool run_basic_checks(void)
{
        bool invalid = raw_syscall0(SYS_NR_COUNT + 17) == -ENOSYS;
        bool unimplemented = raw_syscall0(SYS_STAT) == -ENOSYS &&
                             raw_syscall0(SYS_TEST_SYNC) == -EOPNOTSUPP;

        report(FROG_TEST_SYSCALL_OUT_OF_RANGE, invalid);
        report(FROG_TEST_SYSCALL_UNIMPLEMENTED, unimplemented);
        return invalid && unimplemented;
}

#ifdef USER_SMOKE_PROCESS
static void process_exit(int_32 status) __attribute__((noreturn));

static void process_exit(int_32 status)
{
        exit(status);
        for (;;)
                __asm__ volatile("pause");
}

static bool vm_refs_are(uint_32 expected)
{
        return raw_syscall1(SYS_TESTSYSCALL,
                            FROG_TEST_VM_VERIFY_REFS_BASE + expected) == 0;
}

static bool wait_for_status(pid_t pid, int_32 expected)
{
        int_32 status = 0;

        return pid > 0 && wait(&status) == pid &&
               status == expected;
}

static bool test_wait_fault_retry(void)
{
        pid_t child = fork();
        int_32 status = -1;

        if (child == 0)
                process_exit(38);
        if (child < 0)
                return false;
        return wait((int_32 *) 0xc0000000U) == -EFAULT &&
               wait(&status) == child && status == 38;
}

static bool test_zombie_adoption(void)
{
        pid_t worker = fork();
        int_32 first_status = -1;
        int_32 second_status = -1;
        pid_t first;
        pid_t second;

        if (worker == 0) {
                pid_t grandchild = fork();

                if (grandchild == 0)
                        process_exit(62);
                if (grandchild < 0)
                        process_exit(63);

                /* -EFAULT proves the grandchild is already a zombie. */
                process_exit(wait((int_32 *) 0xc0000000U) == -EFAULT ?
                                 61 : 64);
        }
        if (worker < 0)
                return false;

        first = wait(&first_status);
        second = wait(&second_status);
        return first > 0 && second > 0 && first != second &&
               ((first == worker && first_status == 61 &&
                 second_status == 62) ||
                (second == worker && second_status == 61 &&
                 first_status == 62));
}

static bool test_user_heap_fork(void)
{
        volatile uint_8 *seed = (volatile uint_8 *) (uint_32)
            raw_syscall1(SYS_TESTSYSCALL, FROG_TEST_HEAP_ALLOC_16);
        int_32 child_block = -1;

        if (seed == NULL)
                return false;
        *seed = 0x31U;

        pid_t child = fork();
        if (child == 0) {
                volatile uint_8 *block = (volatile uint_8 *) (uint_32)
                    raw_syscall1(SYS_TESTSYSCALL,
                                 FROG_TEST_HEAP_ALLOC_16);
                bool valid = *seed == 0x31U && block != NULL &&
                             block != seed;

                if (valid) {
                        *block = 0x42U;
                        valid = *block == 0x42U && *seed == 0x31U;
                }
                process_exit(valid ? (int_32) (uint_32) block : -1);
        }
        if (child < 0 || wait(&child_block) != child || child_block <= 0 ||
            *seed != 0x31U)
                return false;

        volatile uint_8 *parent_block = (volatile uint_8 *) (uint_32)
            raw_syscall1(SYS_TESTSYSCALL, FROG_TEST_HEAP_ALLOC_16);
        bool valid = parent_block != NULL &&
                     (int_32) (uint_32) parent_block == child_block;

        if (valid) {
                *parent_block = 0x53U;
                valid = *parent_block == 0x53U && *seed == 0x31U;
        }
        return valid;
}

static bool test_vm_fork_rollback(void)
{
        bool passed = true;

        for (uint_32 step = 0; step < FROG_TEST_VM_FORK_FAIL_COUNT; step++) {
                bool armed = raw_syscall1(
                    SYS_TESTSYSCALL,
                    FROG_TEST_VM_FORK_FAIL_BASE + step) == 0;
                int_32 child = raw_syscall0(SYS_FORK);

                if (child == 0)
                        process_exit(70);
                if (child > 0)
                        (void) wait_for_status(child, 70);
                passed = armed && child == -1 && vm_refs_are(1) && passed;
        }
        return passed;
}

static bool test_vm_parent_first(volatile uint_8 *alias)
{
        const uint_8 waiting = 0x41U;
        const uint_8 release = 0x42U;
        const uint_8 done = 0x43U;
        int_32 worker;

        alias[0] = waiting;
        worker = raw_syscall0(SYS_FORK);
        if (worker == 0) {
                int_32 grandchild = raw_syscall0(SYS_FORK);

                if (grandchild == 0) {
                        while (alias[0] != release)
                                __asm__ volatile("pause");
                        bool passed = vm_refs_are(2);

                        alias[0] = done;
                        process_exit(passed ? 52 : 53);
                }
                process_exit(grandchild > 0 ? 51 : 54);
        }

        bool worker_exit = wait_for_status(worker, 51) && vm_refs_are(2);
        alias[0] = release;
        int_32 status = 0;
        int_32 reparented = raw_syscall1(SYS_WAIT, (uint_32) &status);
        bool grandchild_exit = reparented > 0 && status == 52 &&
                               alias[0] == done && vm_refs_are(1);

        return worker > 0 && worker_exit && grandchild_exit;
}

static void run_vm_lifecycle_checks(volatile uint_8 *alias)
{
        const uint_8 parent_value = 0x31U;
        const uint_8 child_value = 0x32U;
        int_32 child;

        report(FROG_TEST_VM_FORK_ROLLBACK, test_vm_fork_rollback());

        alias[0] = parent_value;
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                bool passed = vm_refs_are(2) && alias[0] == parent_value;

                alias[0] = child_value;
                process_exit(passed ? 37 : 38);
        }
        bool shared = wait_for_status(child, 37) &&
                      alias[0] == child_value;
        bool child_first = vm_refs_are(1);
        report(FROG_TEST_VM_FORK_SHARED, shared);
        report(FROG_TEST_VM_EXIT_CHILD_FIRST,
               child > 0 && child_first);

        report(FROG_TEST_VM_EXIT_PARENT_FIRST,
               test_vm_parent_first(alias));

        alias[0] = FROG_TEST_VM_WRITTEN;
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                if (raw_syscall2(SYS_MUNMAP, (uint_32) alias,
                                 FROG_TEST_VM_LENGTH) != 0)
                        process_exit(39);
                alias[0] = 0xffU;
                process_exit(40);
        }
        bool post_unmap = wait_for_status(child, -EFAULT) &&
                          vm_refs_are(1) &&
                          alias[0] == FROG_TEST_VM_WRITTEN;
        report(FROG_TEST_VM_POST_UNMAP_FAULT, post_unmap);
}

static int_32 vm_mmap_raw(struct frog_mmap_args *args)
{
        return raw_syscall1(SYS_MMAP, (uint_32) args);
}

static bool run_vm_mmap_negative_checks(int_32 fixture_fd)
{
        static const char input_path[] = "/dev/input/event0";
        struct frog_mmap_args args = {
            .addr = 0,
            .length = FROG_TEST_VM_LENGTH,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_SHARED,
            .fd = fixture_fd,
            .offset = 0,
        };
        bool pointer_ok =
            raw_syscall1(SYS_MMAP, 0) == -EFAULT &&
            raw_syscall1(SYS_MMAP, 0x08048ff8U) == -EFAULT;
        report(FROG_TEST_VM_MMAP_POINTER, pointer_ok);

        args.addr = 4096U;
        bool arguments_ok = vm_mmap_raw(&args) == -EINVAL;
        args.addr = 0;
        args.length = 0;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.length = FROG_TEST_VM_LENGTH + 1U;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.length = 2U * 4096U;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.length = 16U * 1024U * 1024U + 4096U;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.length = FROG_TEST_VM_LENGTH;
        args.prot = PROT_READ;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.prot = PROT_READ | PROT_WRITE;
        args.offset = 4096U;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        args.offset = 0;
        args.flags = 0x80000000U;
        arguments_ok = vm_mmap_raw(&args) == -EINVAL && arguments_ok;
        report(FROG_TEST_VM_MMAP_ARGUMENTS, arguments_ok);

        args.flags = MAP_SHARED;
        args.fd = -1;
        bool fd_ok = vm_mmap_raw(&args) == -EBADF;
        report(FROG_TEST_VM_MMAP_FD, fd_ok);

        args.fd = fixture_fd;
        args.flags = MAP_PRIVATE;
        bool unsupported_ok = vm_mmap_raw(&args) == -EOPNOTSUPP;
        args.flags = MAP_SHARED | MAP_FIXED;
        unsupported_ok = vm_mmap_raw(&args) == -EOPNOTSUPP &&
                         unsupported_ok;
        args.flags = MAP_ANONYMOUS;
        unsupported_ok = vm_mmap_raw(&args) == -EOPNOTSUPP &&
                         unsupported_ok;

        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) input_path, O_RDONLY);
        args.flags = MAP_SHARED;
        args.fd = fd;
        bool access_ok = fd >= 0 && vm_mmap_raw(&args) == -EACCES &&
                         raw_syscall1(SYS_CLOSE, fd) == 0;
        report(FROG_TEST_VM_MMAP_ACCESS, access_ok);

        fd = raw_syscall2(SYS_OPEN, (uint_32) input_path, O_RDWR);
        args.fd = fd;
        unsupported_ok = fd >= 0 &&
                         vm_mmap_raw(&args) == -EOPNOTSUPP &&
                         raw_syscall1(SYS_CLOSE, fd) == 0 &&
                         unsupported_ok;
        report(FROG_TEST_VM_MMAP_UNSUPPORTED, unsupported_ok);
        return pointer_ok && arguments_ok && fd_ok && access_ok &&
               unsupported_ok;
}

static bool run_vm_mapping_checks(void)
{
        int_32 fd = raw_syscall1(SYS_TESTSYSCALL, FROG_TEST_VM_PREPARE);
        struct frog_mmap_args args = {
            .addr = 0,
            .length = FROG_TEST_VM_LENGTH,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_SHARED,
            .fd = fd,
            .offset = 0,
        };
        bool negative_ok = fd >= 0 && run_vm_mmap_negative_checks(fd);
        int_32 mapped = fd >= 0 ? vm_mmap_raw(&args) : -1;
        bool address_ok = mapped == (int_32) FROG_TEST_VM_EXPECTED_ADDR &&
                          vm_refs_are(1);
        bool read_ok = false;
        bool write_ok = false;

        if (address_ok) {
                volatile uint_8 *alias = (volatile uint_8 *) mapped;

                read_ok = alias[0] == FROG_TEST_VM_SEED;
                alias[0] = FROG_TEST_VM_WRITTEN;
                write_ok = alias[0] == FROG_TEST_VM_WRITTEN;
        }
        report(FROG_TEST_VM_USER_ADDRESS, address_ok);
        report(FROG_TEST_VM_USER_READ, read_ok);
        report(FROG_TEST_VM_USER_WRITE, write_ok);

        bool busy_ok = address_ok && vm_mmap_raw(&args) == -EBUSY &&
                       vm_refs_are(1);
        report(FROG_TEST_VM_MMAP_BUSY, busy_ok);

        if (address_ok && read_ok && write_ok)
                run_vm_lifecycle_checks((volatile uint_8 *) mapped);
        else {
                report(FROG_TEST_VM_FORK_SHARED, false);
                report(FROG_TEST_VM_EXIT_CHILD_FIRST, false);
                report(FROG_TEST_VM_FORK_ROLLBACK, false);
                report(FROG_TEST_VM_EXIT_PARENT_FIRST, false);
                report(FROG_TEST_VM_POST_UNMAP_FAULT, false);
        }

        uint_8 closed_read = 0;
        bool file_after_close = fd >= 0 &&
            raw_syscall1(SYS_CLOSE, fd) == 0 &&
            raw_syscall3(SYS_READ, fd, (uint_32) &closed_read, 1) == -EBADF;
        if (address_ok) {
                volatile uint_8 *alias = (volatile uint_8 *) mapped;

                alias[0] = FROG_TEST_VM_WRITTEN;
                file_after_close = alias[0] == FROG_TEST_VM_WRITTEN &&
                                   file_after_close;
        }
        report(FROG_TEST_VM_FILE_AFTER_CLOSE, file_after_close);

        bool exact_ok = false;
        if (address_ok) {
                exact_ok =
                    raw_syscall2(SYS_MUNMAP, mapped,
                                 2U * 4096U) == -EINVAL &&
                    raw_syscall2(SYS_MUNMAP, mapped + 4096U,
                                 4096U) == -EINVAL &&
                    raw_syscall2(SYS_MUNMAP, mapped + 4096U,
                                 2U * 4096U) == -EINVAL &&
                    *(volatile uint_8 *) mapped ==
                        FROG_TEST_VM_WRITTEN &&
                    raw_syscall2(SYS_MUNMAP, mapped,
                                 FROG_TEST_VM_LENGTH) == 0;
        }
        report(FROG_TEST_VM_MUNMAP_EXACT, exact_ok);

        bool cleanup_ok =
            raw_syscall1(SYS_TESTSYSCALL,
                         FROG_TEST_VM_VERIFY_CLEANUP) == 0;
        report(FROG_TEST_VM_CLEANUP, cleanup_ok);
        return negative_ok && address_ok && read_ok && write_ok && busy_ok &&
               file_after_close && exact_ok && cleanup_ok;
}

static void run_profile(void)
{
        volatile int private_value = 7;
        (void) run_basic_checks();
        pid_t child_pid = fork();

        if (child_pid == 0) {
                private_value = 19;
                process_exit(37);
        }

        report(FROG_TEST_PROCESS_FORK_PARENT_RESULT,
               child_pid != -1 && child_pid != 0);
        if (child_pid != -1) {
                int_32 status = -1;
                pid_t waited_pid = wait(&status);

                report(FROG_TEST_PROCESS_WAIT_PID,
                       waited_pid == (int_32) child_pid);
                report(FROG_TEST_PROCESS_WAIT_STATUS, status == 37);
                report(FROG_TEST_PROCESS_FORK_ADDRESS_SPACE,
                       private_value == 7);
        } else {
                report(FROG_TEST_PROCESS_WAIT_PID, false);
                report(FROG_TEST_PROCESS_WAIT_STATUS, false);
                report(FROG_TEST_PROCESS_FORK_ADDRESS_SPACE, false);
        }
        report(FROG_TEST_PROCESS_WAIT_FAULT_RETRY,
               test_wait_fault_retry());
        report(FROG_TEST_PROCESS_ZOMBIE_ADOPTION,
               test_zombie_adoption());
        report(FROG_TEST_PROCESS_HEAP_FORK, test_user_heap_fork());
        report(FROG_TEST_PROCESS_WAIT_NO_CHILD,
               wait(NULL) == -1);
        (void) run_vm_mapping_checks();
        finish(1);
}
#elif defined(USER_SMOKE_DISK_PREPARE)
static bool wait_for_child(pid_t expected_pid)
{
        int_32 status = -1;
        int_32 waited = raw_syscall1(SYS_WAIT, (uint_32) &status);

        return waited == (int_32) expected_pid && status == 0;
}

static bool wait_for_child_status(pid_t expected_pid, int_32 expected_status)
{
        int_32 status = -1;

        return expected_pid > 0 && wait(&status) == expected_pid &&
               status == expected_status;
}

static bool reset_and_read_q(int_32 fd)
{
        char value = 0;

        return lseek(fd, 0, USER_SEEK_SET) == 0 &&
               read(fd, &value, 1) == 1 && value == 'Q';
}

static bool test_exec_failpoint_lifecycle(void)
{
        pid_t owner = fork();

        if (owner == 0) {
                bool armed = raw_syscall1(
                    SYS_TESTSYSCALL,
                    FROG_TEST_EXEC_FAIL_PRECOMMIT) == 0;
                pid_t child = armed ? fork() : -1;

                if (child == 0) {
                        bool cleared = raw_syscall1(
                            SYS_TESTSYSCALL,
                            FROG_TEST_EXEC_FAIL_PRECOMMIT) == 0;
                        exit(cleared ? FROG_TEST_EXEC_ARM_EXIT_STATUS : 94);
                        for (;;)
                                __asm__ volatile("pause");
                }
                bool child_cleared = wait_for_child_status(
                    child, FROG_TEST_EXEC_ARM_EXIT_STATUS);
                exit(child_cleared ? FROG_TEST_EXEC_ARM_EXIT_STATUS : 95);
                for (;;)
                        __asm__ volatile("pause");
        }
        return wait_for_child_status(owner,
                                     FROG_TEST_EXEC_ARM_EXIT_STATUS);
}

static void run_exec_checks(void)
{
        static const char target[] = "/test/exec-target";
        static const char input_path[] = "/test/exec-input";
        static const char invalid_path[] = "/test/not-elf";
        static const char input[] = "Q";
        static const char invalid[] = "not-elf";
        static const char arg0[] = "exec-target";
        static const char arg1[] = "alpha";
        static const char arg2[] = "beta";
        static const char overflow_value[] = "x";
        const char *valid_argv[] = {arg0, arg1, arg2, NULL};
        const char *overflow_argv[34];
        int_32 input_fd = -1;
        int_32 invalid_fd = -1;
        bool cleanup_ok = true;

        report(FROG_TEST_EXEC_BAD_PATH,
               execv("/test/missing-exec", valid_argv) == -ENOENT);
        bool bad_pointers =
            execv((const char *) 0xc0000000U, valid_argv) == -EFAULT &&
            execv(target, (const char **) 0xc0000000U) == -EFAULT;
        report(FROG_TEST_EXEC_BAD_POINTERS, bad_pointers);

        for (uint_32 index = 0; index < 33; index++)
                overflow_argv[index] = overflow_value;
        overflow_argv[33] = NULL;
        report(FROG_TEST_EXEC_ARG_OVERFLOW,
               execv(target, overflow_argv) == -E2BIG);

        invalid_fd = open(invalid_path, O_CREAT | O_EXCL | O_WRONLY);
        bool invalid_elf = invalid_fd >= 0 &&
            write(invalid_fd, invalid, sizeof(invalid) - 1U) ==
                (int_32) sizeof(invalid) - 1 &&
            close(invalid_fd) == 0;
        invalid_fd = -1;
        invalid_elf = invalid_elf &&
                      execv(invalid_path, valid_argv) == -ENOEXEC;
        report(FROG_TEST_EXEC_INVALID_ELF, invalid_elf);

        input_fd = open(input_path, O_CREAT | O_EXCL | O_RDWR);
        bool input_ready = input_fd == 0 &&
                           write(input_fd, input, 1) == 1 &&
                           lseek(input_fd, 0, USER_SEEK_SET) == 0;
        pid_t failed_child = input_ready ? fork() : -1;

        if (failed_child == 0) {
                volatile uint_32 canary = 0x4f4c444dU;
                bool armed = raw_syscall1(
                    SYS_TESTSYSCALL,
                    FROG_TEST_EXEC_FAIL_PRECOMMIT) == 0;
                int_32 result = execv(target, valid_argv);
                char inherited = 0;
                bool preserved = armed && result == -ENOMEM &&
                                 canary == 0x4f4c444dU &&
                                 read(input_fd, &inherited, 1) == 1 &&
                                 inherited == 'Q';

                exit(preserved ? FROG_TEST_EXEC_FAIL_STATUS : 92);
                for (;;)
                        __asm__ volatile("pause");
        }

        bool failure_preserved = input_ready &&
            wait_for_child_status(failed_child,
                                  FROG_TEST_EXEC_FAIL_STATUS) &&
            reset_and_read_q(input_fd);
        report(FROG_TEST_EXEC_FAILURE_PRESERVES, failure_preserved);

        bool reset = failure_preserved &&
                     lseek(input_fd, 0, USER_SEEK_SET) == 0;
        bool lifecycle_ok = reset && test_exec_failpoint_lifecycle();
        pid_t exec_child = lifecycle_ok ? fork() : -1;
        if (exec_child == 0) {
                (void) execv(target, valid_argv);
                exit(93);
                for (;;)
                        __asm__ volatile("pause");
        }
        bool exec_success = lifecycle_ok &&
            wait_for_child_status(exec_child,
                                  FROG_TEST_EXEC_TARGET_STATUS);
        report(FROG_TEST_EXEC_SUCCESS, exec_success);

        bool fd_inherit = exec_success && reset_and_read_q(input_fd);
        if (input_fd >= 0)
                cleanup_ok = close(input_fd) == 0 && cleanup_ok;
        if (invalid_fd >= 0)
                cleanup_ok = close(invalid_fd) == 0 && cleanup_ok;
        cleanup_ok = unlink(input_path) == 0 && cleanup_ok;
        cleanup_ok = unlink(invalid_path) == 0 && cleanup_ok;
        cleanup_ok = unlink(target) == 0 && cleanup_ok;
        report(FROG_TEST_EXEC_FD_INHERIT, fd_inherit && cleanup_ok);
}

static bool test_child_close_parent_uses_fd(void)
{
        static const char path[] = "/test/forka";
        static const char value[] = "A";
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) path,
                                 O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return false;
        bool passed = fd == 0 &&
                      raw_syscall3(SYS_WRITE, fd, (uint_32) value, 1) == 1 &&
                      raw_syscall3(SYS_SEEK, fd, 0, USER_SEEK_SET) == 0;
        pid_t pid = raw_syscall0(SYS_FORK);

        if (pid == 0) {
                bool child_ok = raw_syscall1(SYS_CLOSE, fd) == 0;
                raw_syscall1(SYS_EXIT, child_ok ? 0 : 1);
                for (;;)
                        __asm__ volatile("pause");
        }
        if (pid == -1) {
                passed = false;
        } else {
                char read_value = 0;
                passed = wait_for_child(pid) && passed;
                passed = raw_syscall3(SYS_READ, fd,
                                      (uint_32) &read_value, 1) == 1 &&
                         read_value == 'A' && passed;
        }
        passed = raw_syscall1(SYS_CLOSE, fd) == 0 && passed;
        return raw_syscall1(SYS_UNLINK, (uint_32) path) == 0 && passed;
}

static bool test_parent_close_child_uses_fd(void)
{
        static const char path[] = "/test/forkb";
        static const char value[] = "B";
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) path,
                                 O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return false;
        bool passed = fd == 0 &&
                      raw_syscall3(SYS_WRITE, fd, (uint_32) value, 1) == 1 &&
                      raw_syscall3(SYS_SEEK, fd, 0, USER_SEEK_SET) == 0;
        pid_t pid = raw_syscall0(SYS_FORK);

        if (pid == 0) {
                char read_value = 0;
                bool child_ok = raw_syscall3(SYS_READ, fd,
                                             (uint_32) &read_value, 1) == 1 &&
                                read_value == 'B';
                child_ok = raw_syscall1(SYS_CLOSE, fd) == 0 && child_ok;
                raw_syscall1(SYS_EXIT, child_ok ? 0 : 1);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = raw_syscall1(SYS_CLOSE, fd) == 0 && passed;
        if (pid == -1)
                passed = false;
        else
                passed = wait_for_child(pid) && passed;
        return raw_syscall1(SYS_UNLINK, (uint_32) path) == 0 && passed;
}

static void run_profile(void)
{
        (void) run_basic_checks();
        report(FROG_TEST_FD_FORK_CHILD_CLOSE,
               test_child_close_parent_uses_fd());
        report(FROG_TEST_FD_FORK_PARENT_CLOSE,
               test_parent_close_child_uses_fd());
        run_exec_checks();
        finish(1);
}
#else
static void run_profile(void)
{
        finish(run_basic_checks() ? USER_SMOKE_MAGIC : 0);
}
#endif

void _start(void) __attribute__((noreturn, section(".text._start")));

void _start(void)
{
        run_profile();
        for (;;)
                __asm__ volatile("pause");
}
