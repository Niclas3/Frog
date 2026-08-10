#include <asm/page.h>

#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/fork.h>
#include <frog/irqflags.h>
#include <frog/list.h>
#include <frog/memory.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/sched.h>
#include <frog/semaphore.h>
#include <frog/string.h>
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
#include <frog/test.h>
#endif
#include <frog/threads.h>
#include <frog/uaccess.h>
#include <frog/wait.h>
#include <kernel/assert.h>
#include <kernel/dev.h>
#include <kernel/mount.h>
#if defined(CONFIG_FROG_TEST_PACKAGEFS) || \
    defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
#include <kernel/qemu_test.h>
#endif
#include <kernel/vfs.h>

#include "packagefs.h"

#define PACKAGEFS_MAGIC        0x504b4746U
enum packagefs_endpoint_role {
        PACKAGEFS_ENDPOINT_SERVER = 1,
        PACKAGEFS_ENDPOINT_CLIENT,
};

enum packagefs_writable_state {
        PACKAGEFS_WRITABLE_IDLE = 0,
        PACKAGEFS_WRITABLE_ARMED,
        PACKAGEFS_WRITABLE_PENDING,
};

struct packagefs_service;

struct packagefs_queue {
        wait_queue_head_t read_wait;
        wait_queue_head_t write_wait;
        uint_32 head;
        uint_32 tail;
        uint_32 used;
        uint_32 capacity;
        uint_8 data[];
};

struct packagefs_session {
        struct packagefs_service *service;
        struct list_head node;
        uint_32 peer_id;
        uint_32 io_refs;
        uint_32 pending_c2s;
        uint_32 writable_required;
        struct packagefs_queue *s2c;
        enum packagefs_writable_state writable_state;
        bool connected;
        bool disconnect_consumed;
};

struct packagefs_endpoint {
        enum packagefs_endpoint_role role;
        struct packagefs_service *service;
        struct packagefs_session *session;
};

struct packagefs_service {
        struct lock lock;
        struct list_head clients;
        struct dentry *dentry;
        uint_32 client_count;
        uint_32 session_count;
        struct packagefs_queue *c2s;
        bool server_live;
};

static uint_32 packagefs_service_count;
static uint_32 packagefs_next_peer_id = 1U;
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
struct packagefs_test_counters {
        uint_32 services;
        uint_32 sessions;
        uint_32 endpoints;
        uint_32 queues;
};

static struct packagefs_test_counters packagefs_test_live;
static struct packagefs_test_counters packagefs_test_snapshot;
#endif

static int_32 packagefs_close(struct file *file);
static int_32 packagefs_read(struct file *file, void *buf, uint_32 count);
static int_32 packagefs_write(struct file *file, const void *buf,
                              uint_32 count);
static uint_32 packagefs_poll(struct file *file,
                              struct poll_table_struct *wait);

static struct file_operations packagefs_fops = {
    .close = packagefs_close,
    .read = packagefs_read,
    .write = packagefs_write,
    .poll = packagefs_poll,
};

typedef char packagefs_record_header_must_be_12_bytes[
    sizeof(struct frog_pkg_record) == FROG_PKG_HEADER_SIZE ? 1 : -1];

static struct packagefs_queue *packagefs_queue_alloc(void)
{
        struct packagefs_queue *queue = get_kernel_page(1);

        if (queue == NULL)
                return NULL;
        memset(queue, 0, PAGE_SIZE);
        init_waitqueue_head(&queue->read_wait);
        init_waitqueue_head(&queue->write_wait);
        queue->capacity = PAGE_SIZE - sizeof(*queue);
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        packagefs_test_live.queues++;
#endif
        return queue;
}

static void packagefs_queue_free(struct packagefs_queue *queue)
{
        if (queue != NULL) {
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
                ASSERT(packagefs_test_live.queues > 0);
                packagefs_test_live.queues--;
#endif
                free_page(MP_KERNEL, queue, 1);
        }
}

static uint_32 packagefs_queue_free_space(
    const struct packagefs_queue *queue)
{
        return queue->capacity - queue->used;
}

static void packagefs_queue_copy_in(struct packagefs_queue *queue,
                                    const uint_8 *source,
                                    uint_32 length)
{
        uint_32 first = queue->capacity - queue->tail;

        if (first > length)
                first = length;
        memcpy(queue->data + queue->tail, source, first);
        if (length > first)
                memcpy(queue->data, source + first, length - first);
        queue->tail = (queue->tail + length) % queue->capacity;
        queue->used += length;
}

static void packagefs_queue_copy_out(const struct packagefs_queue *queue,
                                     uint_32 offset,
                                     uint_8 *destination,
                                     uint_32 length)
{
        uint_32 start = (queue->head + offset) % queue->capacity;
        uint_32 first = queue->capacity - start;

        if (first > length)
                first = length;
        memcpy(destination, queue->data + start, first);
        if (length > first)
                memcpy(destination + first, queue->data, length - first);
}

static int packagefs_queue_peek(const struct packagefs_queue *queue,
                                struct frog_pkg_record *header,
                                uint_32 *record_size)
{
        if (queue->used == 0)
                return -EAGAIN;
        if (queue->used < FROG_PKG_HEADER_SIZE)
                return -EUCLEAN;
        packagefs_queue_copy_out(queue, 0, (uint_8 *) header,
                                 FROG_PKG_HEADER_SIZE);
        if (header->payload_size > FROG_PKG_PAYLOAD_MAX)
                return -EUCLEAN;
        *record_size = FROG_PKG_HEADER_SIZE + header->payload_size;
        if (*record_size > queue->used)
                return -EUCLEAN;
        return 0;
}

static void packagefs_queue_consume(struct packagefs_queue *queue,
                                    uint_32 length)
{
        ASSERT(length <= queue->used);
        queue->head = (queue->head + length) % queue->capacity;
        queue->used -= length;
        if (queue->used == 0)
                queue->head = queue->tail = 0;
}

static int packagefs_queue_enqueue(struct packagefs_queue *queue,
                                   const uint_8 *record,
                                   uint_32 record_size)
{
        if (packagefs_queue_free_space(queue) < record_size)
                return -EAGAIN;
        packagefs_queue_copy_in(queue, record, record_size);
        wake_up_interruptible(&queue->read_wait);
        return 0;
}

#ifdef CONFIG_FROG_TEST_PACKAGEFS
static void packagefs_test_make_record(uint_8 *storage, uint_32 peer_id,
                                       uint_8 value)
{
        struct frog_pkg_record *record =
            (struct frog_pkg_record *) storage;

        record->peer_id = peer_id;
        record->event = FROG_PKG_DATA;
        record->payload_size = FROG_PKG_PAYLOAD_MAX;
        memset(record->payload, value, FROG_PKG_PAYLOAD_MAX);
}

static bool packagefs_test_expect_record(struct packagefs_queue *queue,
                                         uint_8 *scratch,
                                         uint_32 peer_id,
                                         uint_8 value)
{
        struct frog_pkg_record header;
        struct frog_pkg_record *record =
            (struct frog_pkg_record *) scratch;
        uint_32 record_size;

        if (packagefs_queue_peek(queue, &header, &record_size) != 0 ||
            record_size != FROG_PKG_RECORD_MAX)
                return false;
        packagefs_queue_copy_out(queue, 0, scratch, record_size);
        if (record->peer_id != peer_id || record->event != FROG_PKG_DATA ||
            record->payload_size != FROG_PKG_PAYLOAD_MAX)
                return false;
        for (uint_32 index = 0; index < FROG_PKG_PAYLOAD_MAX; index++) {
                if (record->payload[index] != value)
                        return false;
        }
        packagefs_queue_consume(queue, record_size);
        return true;
}

static bool packagefs_queue_regression_test(void)
{
        struct packagefs_queue *queue = packagefs_queue_alloc();
        uint_8 *record = kmalloc(FROG_PKG_RECORD_MAX);
        uint_8 *scratch = kmalloc(FROG_PKG_RECORD_MAX);
        bool passed = queue != NULL && record != NULL && scratch != NULL;

        if (!passed)
                goto out;
        packagefs_test_make_record(record, 1, 0x11U);
        passed = packagefs_queue_enqueue(queue, record,
                                         FROG_PKG_RECORD_MAX) == 0;
        packagefs_test_make_record(record, 2, 0x22U);
        passed = packagefs_queue_enqueue(queue, record,
                                         FROG_PKG_RECORD_MAX) == 0 &&
                 passed;
        passed = packagefs_test_expect_record(queue, scratch, 1,
                                              0x11U) && passed;
        packagefs_test_make_record(record, 3, 0x33U);
        passed = packagefs_queue_enqueue(queue, record,
                                         FROG_PKG_RECORD_MAX) == 0 &&
                 passed;
        passed = packagefs_test_expect_record(queue, scratch, 2,
                                              0x22U) && passed;
        packagefs_test_make_record(record, 4, 0x44U);
        passed = queue->tail + FROG_PKG_RECORD_MAX > queue->capacity &&
                 passed;
        passed = packagefs_queue_enqueue(queue, record,
                                         FROG_PKG_RECORD_MAX) == 0 &&
                 passed;
        passed = packagefs_test_expect_record(queue, scratch, 3,
                                              0x33U) && passed;
        passed = packagefs_test_expect_record(queue, scratch, 4,
                                              0x44U) && passed;
        passed = queue->used == 0 && passed;
out:
        kfree(scratch);
        kfree(record);
        packagefs_queue_free(queue);
        return passed;
}
#endif

static uint_32 packagefs_accepted_server_flags(void)
{
        return O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC;
}

static uint_32 packagefs_accepted_client_flags(void)
{
        return O_RDWR | O_CLOEXEC;
}

static bool packagefs_flags_match(uint_32 flags, uint_32 accepted)
{
        return flags == accepted || flags == (accepted | O_NONBLOCK);
}

static int packagefs_validate_name(const char *name)
{
        uint_32 length;

        if (name == NULL || name[0] == '\0')
                return -EINVAL;
        length = strlen(name);
        if (length > FROG_PKG_NAME_MAX)
                return -ENAMETOOLONG;
        if ((length == 1 && name[0] == '.') ||
            (length == 2 && name[0] == '.' && name[1] == '.'))
                return -EINVAL;
        for (uint_32 index = 0; index < length; index++) {
                if (name[index] == '/')
                        return -EINVAL;
        }
        return 0;
}

static struct inode *packagefs_alloc_service_inode(
    struct inode *parent, struct packagefs_service *service)
{
        struct inode *inode = kmalloc(sizeof(*inode));

        if (inode == NULL)
                return NULL;
        memset(inode, 0, sizeof(*inode));
        inode->i_mode = FT_FIFO << 11;
        inode->i_nlink = 1;
        inode->i_sb = parent->i_sb;
        inode->i_fop = &packagefs_fops;
        inode->i_private = service;
        INIT_LIST_HEAD(&inode->i_active_node);
        return inode;
}

static int packagefs_init_file(struct file *file,
                               struct dentry *dentry,
                               struct packagefs_endpoint *endpoint)
{
        if (file == NULL || dentry == NULL || dentry->d_inode == NULL ||
            endpoint == NULL)
                return -EINVAL;
        file->f_pos = 0;
        file->f_count = 0;
        file->f_inode = dentry->d_inode;
        file->f_op = &packagefs_fops;
        file->f_dentry = dentry;
        file->private_data = endpoint;
        return 0;
}

static int packagefs_bind(struct inode *dir,
                          struct dentry *candidate,
                          struct file *file)
{
        struct packagefs_service *service;
        struct packagefs_endpoint *endpoint;
        struct inode *inode;
        int result;

        if (dentry_lookup(candidate->d_parent, candidate->d_name) != NULL)
                return -EEXIST;
        if (packagefs_service_count >= FROG_PKG_SERVICE_MAX)
                return -ENOSPC;

        service = kmalloc(sizeof(*service));
        endpoint = kmalloc(sizeof(*endpoint));
        if (service == NULL || endpoint == NULL) {
                kfree(endpoint);
                kfree(service);
                return -ENOMEM;
        }
        memset(service, 0, sizeof(*service));
        memset(endpoint, 0, sizeof(*endpoint));
        lock_init(&service->lock);
        INIT_LIST_HEAD(&service->clients);
        service->server_live = true;
        service->c2s = packagefs_queue_alloc();
        if (service->c2s == NULL) {
                kfree(endpoint);
                kfree(service);
                return -ENOMEM;
        }

        inode = packagefs_alloc_service_inode(dir, service);
        if (inode == NULL) {
                packagefs_queue_free(service->c2s);
                kfree(endpoint);
                kfree(service);
                return -ENOMEM;
        }
        candidate->d_inode = inode;
        candidate->d_type = FT_FIFO;
        candidate->d_flags |= DENTRY_EPHEMERAL;
        service->dentry = candidate;
        endpoint->role = PACKAGEFS_ENDPOINT_SERVER;
        endpoint->service = service;
        result = packagefs_init_file(file, candidate, endpoint);
        if (result != 0) {
                candidate->d_inode = NULL;
                candidate->d_type = FT_UNKOWN;
                candidate->d_flags &= ~DENTRY_EPHEMERAL;
                packagefs_queue_free(service->c2s);
                kfree(inode);
                kfree(endpoint);
                kfree(service);
                return result;
        }
        dentry_add_child(candidate->d_parent, candidate);
        packagefs_service_count++;
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        packagefs_test_live.services++;
        packagefs_test_live.endpoints++;
#endif
        return 0;
}

static int packagefs_connect(struct dentry *candidate, struct file *file)
{
        struct dentry *dentry = dentry_lookup(candidate->d_parent,
                                              candidate->d_name);
        struct packagefs_service *service;
        struct packagefs_session *session;
        struct packagefs_endpoint *endpoint;
        int result;

        if (dentry == NULL || dentry->d_inode == NULL)
                return -ENOENT;
        service = dentry->d_inode->i_private;
        if (service == NULL)
                return -ENOENT;
        if (dentry->d_inode->i_count == 0xffffU)
                return -EMFILE;
        if (packagefs_next_peer_id == 0)
                return -ENOSPC;

        session = kmalloc(sizeof(*session));
        endpoint = kmalloc(sizeof(*endpoint));
        if (session == NULL || endpoint == NULL) {
                kfree(endpoint);
                kfree(session);
                return -ENOMEM;
        }
        memset(session, 0, sizeof(*session));
        memset(endpoint, 0, sizeof(*endpoint));
        session->service = service;
        session->peer_id = packagefs_next_peer_id++;
        session->s2c = packagefs_queue_alloc();
        if (session->s2c == NULL) {
                kfree(endpoint);
                kfree(session);
                return -ENOMEM;
        }
        session->connected = true;
        INIT_LIST_HEAD(&session->node);
        endpoint->role = PACKAGEFS_ENDPOINT_CLIENT;
        endpoint->service = service;
        endpoint->session = session;
        result = packagefs_init_file(file, dentry, endpoint);
        if (result != 0) {
                packagefs_queue_free(session->s2c);
                kfree(endpoint);
                kfree(session);
                return result;
        }

        /* VFS holds namespace_lock; publish under the per-service lock. */
        lock_fetch(&service->lock);
        if (!service->server_live)
                result = -ENOENT;
        else if (service->session_count >= FROG_PKG_CLIENT_MAX)
                result = -ENOSPC;
        else {
                list_add_tail(&session->node, &service->clients);
                service->client_count++;
                service->session_count++;
                result = 0;
        }
        lock_release(&service->lock);
        if (result != 0) {
                file->f_inode = NULL;
                file->f_op = NULL;
                file->f_dentry = NULL;
                file->private_data = NULL;
                packagefs_queue_free(session->s2c);
                kfree(endpoint);
                kfree(session);
                return result;
        }
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        packagefs_test_live.sessions++;
        packagefs_test_live.endpoints++;
#endif
        return 0;
}

static int packagefs_atomic_open(struct inode *dir,
                                 struct dentry *candidate,
                                 struct file *file,
                                 uint_32 flags)
{
        int result;

        if (dir == NULL || candidate == NULL || candidate->d_parent == NULL ||
            file == NULL)
                return -EINVAL;
        result = packagefs_validate_name(candidate->d_name);
        if (result != 0)
                return result;
        if (packagefs_flags_match(flags,
                                  packagefs_accepted_server_flags()))
                return packagefs_bind(dir, candidate, file);
        if (packagefs_flags_match(flags,
                                  packagefs_accepted_client_flags()))
                return packagefs_connect(candidate, file);
        return -EINVAL;
}

static struct dentry *packagefs_lookup(struct inode *dir,
                                       struct dentry *target)
{
        (void) dir;
        if (target == NULL || target->d_parent == NULL ||
            target->d_name == NULL)
                return NULL;
        return dentry_lookup(target->d_parent, target->d_name);
}

static struct packagefs_session *packagefs_find_session(
    struct packagefs_service *service, uint_32 peer_id)
{
        struct list_head *position;

        list_for_each(position, &service->clients) {
                struct packagefs_session *session = list_entry(
                    position, struct packagefs_session, node);

                if (session->connected && session->peer_id == peer_id)
                        return session;
        }
        return NULL;
}

static struct packagefs_session *packagefs_find_any_session(
    struct packagefs_service *service, uint_32 peer_id)
{
        struct list_head *position;

        list_for_each(position, &service->clients) {
                struct packagefs_session *session = list_entry(
                    position, struct packagefs_session, node);

                if (session->peer_id == peer_id)
                        return session;
        }
        return NULL;
}

static struct packagefs_session *packagefs_ready_disconnect(
    struct packagefs_service *service)
{
        struct list_head *position;

        list_for_each(position, &service->clients) {
                struct packagefs_session *session = list_entry(
                    position, struct packagefs_session, node);

                if (!session->connected && !session->disconnect_consumed &&
                    session->pending_c2s == 0)
                        return session;
        }
        return NULL;
}

static struct packagefs_session *packagefs_ready_writable(
    struct packagefs_service *service)
{
        struct list_head *position;

        list_for_each(position, &service->clients) {
                struct packagefs_session *session = list_entry(
                    position, struct packagefs_session, node);

                if (session->connected &&
                    session->writable_state == PACKAGEFS_WRITABLE_PENDING)
                        return session;
        }
        return NULL;
}

static void packagefs_destroy_session_locked(
    struct packagefs_service *service,
    struct packagefs_session *session)
{
        ASSERT(service != NULL && session != NULL &&
               session->service == service && !session->connected &&
               session->io_refs == 0 &&
               (!service->server_live || session->disconnect_consumed));
        list_del_init(&session->node);
        packagefs_queue_free(session->s2c);
        session->s2c = NULL;
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        ASSERT(packagefs_test_live.sessions > 0);
        packagefs_test_live.sessions--;
#endif
        kfree(session);
}

static void packagefs_put_session_locked(
    struct packagefs_service *service,
    struct packagefs_session *session)
{
        ASSERT(service != NULL && session != NULL &&
               session->service == service && session->io_refs > 0);
        session->io_refs--;
        if (!session->connected && session->io_refs == 0 &&
            (!service->server_live || session->disconnect_consumed))
                packagefs_destroy_session_locked(service, session);
}

static void packagefs_wait_locked(struct packagefs_service *service,
                                  wait_queue_head_t *wait_address)
{
        TCB_t *current = running_thread();
        DECLARE_WAITQUEUE(wait, current);
        unsigned long flags;

        local_irq_save(flags);
        add_wait_queue(wait_address, &wait);
        lock_release(&service->lock);
        thread_block(THREAD_TASK_WAITING);
        lock_fetch(&service->lock);
        remove_wait_queue(wait_address, &wait);
        local_irq_restore(flags);
}

static int packagefs_do_read(struct file *file, void *buf, uint_32 count)
{
        struct packagefs_endpoint *endpoint = file->private_data;
        struct packagefs_service *service;
        struct packagefs_queue *queue;
        uint_8 *scratch;
        int result;

        if (endpoint == NULL || endpoint->service == NULL)
                return -EUCLEAN;
        service = endpoint->service;
        scratch = kmalloc(FROG_PKG_RECORD_MAX);
        if (scratch == NULL)
                return -ENOMEM;

        lock_fetch(&service->lock);
        for (;;) {
                struct frog_pkg_record header;
                struct packagefs_session *control_session = NULL;
                uint_32 record_size;

                if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                        queue = service->c2s;
                } else if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT &&
                           endpoint->session != NULL) {
                        queue = endpoint->session->s2c;
                } else {
                        result = -EUCLEAN;
                        break;
                }

                if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                        control_session =
                            packagefs_ready_disconnect(service);
                        if (control_session == NULL)
                                control_session =
                                    packagefs_ready_writable(service);
                }
                if (control_session != NULL) {
                        header.peer_id = control_session->peer_id;
                        header.event = control_session->connected ?
                            FROG_PKG_WRITABLE : FROG_PKG_DISCONNECT;
                        header.payload_size = 0;
                        record_size = FROG_PKG_HEADER_SIZE;
                        if (count < record_size) {
                                result = -EMSGSIZE;
                                break;
                        }
                        result = copy_to_user(buf, &header, record_size);
                        if (result != 0)
                                break;
                        if (control_session->connected) {
                                control_session->writable_state =
                                    PACKAGEFS_WRITABLE_IDLE;
                                control_session->writable_required = 0;
                        } else {
                                control_session->disconnect_consumed = true;
                                ASSERT(service->session_count > 0);
                                service->session_count--;
                                if (control_session->io_refs == 0)
                                        packagefs_destroy_session_locked(
                                            service, control_session);
                        }
                        result = (int) record_size;
                        break;
                }

                result = packagefs_queue_peek(queue, &header,
                                              &record_size);
                if (result == -EAGAIN) {
                        if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT &&
                            !service->server_live) {
                                result = 0;
                                break;
                        }
                        if (file->f_flag & O_NONBLOCK)
                                break;
                        packagefs_wait_locked(service, &queue->read_wait);
                        continue;
                }
                if (result != 0)
                        break;
                if (count < record_size) {
                        result = -EMSGSIZE;
                        break;
                }
                packagefs_queue_copy_out(queue, 0, scratch, record_size);
                result = copy_to_user(buf, scratch, record_size);
                if (result != 0)
                        break;
                packagefs_queue_consume(queue, record_size);
                wake_up_interruptible(&queue->write_wait);
                if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                        struct packagefs_session *source =
                            packagefs_find_any_session(service,
                                                       header.peer_id);

                        if (source != NULL) {
                                ASSERT(source->pending_c2s > 0);
                                source->pending_c2s--;
                                if (!source->connected &&
                                    source->pending_c2s == 0)
                                        wake_up_interruptible(
                                            &service->c2s->read_wait);
                        }
                } else if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT) {
                        struct packagefs_session *session =
                            endpoint->session;

                        if (session->connected &&
                            session->writable_state ==
                                PACKAGEFS_WRITABLE_ARMED &&
                            packagefs_queue_free_space(session->s2c) >=
                                session->writable_required) {
                                session->writable_state =
                                    PACKAGEFS_WRITABLE_PENDING;
                                wake_up_interruptible(
                                    &service->c2s->read_wait);
                        }
                }
                result = (int) record_size;
                break;
        }
        lock_release(&service->lock);
        kfree(scratch);
        return result;
}

static int_32 packagefs_read(struct file *file, void *buf, uint_32 count)
{
        unsigned long flags;
        int result;

        local_irq_save(flags);
        local_irq_enable();
        result = packagefs_do_read(file, buf, count);
        local_irq_restore(flags);
        return result;
}

static int packagefs_copy_write_record(const void *buf,
                                       uint_32 count,
                                       uint_8 **scratch_out)
{
        struct frog_pkg_record header;
        uint_8 *scratch;

        *scratch_out = NULL;
        if (count < FROG_PKG_HEADER_SIZE)
                return -EINVAL;
        if (copy_from_user(&header, buf, FROG_PKG_HEADER_SIZE) != 0)
                return -EFAULT;
        if (header.payload_size > FROG_PKG_PAYLOAD_MAX)
                return -EMSGSIZE;
        if (count != FROG_PKG_HEADER_SIZE + header.payload_size ||
            header.event != FROG_PKG_DATA)
                return -EINVAL;

        scratch = kmalloc(count);
        if (scratch == NULL)
                return -ENOMEM;
        if (copy_from_user(scratch, buf, count) != 0) {
                kfree(scratch);
                return -EFAULT;
        }
        struct frog_pkg_record *copied =
            (struct frog_pkg_record *) scratch;
        if (copied->payload_size != header.payload_size ||
            copied->event != FROG_PKG_DATA) {
                kfree(scratch);
                return -EINVAL;
        }
        *scratch_out = scratch;
        return 0;
}

static int packagefs_do_write(struct file *file,
                              const void *buf,
                              uint_32 count)
{
        struct packagefs_endpoint *endpoint = file->private_data;
        struct packagefs_service *service;
        struct packagefs_session *held_session = NULL;
        struct frog_pkg_record *record;
        struct packagefs_queue *queue;
        uint_8 *scratch;
        uint_32 requested_peer;
        int result;

        if (endpoint == NULL || endpoint->service == NULL)
                return -EUCLEAN;
        result = packagefs_copy_write_record(buf, count, &scratch);
        if (result != 0)
                return result;
        service = endpoint->service;
        record = (struct frog_pkg_record *) scratch;
        requested_peer = record->peer_id;
        if ((endpoint->role == PACKAGEFS_ENDPOINT_CLIENT &&
             requested_peer != 0) ||
            (endpoint->role == PACKAGEFS_ENDPOINT_SERVER &&
             requested_peer == 0)) {
                kfree(scratch);
                return -EINVAL;
        }

        lock_fetch(&service->lock);
        if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                held_session = packagefs_find_session(service,
                                                      requested_peer);
                if (held_session == NULL) {
                        result = -ENOENT;
                        goto unlock;
                }
                if (held_session->io_refs == UINT_MAX) {
                        result = -EOVERFLOW;
                        held_session = NULL;
                        goto unlock;
                }
                held_session->io_refs++;
        }
        for (;;) {
                if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT) {
                        if (!service->server_live) {
                                result = -EPIPE;
                                break;
                        }
                        queue = service->c2s;
                        record->peer_id = endpoint->session->peer_id;
                } else if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                        if (!held_session->connected) {
                                result = -ENOENT;
                                break;
                        }
                        queue = held_session->s2c;
                        record->peer_id = 0;
                } else {
                        result = -EUCLEAN;
                        break;
                }

                result = packagefs_queue_enqueue(queue, scratch, count);
                if (result == 0) {
                        if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT)
                                endpoint->session->pending_c2s++;
                        result = (int) count;
                        break;
                }
                if (result != -EAGAIN)
                        break;
                if (file->f_flag & O_NONBLOCK) {
                        if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER &&
                            held_session->connected) {
                                if (held_session->writable_state ==
                                    PACKAGEFS_WRITABLE_IDLE)
                                        held_session->writable_state =
                                            PACKAGEFS_WRITABLE_ARMED;
                                if (held_session->writable_state ==
                                        PACKAGEFS_WRITABLE_ARMED &&
                                    held_session->writable_required < count)
                                        held_session->writable_required = count;
                        }
                        break;
                }
                packagefs_wait_locked(service, &queue->write_wait);
                record->peer_id = requested_peer;
        }
        if (held_session != NULL)
                packagefs_put_session_locked(service, held_session);
unlock:
        lock_release(&service->lock);
        kfree(scratch);
        return result;
}

static int_32 packagefs_write(struct file *file, const void *buf,
                              uint_32 count)
{
        unsigned long flags;
        int result;

        local_irq_save(flags);
        local_irq_enable();
        result = packagefs_do_write(file, buf, count);
        local_irq_restore(flags);
        return result;
}

static uint_32 packagefs_poll(struct file *file,
                              struct poll_table_struct *wait)
{
        struct packagefs_endpoint *endpoint = file->private_data;
        struct packagefs_service *service;
        uint_32 mask = 0;

        if (endpoint == NULL || endpoint->service == NULL)
                return POLLERR;
        service = endpoint->service;
        lock_fetch(&service->lock);
        if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                poll_wait(file, &service->c2s->read_wait, wait);
                if (service->c2s->used != 0)
                        mask |= POLLIN;
                else if (packagefs_ready_disconnect(service) != NULL)
                        mask |= POLLIN;
                else if (packagefs_ready_writable(service) != NULL)
                        mask |= POLLIN;
        } else if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT &&
                   endpoint->session != NULL) {
                struct packagefs_queue *s2c = endpoint->session->s2c;

                poll_wait(file, &s2c->read_wait, wait);
                poll_wait(file, &service->c2s->write_wait, wait);
                if (s2c->used != 0)
                        mask |= POLLIN;
                if (!service->server_live)
                        mask |= POLLHUP;
                else if (packagefs_queue_free_space(service->c2s) >=
                         FROG_PKG_RECORD_MAX)
                        mask |= POLLOUT;
        } else {
                mask = POLLERR;
        }
        lock_release(&service->lock);
        return mask;
}

static int_32 packagefs_close(struct file *file)
{
        struct packagefs_endpoint *endpoint;
        struct packagefs_service *service;

        if (file == NULL || file->private_data == NULL)
                return -EINVAL;
        endpoint = file->private_data;
        service = endpoint->service;
        if (service == NULL) {
                kfree(endpoint);
                file->private_data = NULL;
                return -EUCLEAN;
        }

        lock_fetch(&service->lock);
        if (endpoint->role == PACKAGEFS_ENDPOINT_CLIENT) {
                struct packagefs_session *session = endpoint->session;

                if (session == NULL || session->service != service) {
                        lock_release(&service->lock);
                        kfree(endpoint);
                        file->private_data = NULL;
                        return -EUCLEAN;
                }
                if (session->connected) {
                        session->connected = false;
                        session->writable_state = PACKAGEFS_WRITABLE_IDLE;
                        session->writable_required = 0;
                        ASSERT(service->client_count > 0);
                        service->client_count--;
                        wake_up_interruptible_all(
                            &session->s2c->read_wait);
                        wake_up_interruptible_all(
                            &session->s2c->write_wait);
                        wake_up_interruptible_all(
                            &service->c2s->read_wait);
                }
                endpoint->session = NULL;
                if (!service->server_live) {
                        if (!session->disconnect_consumed) {
                                ASSERT(service->session_count > 0);
                                service->session_count--;
                                session->disconnect_consumed = true;
                        }
                }
                if (session->io_refs == 0 &&
                    (!service->server_live ||
                     session->disconnect_consumed))
                        packagefs_destroy_session_locked(service, session);
        } else if (endpoint->role == PACKAGEFS_ENDPOINT_SERVER) {
                if (service->server_live) {
                        service->server_live = false;
                        list_del_init(&service->dentry->d_child_node);
                        ASSERT(packagefs_service_count > 0);
                        packagefs_service_count--;
                        wake_up_interruptible_all(
                            &service->c2s->read_wait);
                        wake_up_interruptible_all(
                            &service->c2s->write_wait);
                        struct list_head *position;
                        struct list_head *next;
                        for (position = service->clients.next;
                             position != &service->clients;
                             position = next) {
                                struct packagefs_session *session =
                                    list_entry(position,
                                               struct packagefs_session,
                                               node);
                                next = position->next;
                                wake_up_interruptible_all(
                                    &session->s2c->read_wait);
                                wake_up_interruptible_all(
                                    &session->s2c->write_wait);
                                session->writable_state =
                                    PACKAGEFS_WRITABLE_IDLE;
                                session->writable_required = 0;
                                if (!session->connected) {
                                        if (!session->disconnect_consumed) {
                                                ASSERT(service->session_count > 0);
                                                service->session_count--;
                                                session->disconnect_consumed = true;
                                        }
                                        if (session->io_refs == 0)
                                                packagefs_destroy_session_locked(
                                                    service, session);
                                }
                        }
                }
        } else {
                lock_release(&service->lock);
                kfree(endpoint);
                file->private_data = NULL;
                return -EUCLEAN;
        }
        lock_release(&service->lock);
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        ASSERT(packagefs_test_live.endpoints > 0);
        packagefs_test_live.endpoints--;
#endif
        kfree(endpoint);
        file->private_data = NULL;
        return 0;
}

static void packagefs_evict_inode(struct inode *inode)
{
        struct packagefs_service *service;
        struct dentry *dentry;

        if (inode == NULL)
                return;
        service = inode->i_private;
        if (service == NULL)
                return;
        ASSERT(!service->server_live && service->client_count == 0 &&
               service->session_count == 0 &&
               inode->i_count == 0);
        ASSERT(list_is_empty(&service->clients));
        dentry = service->dentry;
        packagefs_queue_free(service->c2s);
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
        ASSERT(packagefs_test_live.services > 0);
        packagefs_test_live.services--;
#endif
        inode->i_private = NULL;
        kfree(service);
        kfree(inode);
        if (dentry != NULL) {
                kfree(dentry->d_name);
                kfree(dentry);
        }
}

static struct super_operations packagefs_sops = {
    .evict_inode = packagefs_evict_inode,
};

static struct inode_operations packagefs_root_iops = {
    .atomic_open = packagefs_atomic_open,
    .lookup = packagefs_lookup,
};

static struct super_block *packagefs_mount(struct fs_type *fs,
                                           int flags,
                                           const struct vfs_mount_source *source,
                                           void *data)
{
        struct super_block *sb;
        struct inode *root;

        (void) fs;
        (void) flags;
        (void) source;
        (void) data;
        sb = kmalloc(sizeof(*sb));
        root = kmalloc(sizeof(*root));
        if (sb == NULL || root == NULL) {
                kfree(root);
                kfree(sb);
                return NULL;
        }
        memset(sb, 0, sizeof(*sb));
        memset(root, 0, sizeof(*root));
        sb->s_magic = PACKAGEFS_MAGIC;
        sb->s_op = &packagefs_sops;
        sb->s_root = root;
        INIT_LIST_HEAD(&sb->s_inodes);
        root->i_mode = FT_DIRECTORY << 11;
        root->i_nlink = 2;
        root->i_sb = sb;
        root->i_op = &packagefs_root_iops;
        INIT_LIST_HEAD(&root->i_active_node);
        return sb;
}

static struct fs_type packagefs_type = {
    .name = "packagefs",
    .mount = packagefs_mount,
};

int packagefs_init(void)
{
        int result = register_fs(&packagefs_type);

        if (result != 0)
                return result;
        result = devfs_create_directory("pkg");
        if (result != 0)
                goto unregister;
        result = vfs_mount("/dev/pkg", "packagefs", 0, NULL, NULL);
        if (result != 0)
                goto unregister;
#ifdef CONFIG_FROG_TEST_PACKAGEFS
        frog_test_case("packagefs.queue-wrap-fifo",
                       packagefs_queue_regression_test());
#endif
        return 0;

unregister:
        (void) unregister_fs(&packagefs_type);
        return result;
}

#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
int packagefs_lifecycle_test_command(uint_32 command)
{
        if (command == FROG_TEST_PACKAGEFS_LIFECYCLE_SNAPSHOT) {
                packagefs_test_snapshot = packagefs_test_live;
                return 0;
        }
        if (command == FROG_TEST_PACKAGEFS_LIFECYCLE_VERIFY)
                return memcmp(&packagefs_test_snapshot,
                              &packagefs_test_live,
                              sizeof(packagefs_test_live)) == 0 ? 0 :
                                                                  -EUCLEAN;
        return -EINVAL;
}
#endif
