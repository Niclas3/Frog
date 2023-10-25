#ifndef __IPC_H
#define __IPC_H
#include <const.h>
#include <ostype.h>

/* Check that the message payload type doesn't grow past the maximum IPC payload size.
 * This is a compile time check. */
#define _ASSERT_MSG_SIZE(msg_type) \
    typedef int _ASSERT_##msg_type[/* CONSTCOND */sizeof(msg_type) == 56 ? 1 : -1]

/* Process */
#define SENDING   0x02	/* set when proc trying to send */
#define RECEIVING 0x04	/* set when proc trying to recv */

/* ipc function */
#define SEND		1
#define RECEIVE		2
#define BOTH		3	/* BOTH = (SEND | RECEIVE) */

#define SEND_MAX        50   // max number of sending queue each process(task)

// TASK number
#define NR_TASKS	2
#define NR_PROCS	3
//Task Number
#define ANY_TASK        0xffffffff
#define NO_TASK         0xfffffff7
#define INTR_TASK       -10
#define TASK_MAX        (NR_TASKS + NR_PROCS)

typedef struct thread_control_block TCB_t;

typedef uint_32 pid_t; // process id for IPC
/**
 * @enum msgtype
 * @brief MESSAGE types
 */
typedef enum kwaak_type{
	/* 
	 * when hard interrupt occurs, a msg (with type==HARD_INT) will
	 * be sent to some tasks
	 */
	HARD_INT = 1,

	/* SYS task */
	GET_TICKS,
        /* FS task*/
        OPEN_FILE,
} msg_type;

typedef struct {
	uint_8 data[56];
} mess_u8;
_ASSERT_MSG_SIZE(mess_u8);

typedef struct {
	uint_16 data[28];
} mess_u16;
_ASSERT_MSG_SIZE(mess_u16);

typedef struct {
	uint_32 data[14];
} mess_u32;
_ASSERT_MSG_SIZE(mess_u32);


// kwaak name from this https://medium.com/@radams20/what-does-the-frog-say-792b9f50b870
typedef struct kwaak_msg {
	pid_t m_source;		        /* who sent the message it is a task number*/
	msg_type m_type;		/* what kind of message is it */
	union {
		mess_u8			m_u8;
		mess_u16		m_u16;
		mess_u32		m_u32;
		uint_8 size[56];	/* message payload may have 56 bytes at most */
	};
} message;                  // __ALIGNED(16);

TCB_t* pid2proc(pid_t id);
void reset_msg(message *msg);
uint_32 msg_send(TCB_t *sender, pid_t dest, message *msg);
uint_32 msg_receive(TCB_t *receiver, pid_t src, message *msg);

#endif
