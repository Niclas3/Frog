#ifndef __FROG_POLL_H
#define __FROG_POLL_H
#include <sys/wait.h>
/* These are specified by iBCS2 */
#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

/* The rest seem to be more-or-less nonstandard. Check them! */
#define POLLRDNORM	0x0040
#define POLLRDBAND	0x0080
#define POLLWRNORM	0x0100
#define POLLWRBAND	0x0200
#define POLLMSG		0x0400

struct poll_table_page;
struct file;

typedef struct poll_table_struct {
	int error;
	struct poll_table_page * table;
} poll_table;

extern void __pollwait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p);

static inline void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	if (p && wait_address)
		__pollwait(filp, wait_address, p);
}


static inline void poll_initwait(poll_table* pt)
{
	pt->error = 0;
	pt->table = NULL;
}
extern void poll_freewait(poll_table* pt);

#endif
