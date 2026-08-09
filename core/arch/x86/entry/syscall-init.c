/* #include <debug.h> */
/* #include <frog/types.h> */
/* #include <print.h> */
/* #include <stdio.h> */
/* #include <string.h> */

#include <frog/syscall-init.h>
#include <frog/syscall.h>
#include <frog/exit.h>
#include <frog/fork.h>
#include <frog/errno.h>
#include <frog/exec.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/test.h>
#include <kernel/debug.h>
#include <kernel/framebuffer.h>
#include <kernel/mm_test.h>
#include <kernel/syscall_fs.h>
#include <kernel/timekeeping.h>
#include <kernel/qemu_test.h>
#include <kernel/wait2.h>
#include "../../../fs/packagefs/packagefs.h"

/* #include <fs/fs.h>  // for sys_write/ sys_open/ sys_close */
/* #include <sys/exec.h> */
/* #include <sys/fork.h> */
/* #include <sys/exit.h> */
/* #include <sys/graphic.h> */
/* #include <sys/threads.h> */
/* #include <fs/pipe.h> */
/* #include <device/cmos.h> */
/* #include <fs/select.h> */
/* #include <ipc.h> */

// for test
/* #include <math.h> */
/* #include <sys/memory.h> */

typedef void *syscall;

syscall syscall_table[SYS_NR_COUNT];
uint_32 syscall_table_size = SYS_NR_COUNT;

static int_32 sys_ni_syscall(void)
{
    return -ENOSYS;
}

static int_32 sys_putc(uint_32 value)
{
    char text[2] = {(char) value, '\0'};

    printk("%s", text);
    return 0;
}

static int_32 sys_test_sync(uint_32 command)
{
#ifdef CONFIG_FROG_TEST_FRAMEBUFFER_MMAP
    unsigned long entry_flags;
    int result;

    local_irq_save(entry_flags);
    local_irq_enable();
    result = pc_framebuffer_test_verify_cleanup();

    if (result != 0) {
        frog_test_case("framebuffer.mmap.cleanup", 0);
        local_irq_restore(entry_flags);
        return result;
    }
    if (frog_test_has_failures()) {
        local_irq_restore(entry_flags);
        return -EUCLEAN;
    }

    frog_test_sync("framebuffer-mmap-ready");
    local_irq_disable();
    for (;;)
        __asm__ volatile("hlt");
#elif defined(CONFIG_FROG_TEST_DESKTOP)
    switch (command) {
    case FROG_TEST_DESKTOP_INITIAL_READY:
        frog_test_sync("desktop-initial-ready");
        return 0;
    case FROG_TEST_DESKTOP_FOCUS_READY:
        frog_test_sync("desktop-focus-ready");
        return 0;
    case FROG_TEST_DESKTOP_KEYBOARD_READY:
        frog_test_sync("desktop-keyboard-ready");
        return 0;
    case FROG_TEST_DESKTOP_DRAG_READY:
        frog_test_sync("desktop-drag-ready");
        return 0;
    case FROG_TEST_DESKTOP_FINAL_READY:
        if (frog_test_has_failures())
            return -EUCLEAN;
        frog_test_sync("desktop-final-frame");
        return 0;
    case FROG_TEST_DESKTOP_CLIENT_READY:
        if (frog_test_has_failures())
            return -EUCLEAN;
        frog_test_sync("desktop-client-observed");
        return 0;
    case FROG_TEST_DESKTOP_IDLE_READY:
        if (frog_test_has_failures())
            return -EUCLEAN;
        frog_test_sync("desktop-idle-stable");
        return 0;
    default:
        return -EINVAL;
    }
#elif defined(CONFIG_FROG_TEST_POUDLAND_E2E)
    if (command != FROG_TEST_POUDLAND_E2E_DESKTOP_LAUNCHED)
        return -EINVAL;
    frog_test_sync("poudland-e2e-desktop-launched");
    return 0;
#elif defined(CONFIG_FROG_TEST_POUDLAND_BUILTIN)
    switch (command) {
    case FROG_TEST_POUDLAND_BUILTIN_INITIAL_READY:
        frog_test_sync("poudland-builtin-initial-ready");
        return 0;
    case FROG_TEST_POUDLAND_BUILTIN_FOCUS_READY:
        frog_test_sync("poudland-builtin-focus-ready");
        return 0;
    case FROG_TEST_POUDLAND_BUILTIN_KEYBOARD_READY:
        frog_test_sync("poudland-builtin-keyboard-ready");
        return 0;
    case FROG_TEST_POUDLAND_BUILTIN_DRAG_READY:
        frog_test_sync("poudland-builtin-drag-ready");
        return 0;
    case FROG_TEST_POUDLAND_BUILTIN_FINAL_READY:
        if (frog_test_has_failures())
            return -EUCLEAN;
        frog_test_sync("poudland-builtin-final-ready");
        local_irq_disable();
        for (;;)
            __asm__ volatile("hlt");
    default:
        return -EINVAL;
    }
#elif defined(CONFIG_FROG_TEST_INPUT)
    switch (command) {
    case FROG_TEST_INPUT_KEYBOARD_READY:
        frog_test_sync("input-keyboard-ready");
        return 0;
    case FROG_TEST_INPUT_MOUSE_MOVE_READY:
        frog_test_sync("input-mouse-move-ready");
        return 0;
    case FROG_TEST_INPUT_MOUSE_BUTTON_READY:
        frog_test_sync("input-mouse-button-ready");
        return 0;
    case FROG_TEST_INPUT_BOTH_READY:
        frog_test_sync("input-both-ready");
        return 0;
    default:
        return -EINVAL;
    }
#elif defined(CONFIG_FROG_TEST_WAIT2)
    return wait2_test_command(command);
#else
    (void) command;
    return -EOPNOTSUPP;
#endif
}

#ifdef CONFIG_QEMU_TEST
static int_32 sys_test_report(uint_32 id, int_32 passed)
{
    const char *name;

    switch (id) {
    case FROG_TEST_PROCESS_FORK_PARENT_RESULT:
        name = "process.fork.parent-result";
        break;
    case FROG_TEST_PROCESS_WAIT_PID:
        name = "process.wait.pid";
        break;
    case FROG_TEST_PROCESS_WAIT_STATUS:
        name = "process.wait.status";
        break;
    case FROG_TEST_PROCESS_FORK_ADDRESS_SPACE:
        name = "process.fork.address-space";
        break;
    case FROG_TEST_PROCESS_WAIT_NO_CHILD:
        name = "process.wait.no-child";
        break;
    case FROG_TEST_FD_FORK_CHILD_CLOSE:
        name = "fd.fork-child-close";
        break;
    case FROG_TEST_FD_FORK_PARENT_CLOSE:
        name = "fd.fork-parent-close";
        break;
    case FROG_TEST_SYSCALL_OUT_OF_RANGE:
        name = "syscall.out-of-range";
        break;
    case FROG_TEST_SYSCALL_UNIMPLEMENTED:
        name = "syscall.unimplemented";
        break;
    case FROG_TEST_VM_USER_ADDRESS:
        name = "vm.user-address";
        break;
    case FROG_TEST_VM_USER_READ:
        name = "vm.user-read";
        break;
    case FROG_TEST_VM_USER_WRITE:
        name = "vm.user-write";
        break;
    case FROG_TEST_VM_CLEANUP:
        name = "vm.cleanup";
        break;
    case FROG_TEST_VM_FORK_ROLLBACK:
        name = "vm.fork-rollback";
        break;
    case FROG_TEST_VM_FORK_SHARED:
        name = "vm.fork-shared";
        break;
    case FROG_TEST_VM_EXIT_CHILD_FIRST:
        name = "vm.exit-child-first";
        break;
    case FROG_TEST_VM_EXIT_PARENT_FIRST:
        name = "vm.exit-parent-first";
        break;
    case FROG_TEST_VM_POST_UNMAP_FAULT:
        name = "vm.post-unmap-fault";
        break;
    case FROG_TEST_VM_MMAP_POINTER:
        name = "vm.mmap-pointer";
        break;
    case FROG_TEST_VM_MMAP_ARGUMENTS:
        name = "vm.mmap-arguments";
        break;
    case FROG_TEST_VM_MMAP_FD:
        name = "vm.mmap-fd";
        break;
    case FROG_TEST_VM_MMAP_ACCESS:
        name = "vm.mmap-access";
        break;
    case FROG_TEST_VM_MMAP_UNSUPPORTED:
        name = "vm.mmap-unsupported";
        break;
    case FROG_TEST_VM_MUNMAP_EXACT:
        name = "vm.munmap-exact";
        break;
    case FROG_TEST_VM_FILE_AFTER_CLOSE:
        name = "vm.file-after-close";
        break;
    case FROG_TEST_VM_MMAP_BUSY:
        name = "vm.mmap-busy";
        break;
    case FROG_TEST_PROCESS_WAIT_FAULT_RETRY:
        name = "process.wait-fault-retry";
        break;
    case FROG_TEST_PROCESS_ZOMBIE_ADOPTION:
        name = "process.zombie-adoption";
        break;
    case FROG_TEST_PROCESS_HEAP_FORK:
        name = "process.heap-fork";
        break;
    case FROG_TEST_EXEC_BAD_PATH:
        name = "exec.bad-path";
        break;
    case FROG_TEST_EXEC_BAD_POINTERS:
        name = "exec.bad-pointers";
        break;
    case FROG_TEST_EXEC_ARG_OVERFLOW:
        name = "exec.arg-overflow";
        break;
    case FROG_TEST_EXEC_INVALID_ELF:
        name = "exec.invalid-elf";
        break;
    case FROG_TEST_EXEC_FAILURE_PRESERVES:
        name = "exec.failure-preserves-image";
        break;
    case FROG_TEST_EXEC_SUCCESS:
        name = "exec.success";
        break;
    case FROG_TEST_EXEC_FD_INHERIT:
        name = "exec.fd-inherit";
        break;
    case FROG_TEST_INPUT_NONBLOCK:
        name = "input.nonblock";
        break;
    case FROG_TEST_INPUT_KEYBOARD:
        name = "input.keyboard";
        break;
    case FROG_TEST_INPUT_MOUSE_MOVE:
        name = "input.mouse-move";
        break;
    case FROG_TEST_INPUT_MOUSE_BUTTON:
        name = "input.mouse-button";
        break;
    case FROG_TEST_INPUT_WAIT2_EMPTY:
        name = "input.wait2.empty";
        break;
    case FROG_TEST_INPUT_WAIT2_KEYBOARD:
        name = "input.wait2.keyboard";
        break;
    case FROG_TEST_INPUT_WAIT2_MOUSE_MOVE:
        name = "input.wait2.mouse-move";
        break;
    case FROG_TEST_INPUT_WAIT2_MOUSE_BUTTON:
        name = "input.wait2.mouse-button";
        break;
    case FROG_TEST_INPUT_WAIT2_BOTH:
        name = "input.wait2.both";
        break;
    case FROG_TEST_INPUT_WAIT2_CLOSE:
        name = "input.wait2.close";
        break;
    case FROG_TEST_ANON_MMAP_SUPPORTED:
        name = "anonymous-mmap.supported";
        break;
    case FROG_TEST_ANON_MMAP_ARGUMENTS:
        name = "anonymous-mmap.arguments";
        break;
    case FROG_TEST_ANON_MMAP_ONE_PAGE:
        name = "anonymous-mmap.one-page";
        break;
    case FROG_TEST_ANON_MMAP_MULTIPAGE:
        name = "anonymous-mmap.multi-page";
        break;
    case FROG_TEST_ANON_MUNMAP_EXACT:
        name = "anonymous-mmap.munmap-exact";
        break;
    case FROG_TEST_ANON_MMAP_FORK_PRIVATE:
        name = "anonymous-mmap.fork-private";
        break;
    case FROG_TEST_ANON_MMAP_EXIT_CLEANUP:
        name = "anonymous-mmap.exit-cleanup";
        break;
    case FROG_TEST_ANON_MMAP_ROLLBACK:
        name = "anonymous-mmap.rollback";
        break;
    case FROG_TEST_ANON_MMAP_FORK_ROLLBACK:
        name = "anonymous-mmap.fork-rollback";
        break;
    case FROG_TEST_ANON_MMAP_MAX_LENGTH:
        name = "anonymous-mmap.max-length";
        break;
    case FROG_TEST_ANON_MMAP_ALLOC_ROLLBACK:
        name = "anonymous-mmap.alloc-rollback";
        break;
    case FROG_TEST_ANON_MMAP_BACKBUFFER:
        name = "anonymous-mmap.backbuffer-3m";
        break;
    case FROG_TEST_ANON_MMAP_EXEC_CLEANUP:
        name = "anonymous-mmap.exec-cleanup";
        break;
    case FROG_TEST_USER_ALLOC_ZERO_NULL:
        name = "user-allocator.zero-null";
        break;
    case FROG_TEST_USER_ALLOC_ALIGNMENT_WRITE:
        name = "user-allocator.alignment-write";
        break;
    case FROG_TEST_USER_ALLOC_SMALL_REUSE:
        name = "user-allocator.small-reuse";
        break;
    case FROG_TEST_USER_ALLOC_ARENA_RELEASE:
        name = "user-allocator.arena-release";
        break;
    case FROG_TEST_USER_ALLOC_LARGE_RELEASE:
        name = "user-allocator.large-release";
        break;
    case FROG_TEST_USER_ALLOC_FORCED_OOM:
        name = "user-allocator.forced-oom";
        break;
    case FROG_TEST_USER_ALLOC_FORK_ISOLATION:
        name = "user-allocator.fork-isolation";
        break;
    case FROG_TEST_USER_ALLOC_EXIT_CLEANUP:
        name = "user-allocator.exit-cleanup";
        break;
    case FROG_TEST_USER_ALLOC_LEGACY_SYSCALLS:
        name = "user-allocator.legacy-syscalls";
        break;
    case FROG_TEST_USER_ALLOC_LIMITS:
        name = "user-allocator.limits";
        break;
    case FROG_TEST_PACKAGEFS_BIND_CONNECT:
        name = "packagefs.bind-connect";
        break;
    case FROG_TEST_PACKAGEFS_DIRECTED_ROUTING:
        name = "packagefs.directed-routing";
        break;
    case FROG_TEST_PACKAGEFS_RECORD_BOUNDARIES:
        name = "packagefs.record-boundaries";
        break;
    case FROG_TEST_PACKAGEFS_READ_PRESERVES_RECORD:
        name = "packagefs.read-preserves-record";
        break;
    case FROG_TEST_PACKAGEFS_INVALID_IO:
        name = "packagefs.invalid-io";
        break;
    case FROG_TEST_PACKAGEFS_OPEN_CONTRACT:
        name = "packagefs.open-contract";
        break;
    case FROG_TEST_PACKAGEFS_STALE_GENERATION:
        name = "packagefs.stale-generation";
        break;
    case FROG_TEST_PACKAGEFS_LIMITS:
        name = "packagefs.limits";
        break;
    case FROG_TEST_PACKAGEFS_BLOCKING_WAKE:
        name = "packagefs.blocking-wake";
        break;
    case FROG_TEST_PACKAGEFS_BACKPRESSURE:
        name = "packagefs.backpressure";
        break;
    case FROG_TEST_PACKAGEFS_CLOEXEC:
        name = "packagefs.cloexec";
        break;
    case FROG_TEST_PACKAGEFS_DATA_DISCONNECT_ORDER:
        name = "packagefs-lifecycle.data-disconnect-order";
        break;
    case FROG_TEST_PACKAGEFS_DISCONNECT_EXACTLY_ONCE:
        name = "packagefs-lifecycle.disconnect-exactly-once";
        break;
    case FROG_TEST_PACKAGEFS_BAD_CONTROL_READ_PRESERVES:
        name = "packagefs-lifecycle.bad-control-read-preserves";
        break;
    case FROG_TEST_PACKAGEFS_TOMBSTONE_LIMIT:
        name = "packagefs-lifecycle.tombstone-limit";
        break;
    case FROG_TEST_PACKAGEFS_SERVER_CLOSE_DRAIN:
        name = "packagefs-lifecycle.server-close-drain-hup-epipe";
        break;
    case FROG_TEST_PACKAGEFS_WRITABLE_ARM_COALESCE:
        name = "packagefs-lifecycle.writable-arm-coalesce-correct-peer";
        break;
    case FROG_TEST_PACKAGEFS_LIFECYCLE_WAIT2_MASKS:
        name = "packagefs-lifecycle.wait2-masks";
        break;
    case FROG_TEST_PACKAGEFS_FORK_FINAL_REF:
        name = "packagefs-lifecycle.fork-final-ref";
        break;
    case FROG_TEST_PACKAGEFS_PROCESS_EXIT:
        name = "packagefs-lifecycle.process-exit";
        break;
    case FROG_TEST_PACKAGEFS_BLOCKED_CLOSE_RACES:
        name = "packagefs-lifecycle.blocked-close-races";
        break;
    case FROG_TEST_PACKAGEFS_REBIND_LEAK_BASELINE:
        name = "packagefs-lifecycle.rebind-repeat-leak-baseline";
        break;
    case FROG_TEST_PACKAGEFS_CONTROL_NOT_STARVED:
        name = "packagefs-lifecycle.control-not-starved-by-data";
        break;
    case FROG_TEST_PACKAGEFS_USERLIB_BIND_CONNECT:
        name = "packagefs-userlib.bind-connect";
        break;
    case FROG_TEST_PACKAGEFS_USERLIB_EXACT_RECORDS:
        name = "packagefs-userlib.zero-one-max-exact-records";
        break;
    case FROG_TEST_PACKAGEFS_USERLIB_DIRECTED_REPLY:
        name = "packagefs-userlib.directed-reply";
        break;
    case FROG_TEST_PACKAGEFS_USERLIB_CONTROL_ERRNO:
        name = "packagefs-userlib.control-and-negative-errno";
        break;
    case FROG_TEST_PACKAGEFS_USERLIB_BROADCAST_RESULTS:
        name = "packagefs-userlib.broadcast-independent-results";
        break;
    case FROG_TEST_POUDLAND_V1_CONNECT_HANDSHAKE:
        name = "poudland-v1.connect-delayed-bind-handshake";
        break;
    case FROG_TEST_POUDLAND_V1_WINDOW_LIFECYCLE:
        name = "poudland-v1.window-create-success";
        break;
    case FROG_TEST_POUDLAND_V1_CONNECT_ERRNO:
        name = "poudland-v1.connect-non-enoent-not-retried";
        break;
    case FROG_TEST_POUDLAND_V1_INCOMPATIBLE_VERSION:
        name = "poudland-v1.incompatible-version";
        break;
    case FROG_TEST_POUDLAND_V1_ERROR_RESPONSE:
        name = "poudland-v1.error-response-consumes-pending";
        break;
    case FROG_TEST_POUDLAND_V1_WRAPPER_TIMEOUT_RESET:
        name = "poudland-v1.wrapper-timeout-resets-context";
        break;
    case FROG_TEST_POUDLAND_V1_ID_WRAP_SKIP:
        name = "poudland-v1.request-id-wrap-skip-pending";
        break;
    case FROG_TEST_POUDLAND_V1_REORDERED_RESPONSE_EVENT:
        name = "poudland-v1.reordered-response-and-event";
        break;
    case FROG_TEST_POUDLAND_V1_ERROR_TIMEOUT_REWAIT:
        name = "poudland-v1.error-and-timeout-rewait";
        break;
    case FROG_TEST_POUDLAND_V1_WINDOW_CLOSE:
        name = "poudland-v1.window-close-success";
        break;
    case FROG_TEST_POUDLAND_V1_MALFORMED_FRAME:
        name = "poudland-v1.malformed-frame-fatal";
        break;
    case FROG_TEST_POUDLAND_V1_ROUTING_FATAL:
        name = "poudland-v1.unknown-id-and-wrong-type-fatal";
        break;
    case FROG_TEST_POUDLAND_V1_INBOX_OVERFLOW:
        name = "poudland-v1.ninth-inbox-item-overflow-fatal";
        break;
    case FROG_TEST_POUDLAND_V1_HUP_DRAIN:
        name = "poudland-v1.pollin-hup-drains-response-first";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_EXEC:
        name = "poudland-builtin.exec-multipage-elf";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_FRAMEBUFFER:
        name = "poudland-builtin.framebuffer-open-map";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_BMP_CURSOR:
        name = "poudland-builtin.bmp-cursor-stream";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_FORMATS:
        name = "poudland-builtin.xrgb-8-16-32";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_INITIAL_DAMAGE:
        name = "poudland-builtin.initial-frame-damage-exact";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_IDLE:
        name = "poudland-builtin.idle-500ms-quiescent";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_FOCUS:
        name = "poudland-builtin.focus-second-window";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_KEYBOARD:
        name = "poudland-builtin.keyboard-to-focus";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_DRAG:
        name = "poudland-builtin.drag-whole-window";
        break;
    case FROG_TEST_POUDLAND_BUILTIN_FINAL_DAMAGE:
        name = "poudland-builtin.final-frame-damage-exact";
        break;
    case FROG_TEST_FROGFS_EXEC_COMPOSITOR_SIZE:
        name = "frogfs-exec.compositor-multipage-file";
        break;
    case FROG_TEST_FROGFS_EXEC_COMPOSITOR:
        name = "frogfs-exec.compositor-exec-wait";
        break;
    case FROG_TEST_FROGFS_EXEC_DESKTOP_SIZE:
        name = "frogfs-exec.desktop-multipage-file";
        break;
    case FROG_TEST_FROGFS_EXEC_DESKTOP:
        name = "frogfs-exec.desktop-exec-wait";
        break;
    case FROG_TEST_POUDLAND_E2E_HARNESS_EXEC:
        name = "poudland-e2e.harness-exec";
        break;
    case FROG_TEST_POUDLAND_E2E_COMPOSITOR_FORK:
        name = "poudland-e2e.compositor-fork";
        break;
    case FROG_TEST_POUDLAND_E2E_CLIENT_A_CONNECT:
        name = "poudland-e2e.client-a-connect";
        break;
    case FROG_TEST_POUDLAND_E2E_THREE_WINDOWS:
        name = "poudland-e2e.three-window-create";
        break;
    case FROG_TEST_POUDLAND_E2E_CLOSE_THIRD:
        name = "poudland-e2e.close-third";
        break;
    case FROG_TEST_POUDLAND_E2E_CLOSE_STALE:
        name = "poudland-e2e.close-stale-enoent";
        break;
    case FROG_TEST_POUDLAND_E2E_CLIENT_B_CONNECT:
        name = "poudland-e2e.client-b-connect";
        break;
    case FROG_TEST_POUDLAND_E2E_FOREIGN_CLOSE:
        name = "poudland-e2e.foreign-close-eperm";
        break;
    case FROG_TEST_POUDLAND_E2E_INVALID_GEOMETRY:
        name = "poudland-e2e.invalid-geometry-einval";
        break;
    case FROG_TEST_POUDLAND_E2E_UNKNOWN_TYPE:
        name = "poudland-e2e.unknown-type-eproto";
        break;
    case FROG_TEST_POUDLAND_E2E_DISCONNECT:
        name = "poudland-e2e.disconnect-clients";
        break;
    case FROG_TEST_POUDLAND_E2E_DESKTOP_FORK:
        name = "poudland-e2e.desktop-fork";
        break;
    case FROG_TEST_POUDLAND_E2E_DESKTOP_SYNC:
        name = "poudland-e2e.desktop-launch-sync";
        break;
    case FROG_TEST_POUDLAND_E2E_HOLD:
        name = "poudland-e2e.hold-one-second";
        break;
    case FROG_TEST_POUDLAND_E2E_SESSION_LIMIT:
        name = "poudland-e2e.session-limit";
        break;
    case FROG_TEST_POUDLAND_E2E_SERVER_LIMIT:
        name = "poudland-e2e.server-limit";
        break;
    case FROG_TEST_POUDLAND_E2E_DISCONNECT_CLEANUP:
        name = "poudland-e2e.disconnect-cleanup";
        break;
    case FROG_TEST_POUDLAND_E2E_DESKTOP_TWO_LIVE:
        name = "poudland-e2e.desktop-two-live";
        break;
    case FROG_TEST_DESKTOP_INIT_COMPOSITOR_FORK:
        name = "desktop.init-compositor-fork";
        break;
    case FROG_TEST_DESKTOP_INIT_DESKTOP_FORK:
        name = "desktop.init-desktop-fork";
        break;
    case FROG_TEST_DESKTOP_COMPOSITOR_EXEC:
        name = "desktop.compositor-exec";
        break;
    case FROG_TEST_DESKTOP_COMPOSITOR_READY:
        name = "desktop.compositor-ready";
        break;
    case FROG_TEST_DESKTOP_CLIENT_CONNECT:
        name = "desktop.handshake";
        break;
    case FROG_TEST_DESKTOP_TWO_WINDOWS:
        name = "desktop.two-live-windows";
        break;
    case FROG_TEST_DESKTOP_FOCUS:
        name = "desktop.focus-second-window";
        break;
    case FROG_TEST_DESKTOP_KEYBOARD:
        name = "desktop.keyboard-to-focus";
        break;
    case FROG_TEST_DESKTOP_CONFIGURE:
        name = "desktop.drag-configure";
        break;
    case FROG_TEST_DESKTOP_CLIENT_OBSERVED:
        name = "desktop.client-observed-events";
        break;
    case FROG_TEST_DESKTOP_FINAL_SCENE:
        name = "desktop.final-scene";
        break;
    case FROG_TEST_DESKTOP_LIFECYCLE:
        name = "desktop.child-lifecycle";
        break;
    case FROG_TEST_DESKTOP_CLIENT_EXEC:
        name = "desktop.client-exec";
        break;
    case FROG_TEST_DESKTOP_SERVICE_BIND:
        name = "desktop.service-bind";
        break;
    case FROG_TEST_DESKTOP_THREE_CREATES:
        name = "desktop.three-creates";
        break;
    case FROG_TEST_DESKTOP_THIRD_CLOSE:
        name = "desktop.third-close";
        break;
    case FROG_TEST_DESKTOP_IDLE_PRESENT_STABLE:
        name = "desktop.idle-present-stable";
        break;
    case FROG_TEST_TIME_MONOTONIC_NORMALIZED:
        name = "time.monotonic.normalized";
        break;
    case FROG_TEST_TIME_MONOTONIC_ADVANCES:
        name = "time.monotonic.advances";
        break;
    case FROG_TEST_TIME_MONOTONIC_BAD_POINTERS:
        name = "time.monotonic.bad-pointers";
        break;
    case FROG_TEST_TIME_INVALID_CLOCK:
        name = "time.clock.invalid";
        break;
    case FROG_TEST_TIME_REALTIME_UNAVAILABLE:
        name = "time.realtime.unavailable";
        break;
    case FROG_TEST_TIME_SETTIMEOFDAY_UNIMPLEMENTED:
        name = "time.settimeofday-unimplemented";
        break;
    case FROG_TEST_WAIT2_AVAILABLE:
        name = "wait2.available";
        break;
    case FROG_TEST_WAIT2_ABI:
        name = "wait2.abi";
        break;
    case FROG_TEST_WAIT2_ARGUMENTS:
        name = "wait2.arguments";
        break;
    case FROG_TEST_WAIT2_NEGATIVE_FD:
        name = "wait2.negative-fd";
        break;
    case FROG_TEST_WAIT2_INVALID_FD:
        name = "wait2.invalid-fd";
        break;
    case FROG_TEST_WAIT2_REVENTS_RESET:
        name = "wait2.revents-reset";
        break;
    case FROG_TEST_WAIT2_TIMEOUT_ZERO:
        name = "wait2.timeout-zero";
        break;
    case FROG_TEST_WAIT2_FINITE_SLEEP:
        name = "wait2.finite-sleep";
        break;
    case FROG_TEST_WAIT2_BOUNDARY_SLEEP:
        name = "wait2.boundary-sleep";
        break;
    case FROG_TEST_WAIT2_MULTIPLE_INVALID:
        name = "wait2.multiple-invalid";
        break;
    case FROG_TEST_WAIT2_COPYOUT_CLEANUP:
        name = "wait2.copyout-cleanup";
        break;
    default:
        return -EINVAL;
    }

    frog_test_case(name, passed != 0);
#ifdef CONFIG_FROG_TEST_DESKTOP
    if (passed)
        printk("FROGTEST CASE %s PASS\n", name);
#endif
    return 0;
}
#endif

/* uint_32 sys_getpid(void) */
/* { */
/*     return running_thread()->pid; */
/* } */
/*  */
/* #<{(|* */
/*  * The real producer of system call 'sendrec()' */
/*  * */
/*  * @param func SEND or RECEIVE */
/*  * @param src_dest To/From whom the message is transferred. this is task number */
/*  * @param p_msg pointer to message */
/*  * */
/*  * @return Zero if success */
/*  ****************************************************************************|)}># */
/* uint_32 sys_sendrec(uint_32 func, uint_32 src_dest, message *p_msg) */
/* { */
/*     TCB_t *caller = running_thread(); */
/*  */
/*     ASSERT((src_dest > 0 && src_dest < TASK_MAX) || src_dest == ANY_TASK || */
/*            src_dest == INTR_TASK); */
/*     int ret = 0; */
/*     p_msg->m_source = caller->pid; */
/*     ASSERT(p_msg->m_source != src_dest); */
/*     #<{(|* */
/*      * There are three function about sending and receiving message, SEND, */
/*      * RECIEVE, and BOTH. First two are easy understand. */
/*      * BOTH mean it is transformed into a SEND followed by a RECIEVE */
/*      * */
/*      ****************************************************************************|)}># */
/*     if (func == SEND) { */
/*         ret = msg_send(caller, src_dest, p_msg); */
/*         if (ret != 0) { */
/*             return ret; */
/*         } */
/*     } else if (func == RECEIVE) { */
/*         ret = msg_receive(caller, src_dest, p_msg); */
/*         if (ret != 0) { */
/*             return ret; */
/*         } */
/*     } else if (func == BOTH) { */
/*         ret = msg_send(caller, src_dest, p_msg); */
/*         if (ret == 0) */
/*             ret = msg_receive(caller, src_dest, p_msg); */
/*     } else { */
/*         char error[60]; */
/*         sprintf(error, */
/*                 "sys_sendrec invalid function: %d (SEND:%d, RECEIVE:%d).", func, */
/*                 SEND, RECEIVE); */
/*         PANIC(error); */
/*     } */
/*  */
/*     return 0; */
/* } */
/*  */
/* void sys_putc(char c) */
/* { */
/*     put_char(c); */
/* } */

int_32 sys_testsyscall(uint_32 command)
{
#ifdef CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE
    if (command == FROG_TEST_PACKAGEFS_LIFECYCLE_SNAPSHOT ||
        command == FROG_TEST_PACKAGEFS_LIFECYCLE_VERIFY)
        return packagefs_lifecycle_test_command(command);
#endif
#ifdef CONFIG_FROG_TEST_ANONYMOUS_MMAP
    if (command != FROG_TEST_ANON_MMAP_FINISH)
        return mm_anon_mmap_test_command(command);
#endif
#ifdef CONFIG_FROG_TEST_USER_ALLOCATOR
    if (command != FROG_TEST_USER_ALLOC_FINISH)
        return mm_user_allocator_test_command(command);
#endif
#ifdef CONFIG_FROG_TEST_PROCESS
    if (command == FROG_TEST_HEAP_ALLOC_16)
        return (int_32) (uint_32) umalloc(16);
    if (command == FROG_TEST_VM_PREPARE)
        return mm_vm_process_prepare();
    if (command == FROG_TEST_VM_VERIFY_CLEANUP)
        return mm_vm_process_verify_cleanup();
    if (command >= FROG_TEST_VM_FORK_FAIL_BASE &&
        command < FROG_TEST_VM_FORK_FAIL_BASE + FROG_TEST_VM_FORK_FAIL_COUNT)
        return mm_vm_process_arm_fork_failure(
            command - FROG_TEST_VM_FORK_FAIL_BASE);
    if (command > FROG_TEST_VM_VERIFY_REFS_BASE &&
        command <= FROG_TEST_VM_VERIFY_REFS_BASE +
                       FROG_TEST_VM_FORK_FAIL_COUNT)
        return mm_vm_process_verify_refs(
            command - FROG_TEST_VM_VERIFY_REFS_BASE);
#endif
#ifdef CONFIG_FROG_TEST_DISK
    if (command == FROG_TEST_EXEC_FAIL_PRECOMMIT)
        return exec_test_arm_fail_before_commit();
#endif
#if !defined(CONFIG_FROG_TEST_USER) && \
    !defined(CONFIG_FROG_TEST_USER_ALLOCATOR) && \
    !defined(CONFIG_FROG_TEST_FRAMEBUFFER_MMAP) && \
    !defined(CONFIG_FROG_TEST_POUDLAND_E2E) && \
    !defined(CONFIG_FROG_TEST_POUDLAND_BUILTIN) && \
    !defined(CONFIG_FROG_TEST_INPUT)
    INFO("[init]: ring3 reached, testsyscall a=%d", command);
#endif
#ifdef CONFIG_QEMU_TEST
#ifdef CONFIG_FROG_TEST_USER
    frog_test_case("user.low-image.syscalls", command == 0x46524f47);
#endif
#ifdef CONFIG_FROG_TEST_PROCESS
    mm_uaccess_process_regression();
#endif
    frog_test_milestone("ring3", 1);
    frog_test_finish();
#endif
    return 0;
}

void syscall_init(void)
{
    for (uint_32 nr = 0; nr < SYS_NR_COUNT; nr++)
        syscall_table[nr] = sys_ni_syscall;

    syscall_table[SYS_OPEN]    = sys_open;
    syscall_table[SYS_CLOSE]   = sys_close;
    syscall_table[SYS_READ]    = sys_read;
    syscall_table[SYS_WRITE]   = sys_write;
    syscall_table[SYS_PUTC]    = sys_putc;
    syscall_table[SYS_SEEK]    = sys_lseek;
    syscall_table[SYS_UNLINK]  = sys_unlink;
    syscall_table[SYS_MKDIR]   = sys_mkdir;
    syscall_table[SYS_RMDIR]   = sys_rmdir;
    syscall_table[SYS_IOCTL]   = sys_ioctl;
    syscall_table[SYS_MMAP]    = sys_mmap;
    syscall_table[SYS_MUNMAP]  = sys_munmap;
    syscall_table[SYS_CLOCK_GETTIME] = sys_clock_gettime;
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_SETTIMEOFDAY] = sys_settimeofday;
    syscall_table[SYS_TEST_SYNC] = sys_test_sync;
    syscall_table[SYS_FORK]    = sys_fork;
    syscall_table[SYS_EXIT]    = sys_exit;
    syscall_table[SYS_EXECV]   = sys_execv;
    syscall_table[SYS_WAIT]    = sys_wait;
    syscall_table[SYS_WAIT2]   = sys_wait2;
    syscall_table[SYS_TESTSYSCALL] = sys_testsyscall;
#ifdef CONFIG_QEMU_TEST
    syscall_table[SYS_TEST_REPORT] = sys_test_report;
#endif
}
