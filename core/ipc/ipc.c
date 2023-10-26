#include <debug.h>
#include <ipc.h>
#include <list.h>
#include <string.h>
#include <sys/threads.h>
#include <sys/memory.h>

extern struct list_head process_all_list;

/**
 * Detect the messaging graph contains a cycle.
 * For instance, if we have process trying to send messages like this: 
 * A -> B -> C -> A, then a deadlock occurs, because all of them will wait
 * forever, If no cycles detected, It is considered as safe.
 *
 * continuing asking the dest_process if sending message or not , if it is
 * sending, then checking the target if src or not, if not check dest->p_sendto
 * process and repeating 
 *
 * @param param write here param Comments write here
 * @return return zero if success
 *****************************************************************************/
uint_32 detect_cycle(pid_t src, pid_t dest){
    TCB_t *proc = pid2proc(dest);
    while(1){
        if(proc->p_flags & SENDING) {
            if(proc->p_sendto == src) {
                return 1;
            }
            proc = pid2proc(proc->p_sendto);
        } else {
            break;
        }
    }
    return 0;
}

/**
 * Go though thread_all_list to find target id and return tcb pointer.
 * There are plural thread on same pid in thread_all_list, if it happens choose
 * the one who thread id(tid) is 1.
 *
 * Or there is a list called process_all_list, when process is created it will
 * be put on this list, searching this list also can find target process thread.
 *
 * @param id process id
 * @return the main thread of giving pid
 *****************************************************************************/
boolean pid_helper(struct list_head *proc_tag, int value);
boolean pid_helper(struct list_head *proc_tag, int value)
{
    TCB_t *proc = GET_PROC_FROM_PROCLIST(proc_tag);
    return (proc->pid == value);
}

TCB_t *pid2proc(pid_t id)
{
    ASSERT(!list_is_empty(&process_all_list));
    struct list_head *proc_tag = list_walker(&process_all_list, pid_helper, id);
    if (proc_tag) {
        TCB_t *proc = GET_PROC_FROM_PROCLIST(proc_tag);
        return proc;
    }
    return NULL;
}

/* Reset message to 0
 * */
void reset_msg(message *msg)
{
    memset(msg, 0, sizeof(message));
}

/**
 * <Ring 0>
 * Send a message to the dest proc. If dest process is blocked waiting for the
 * message, copy the message to it and unblock dest process. Or the caller will
 * be blocked and appended to the dest's sending queue.
 *
 * @param sender    The caller, the sender (current)
 * @param dest      To whom task number the message is sent
 * @param msg       The Message
 *
 * @return Zero if success
 * ****************************************************************************/
uint_32 msg_send(TCB_t *sender, pid_t dest, message *msg)
{
    TCB_t *p_sender = sender;
    TCB_t *p_dest = pid2proc(dest);
    ASSERT(p_dest != NULL);
    // Check for deadlock
    if(detect_cycle(sender->pid, dest)) {
        PAINC(">>DEADLOCK<<");
    }
    /*
     * First dest process is waiting for message aka p_dest->status ==
     * THREAD_TASK_RECEIVING second test if dest is expecting p_sender aka
     * p_dest->p_recvfrom == p_sender->pid third or dest is expecting ANY task
     * message
     * */
    if ((p_dest->p_flags & RECEIVING) &&
            (p_dest->p_recvfrom == p_sender->pid) ||
        (p_dest->p_recvfrom == ANY_TASK)) {
        ASSERT(p_dest->p_message.m_source>0);
        ASSERT(msg);
        memcpy(&p_dest->p_message, msg, sizeof(message));

        p_dest->p_flags &= ~RECEIVING;
        p_dest->p_recvfrom = NO_TASK;
        thread_unblock(p_dest);
    } else {  // dest is not expecting the message.
        /**
         * While dest is not expecting p_sender message, then To put p_sender to
         * p_dest's waiting list, and block p_sender process for waiting p_dest
         * receiving message.
         *
         *****************************************************************************/
        p_sender->p_flags |= SENDING;
        ASSERT(p_sender->p_flags == SENDING);
        p_sender->p_sendto = dest;
        /* p_sender->p_message = msg; */
        memcpy(&p_sender->p_message, msg, sizeof(message));

        TCB_t *p;
        if (p_dest->p_sending_queue) {
            p = p_dest->p_sending_queue;
            while (p->p_next_sending)
                p = p->p_next_sending;
            p->p_next_sending = p_sender;
        } else {
            p_dest->p_sending_queue = sender;
        }
        p_sender->p_next_sending = NULL;
        thread_auth_block(p_sender, THREAD_TASK_BLOCKED);

        /* ASSERT(p_sender->p_flags == SENDING); */
        /* ASSERT(p_sender->p_message != 0); */
        /* ASSERT(p_sender->p_recvfrom == NO_TASK); */
        /* ASSERT(p_sender->p_sendto == dest); */
    }

    return 0;
}

/**
 * <Ring 0>
 * Try to get a message from the src proc. If src is blocked sending the message
 * , copy the message from it and unblocked src. Otherwise the caller will be
 * blocked.
 *
 * @param receiver the process receives message
 * @param src      task number of the one, who sent the message,
 * @param msg      the message
 *
 * @return Zero if success
 * */
uint_32 msg_receive(TCB_t *receiver, pid_t src, message *msg)
{
    TCB_t *p_receiver = receiver;
    TCB_t *p_from = NULL;  // aka sender
    TCB_t *prev = NULL;
    int copyok = 0;

    /**
     * TODO:
     *
     * Message from interrupt
     *****************************************************************************/



    /**
     * If no interrupt for p_receiver
     *****************************************************************************/
    if (src == ANY_TASK) {
        if (receiver->p_sending_queue) {
            p_from = p_receiver->p_sending_queue;
            copyok = 1;
        }
    } else if (src >= 0 && src < NR_PROCS + NR_TASKS) {
        p_from = pid2proc(src);

        if ((p_from->p_flags & SENDING) &&
            (p_from->p_sendto == p_receiver->pid)) {
            copyok = 1;
            TCB_t *p = p_receiver->p_sending_queue;
            ASSERT(p);
            while (p) {
                ASSERT(p_from->p_flags & SENDING);
                if (p->pid == src) {
                    break;
                }
                prev = p;
                p = p->p_next_sending;
            }
            ASSERT(p_receiver->p_flags == 0);

        }
    }
    if(copyok){
        if(p_from == p_receiver->p_sending_queue){
            ASSERT(prev == 0);
            p_receiver->p_sending_queue = p_from->p_next_sending;
            p_from->p_next_sending = 0;
        } else {
            ASSERT(prev);
            prev->p_next_sending = p_from->p_next_sending;
            p_from->p_next_sending = 0;
        }
        ASSERT(msg);
        ASSERT(p_from->p_message.m_source > 0);
        //copy message 

        memcpy(msg, &p_from->p_message, sizeof(message));
        reset_msg(&p_from->p_message);
        p_from->p_sendto = NO_TASK;
        p_from->p_flags &= ~SENDING;

        thread_unblock(p_from);
    } else {
        /**
         * no tasks sending any message 
         * set p_flag 
         * p_receiver will not be scheduled utill some one unblock it.
         *****************************************************************************/
        p_receiver->p_flags |= RECEIVING;
        /* p_receiver->p_message = msg; */
        memcpy(&p_receiver->p_message, msg, sizeof(message));
        p_receiver->p_recvfrom = src;
        thread_auth_block(p_receiver, THREAD_TASK_BLOCKED);
        // If and only if, receiver wanted message be set.
        memcpy(msg, &p_receiver->p_message, sizeof(message));
    }

    return 0;
}
