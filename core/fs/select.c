#include <asm/page.h>
#include <frog/errno.h>
#include <frog/fork.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/poll.h>
#include <frog/sched.h>
#include <frog/test.h>
#include <frog/threads.h>
#include <frog/uaccess.h>
#include <kernel/fd.h>
#include <kernel/timekeeping.h>
#include <kernel/vfs.h>
#include <kernel/wait2.h>
#ifdef CONFIG_FROG_TEST_SYSTEM_INIT_SELECTION
#include <kernel/system_init_selection_test.h>
#endif
#ifdef CONFIG_FROG_TEST_GRAPHICAL_INIT_PRODUCTION
#include <kernel/graphical_init_production_test.h>
#endif

#define WAIT2_MAX_FDS      ((uint_32) MAX_FILES_OPEN_PER_PROC)
#define WAIT2_REQUEST_BITS (POLLIN | POLLOUT)
#define WAIT2_RESULT_BITS  (POLLERR | POLLHUP | POLLNVAL)
#define NSEC_PER_SEC       1000000000
#define NSEC_PER_MSEC      1000000

struct poll_table_entry {
        struct file *filp;
        wait_queue_t wait;
        wait_queue_head_t *wait_address;
};

struct poll_table_page {
        struct poll_table_page *next;
        struct poll_table_entry *entry;
        struct poll_table_entry entries[0];
};

struct wait2_entry {
        struct pollfd descriptor;
        struct file *file;
};

typedef char wait2_fd_limit_must_be_32[
    MAX_FILES_OPEN_PER_PROC == 32 ? 1 : -1];

#define POLL_TABLE_FULL(table)                  \
        ((unsigned long) ((table)->entry + 1) > \
         PAGE_SIZE + (unsigned long) (table))

#ifdef CONFIG_FROG_TEST_WAIT2
static bool wait2_fail_copyout;
static uint_32 wait2_active_file_refs;
static uint_32 wait2_active_registrations;
#endif

static bool poll_table_contains(const poll_table *poll_table,
                                wait_queue_head_t *wait_address)
{
        const struct poll_table_page *page;

        for (page = poll_table->table; page != NULL; page = page->next) {
                const struct poll_table_entry *entry;

                for (entry = page->entries; entry < page->entry; entry++) {
                        if (entry->wait_address == wait_address)
                                return true;
                }
        }
        return false;
}

void __pollwait(struct file *filp, wait_queue_head_t *wait_address,
                poll_table *poll_table)
{
        struct poll_table_page *page;
        TCB_t *current;

        if (poll_table == NULL || filp == NULL || wait_address == NULL) {
                if (poll_table != NULL)
                        poll_table->error = -EINVAL;
                return;
        }
        if (poll_table_contains(poll_table, wait_address))
                return;

        page = poll_table->table;
        current = running_thread();
        if (page == NULL || POLL_TABLE_FULL(page)) {
                struct poll_table_page *new_page = get_kernel_page(1);

                if (new_page == NULL) {
                        poll_table->error = -ENOMEM;
                        return;
                }
                new_page->entry = new_page->entries;
                new_page->next = page;
                poll_table->table = new_page;
                page = new_page;
        }

        struct poll_table_entry *entry = page->entry++;

        entry->filp = filp;
        entry->wait_address = wait_address;
        init_waitqueue_entry(&entry->wait, current);
        add_wait_queue(wait_address, &entry->wait);
        poll_table->registrations++;
#ifdef CONFIG_FROG_TEST_WAIT2
        wait2_active_registrations++;
#endif
}

void poll_freewait(poll_table *poll_table)
{
        struct poll_table_page *page;

        if (poll_table == NULL)
                return;
        page = poll_table->table;
        poll_table->table = NULL;
        while (page != NULL) {
                struct poll_table_entry *entry = page->entry;
                struct poll_table_page *old_page;

                while (entry > page->entries) {
                        entry--;
                        remove_wait_queue(entry->wait_address, &entry->wait);
                        poll_table->registrations--;
#ifdef CONFIG_FROG_TEST_WAIT2
                        wait2_active_registrations--;
#endif
                }
                old_page = page;
                page = page->next;
                free_page(MP_KERNEL, old_page, 1);
        }
}

static int timespec_compare(const struct timespec *left,
                            const struct timespec *right)
{
        if (left->tv_sec != right->tv_sec)
                return left->tv_sec < right->tv_sec ? -1 : 1;
        if (left->tv_nsec != right->tv_nsec)
                return left->tv_nsec < right->tv_nsec ? -1 : 1;
        return 0;
}

static void wait2_make_deadline(struct timespec *deadline,
                                const struct timespec *now,
                                int_32 timeout_ms)
{
        deadline->tv_sec = now->tv_sec + timeout_ms / 1000;
        deadline->tv_nsec = now->tv_nsec +
                            (timeout_ms % 1000) * NSEC_PER_MSEC;
        if (deadline->tv_nsec >= NSEC_PER_SEC) {
                deadline->tv_nsec -= NSEC_PER_SEC;
                deadline->tv_sec++;
        }
}

static int_32 wait2_remaining_ticks(const struct timespec *deadline,
                                    const struct timespec *now)
{
        int_32 nanoseconds;
        int_32 ticks;

        if (timespec_compare(now, deadline) >= 0)
                return 0;
        if (deadline->tv_sec - now->tv_sec > 1)
                return 1000;
        if (deadline->tv_sec != now->tv_sec)
                nanoseconds = NSEC_PER_SEC - now->tv_nsec +
                              deadline->tv_nsec;
        else
                nanoseconds = deadline->tv_nsec - now->tv_nsec;
        ticks = nanoseconds / NSEC_PER_MSEC;
        if (nanoseconds % NSEC_PER_MSEC != 0)
                ticks++;
        return ticks > 0 ? ticks : 1;
}

static int_32 wait2_scan(struct wait2_entry *entries, uint_32 count,
                         poll_table *poll_table)
{
        int_32 ready = 0;

        for (uint_32 index = 0; index < count; index++) {
                struct wait2_entry *entry = &entries[index];
                uint_32 mask;

                entry->descriptor.revents = 0;
                if (entry->descriptor.fd < 0)
                        continue;
                if (entry->file == NULL) {
                        entry->descriptor.revents = POLLNVAL;
                        ready++;
                        continue;
                }
                mask = vfs_poll(entry->file, poll_table);
                entry->descriptor.revents =
                    (uint_16) (mask & (entry->descriptor.events |
                                      WAIT2_RESULT_BITS));
                if (entry->descriptor.revents != 0)
                        ready++;
        }
        return ready;
}

static void wait2_release_entries(struct wait2_entry *entries,
                                  uint_32 count)
{
        if (entries == NULL)
                return;
        for (uint_32 index = 0; index < count; index++) {
                if (entries[index].file != NULL) {
                        file_put(entries[index].file);
#ifdef CONFIG_FROG_TEST_WAIT2
                        wait2_active_file_refs--;
#endif
                }
        }
        kfree(entries);
}

static int_32 wait2_copy_revents(struct pollfd *user_fds,
                                 const uint_16 *revents,
                                 uint_32 count)
{
#ifdef CONFIG_FROG_TEST_WAIT2
        if (wait2_fail_copyout) {
                bool lifecycle_ok = wait2_active_file_refs != 0 &&
                                    wait2_active_registrations == 0;

                wait2_fail_copyout = false;
                return lifecycle_ok ? -EFAULT : -EUCLEAN;
        }
#endif
        for (uint_32 index = 0; index < count; index++) {
                int_32 status = copy_to_user(&user_fds[index].revents,
                                             &revents[index],
                                             sizeof(user_fds[index].revents));

                if (status != 0)
                        return status;
        }
        return 0;
}

static int_32 do_wait2(struct pollfd *user_fds, uint_32 count,
                       int_32 timeout_ms)
{
        struct wait2_entry *entries = NULL;
        struct pollfd copied[WAIT2_MAX_FDS];
        uint_16 revents[WAIT2_MAX_FDS];
        struct timespec deadline;
        poll_table table;
        bool have_deadline = timeout_ms > 0;
        int_32 result = 0;
        int_32 status;

        if (timeout_ms < -1 || count > WAIT2_MAX_FDS)
                return -EINVAL;
        if (count == 0 && timeout_ms == -1)
                return -EINVAL;
        if (count != 0) {
                status = copy_from_user(copied, user_fds,
                                        count * sizeof(struct pollfd));
                if (status != 0)
                        return status;
                entries = kmalloc(count * sizeof(*entries));
                if (entries == NULL)
                        return -ENOMEM;
                for (uint_32 index = 0; index < count; index++) {
                        entries[index].descriptor = copied[index];
                        entries[index].file = NULL;
                        entries[index].descriptor.revents = 0;
                }
                for (uint_32 index = 0; index < count; index++) {
                        if ((entries[index].descriptor.events &
                             ~WAIT2_REQUEST_BITS) != 0) {
                                wait2_release_entries(entries, count);
                                return -EINVAL;
                        }
                }
                for (uint_32 index = 0; index < count; index++) {
                        if (entries[index].descriptor.fd < 0)
                                continue;
                        entries[index].file =
                            fdget(entries[index].descriptor.fd);
#ifdef CONFIG_FROG_TEST_WAIT2
                        if (entries[index].file != NULL)
                                wait2_active_file_refs++;
#endif
                }
        }

        if (have_deadline) {
                struct timespec now;

                timekeeping_get_monotonic(&now);
                wait2_make_deadline(&deadline, &now, timeout_ms);
        }

        poll_initwait(&table);
        for (;;) {
                unsigned long scan_flags;
                int_32 ready;

                local_irq_save(scan_flags);
                if (timeout_ms == 0) {
                        result = wait2_scan(entries, count, NULL);
                        running_thread()->status = THREAD_TASK_RUNNING;
                        local_irq_restore(scan_flags);
                        break;
                }
                running_thread()->status = THREAD_TASK_WAITING;
                if (table.registrations == 0) {
                        (void) wait2_scan(entries, count, &table);
                        if (table.error != 0) {
                                result = table.error;
                                running_thread()->status = THREAD_TASK_RUNNING;
                                local_irq_restore(scan_flags);
                                break;
                        }
                }
                ready = wait2_scan(entries, count, NULL);
                if (ready > 0) {
                        result = ready;
                        running_thread()->status = THREAD_TASK_RUNNING;
                        local_irq_restore(scan_flags);
                        break;
                }
                if (timeout_ms == -1 && table.registrations == 0) {
                        result = -EINVAL;
                        running_thread()->status = THREAD_TASK_RUNNING;
                        local_irq_restore(scan_flags);
                        break;
                }
                if (have_deadline) {
                        struct timespec now;
                        int_32 remaining;

                        timekeeping_get_monotonic(&now);
                        remaining = wait2_remaining_ticks(&deadline, &now);
                        if (remaining == 0) {
                                running_thread()->status = THREAD_TASK_RUNNING;
                                local_irq_restore(scan_flags);
                                break;
                        }
                        (void) schedule_timeout(remaining);
                } else {
                        (void) schedule_timeout(MAX_SCHEDULE_TIMEOUT);
                }
                local_irq_restore(scan_flags);
        }

        for (uint_32 index = 0; index < count; index++)
                revents[index] = entries[index].descriptor.revents;
        poll_freewait(&table);
        if (result >= 0 && count != 0) {
                status = wait2_copy_revents(user_fds, revents, count);
        } else {
                status = 0;
        }
        wait2_release_entries(entries, count);
        if (status != 0)
                return status;
        return result;
}

int_32 sys_wait2(struct pollfd *user_fds, uint_32 count, int_32 timeout_ms)
{
        unsigned long entry_flags;
        int_32 result;

#ifdef CONFIG_FROG_TEST_SYSTEM_INIT_SELECTION
        system_init_selection_test_observe_wait(user_fds, count, timeout_ms);
#endif
#ifdef CONFIG_FROG_TEST_GRAPHICAL_INIT_PRODUCTION
        graphical_init_production_test_observe_wait(user_fds, count,
                                                    timeout_ms);
#endif
        local_irq_save(entry_flags);
        local_irq_enable();
        result = do_wait2(user_fds, count, timeout_ms);
        local_irq_restore(entry_flags);
        return result;
}

#ifdef CONFIG_FROG_TEST_WAIT2
int_32 wait2_test_command(uint_32 command)
{
        if (command == FROG_TEST_WAIT2_ARM_COPYOUT_FAULT) {
                wait2_fail_copyout = true;
                return 0;
        }
        if (command == FROG_TEST_WAIT2_VERIFY_CLEANUP)
                return wait2_active_file_refs == 0 &&
                               wait2_active_registrations == 0
                           ? 0
                           : -EUCLEAN;
        return -EINVAL;
}
#endif
