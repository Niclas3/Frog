#ifndef __SYS_WAIT_H
#define __SYS_WAIT_H

#include <kernel_print.h>
#include <list.h>
#include <sys/memory.h>
#include <sys/spinlock.h>

extern TCB_t *running_thread(void);

/*
 * Debug control.  Slow but useful.
 * first test this kind of debug method(:-|)
 */
#define CONFIG_DEBUG_WAITQ
#if defined(CONFIG_DEBUG_WAITQ)
#define WAITQUEUE_DEBUG 1
#else
#define WAITQUEUE_DEBUG 0
#endif

struct __wait_queue {
    unsigned int flags;
#define WQ_FLAG_EXCLUSIVE 0x01
    struct thread_control_block *task;
    struct list_head task_list;
#if WAITQUEUE_DEBUG
    long __magic;
    long __waker;
#endif
};

typedef struct __wait_queue wait_queue_t;

#define wq_lock_t spinlock_t
#define WAITQUEUE_RW_LOCK_UNLOCKED SPIN_LOCK_UNLOCKED

#define wq_read_lock spin_lock
#define wq_read_lock_irqsave spin_lock_irqsave
#define wq_read_unlock spin_unlock
#define wq_read_unlock_irqrestore spin_unlock_irqrestore
#define wq_write_lock_irq spin_lock_irq
#define wq_write_lock_irqsave spin_lock_irqsave
#define wq_write_unlock_irqrestore spin_unlock_irqrestore
#define wq_write_unlock spin_unlock

struct __wait_queue_head {
    wq_lock_t lock;
    struct list_head task_list;
#if WAITQUEUE_DEBUG
    long __magic;
    long __creator;
#endif
};
typedef struct __wait_queue_head wait_queue_head_t;

/*
 * Debugging macros.  We eschew `do { } while (0)' because gcc can generate
 * spurious .aligns.
 */
#if WAITQUEUE_DEBUG
#define WQ_BUG() BUG()
#define CHECK_MAGIC(x)                                                        \
    do {                                                                      \
        if ((x) != (long) &(x)) {                                             \
            kprint("bad magic %lx (should be %lx), ", (long) x, (long) &(x)); \
            WQ_BUG();                                                         \
        }                                                                     \
    } while (0)
#define CHECK_MAGIC_WQHEAD(x)                                             \
    do {                                                                  \
        if ((x)->__magic != (long) &((x)->__magic)) {                     \
            kprint("bad magic %lx (should be %lx, creator %lx), ",        \
                   (x)->__magic, (long) &((x)->__magic), (x)->__creator); \
            WQ_BUG();                                                     \
        }                                                                 \
    } while (0)
#define WQ_CHECK_LIST_HEAD(list)            \
    do {                                    \
        if (!(list)->next || !(list)->prev) \
            WQ_BUG();                       \
    } while (0)
#define WQ_NOTE_WAKER(tsk)                                   \
    do {                                                     \
        (tsk)->__waker = (long) __builtin_return_address(0); \
    } while (0)
#else
#define WQ_BUG()
#define CHECK_MAGIC(x)
#define CHECK_MAGIC_WQHEAD(x)
#define WQ_CHECK_LIST_HEAD(list)
#define WQ_NOTE_WAKER(tsk)
#endif


/*
 * Macros for declaration and initialization of the datatypes
 */

#if WAITQUEUE_DEBUG
#define __WAITQUEUE_DEBUG_INIT(name) (long) &(name).__magic, 0
#define __WAITQUEUE_HEAD_DEBUG_INIT(name) \
    (long) &(name).__magic, (long) &(name).__magic
#else
#define __WAITQUEUE_DEBUG_INIT(name)
#define __WAITQUEUE_HEAD_DEBUG_INIT(name)
#endif

#define __WAITQUEUE_INITIALIZER(name, tsk)                          \
    {                                                               \
    task:                                                           \
        tsk, task_list : {NULL, NULL}, __WAITQUEUE_DEBUG_INIT(name) \
    }

#define DECLARE_WAITQUEUE(name, tsk) \
    wait_queue_t name = __WAITQUEUE_INITIALIZER(name, tsk)

#define __WAIT_QUEUE_HEAD_INITIALIZER(name)                     \
    {                                                           \
    lock:                                                       \
        WAITQUEUE_RW_LOCK_UNLOCKED,                             \
            task_list : {&(name).task_list, &(name).task_list}, \
                        __WAITQUEUE_HEAD_DEBUG_INIT(name)       \
    }

#define DECLARE_WAIT_QUEUE_HEAD(name) \
    wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

static inline void init_waitqueue_head(wait_queue_head_t *q)
{
#if WAITQUEUE_DEBUG
    if (!q)
        WQ_BUG();
#endif
    q->lock = WAITQUEUE_RW_LOCK_UNLOCKED;
    INIT_LIST_HEAD(&q->task_list);
#if WAITQUEUE_DEBUG
    q->__magic = (long) &q->__magic;
    q->__creator = (long)running_thread();
#endif
}

static inline void init_waitqueue_entry(wait_queue_t *q, TCB_t *p)
{
#if WAITQUEUE_DEBUG
    if (!q || !p)
        WQ_BUG();
#endif
    q->flags = 0;
    q->task = p;
#if WAITQUEUE_DEBUG
    q->__magic = (long) &q->__magic;
#endif
}

static inline int waitqueue_active(wait_queue_head_t *q)
{
#if WAITQUEUE_DEBUG
    if (!q)
        WQ_BUG();
    CHECK_MAGIC_WQHEAD(q);
#endif

    return !list_is_empty(&q->task_list);
}

static inline void __add_wait_queue(wait_queue_head_t *head, wait_queue_t *new)
{
#if WAITQUEUE_DEBUG
    if (!head || !new)
        WQ_BUG();
    CHECK_MAGIC_WQHEAD(head);
    CHECK_MAGIC(new->__magic);
    if (!head->task_list.next || !head->task_list.prev)
        WQ_BUG();
#endif
    list_add(&new->task_list, &head->task_list);
}

/*
 * Used for wake-one threads:
 */
static inline void __add_wait_queue_tail(wait_queue_head_t *head,
                                         wait_queue_t *new)
{
#if WAITQUEUE_DEBUG
    if (!head || !new)
        WQ_BUG();
    CHECK_MAGIC_WQHEAD(head);
    CHECK_MAGIC(new->__magic);
    if (!head->task_list.next || !head->task_list.prev)
        WQ_BUG();
#endif
    list_add_tail(&new->task_list, &head->task_list);
}

static inline void __remove_wait_queue(wait_queue_head_t *head,
                                       wait_queue_t *old)
{
#if WAITQUEUE_DEBUG
    if (!old)
        WQ_BUG();
    CHECK_MAGIC(old->__magic);
#endif
    list_del(&old->task_list);
}



#endif /*__SYS_WAIT_H*/
