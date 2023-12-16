#include <fs/file.h>
#include <fs/pipe.h>
#include <ioqueue.h>
#include <math.h>
#include <sys/memory.h>

extern struct file g_file_table[MAX_FILE_OPEN];

bool is_pipe(int_32 fd)
{
    uint_32 g_fd = fd_local2global(fd);
    return g_file_table[g_fd].fd_flag == PIPE_FLAG;
}

/**
 * pipe() creates a pipe, a unidirectional data channel that can be used for in‐
 * terprocess communication.  The array pipefd is used to return  two  file  de‐
 * scriptors  referring  to  the ends of the pipe.  pipefd[0] refers to the read
 * end of the pipe.  pipefd[1] refers to the write end of the pipe.  Data  writ‐
 * ten  to  the write end of the pipe is buffered by the kernel until it is read
 * from the read end of the pipe.
 * @return
 *     On success, zero is returned.  On error, -1 is returned, errno is set
 *appro‐ priately, and pipefd is left unchanged. On  Linux  (and  other
 *systems), pipe() does not modify pipefd on failure.  A requirement
 *standardizing this behavior was added in POSIX.1-2008  TC2.   The
 *     Linux-specific  pipe2()  system call likewise does not modify pipefd on
 *fail‐ ure.
 *****************************************************************************/
int_32 sys_pipe(int_32 pipefd[2])
{
    int_32 g_fd = occupy_file_table_slot();
    g_file_table[g_fd].fd_inode = get_kernel_page(1);
    init_ioqueue((struct ioqueue *) g_file_table[g_fd].fd_inode);
    if (g_file_table[g_fd].fd_inode == NULL) {
        return -1;
    }
    // reuse fd_flag
    g_file_table[g_fd].fd_flag = PIPE_FLAG;

    // reuse fd_pos as pipe open count
    g_file_table[g_fd].fd_pos = 2;
    pipefd[0] = install_thread_fd(g_fd);
    pipefd[1] = install_thread_fd(g_fd);
    return 0;
}

static int_32 destory_pipe(int_32 g_fd)
{
    struct file file = g_file_table[g_fd];
    if (is_pipe(g_fd) && (--file.fd_pos == 0)) {
        mfree_page(MP_KERNEL, (uint_32) file.fd_inode);
        file.fd_inode = NULL;
        return 0;
    }
    return -1;
}

int_32 open_pipe(int_32 fd)
{
    if (is_pipe(fd)) {
        uint_32 g_fd = fd_local2global(fd);
        struct file pipe = g_file_table[g_fd];
        pipe.fd_pos++;
        return 0;
    }else {
        return -1;
    }
}

void close_pipe(int_32 fd)
{
    uint_32 g_fd = fd_local2global(fd);
    destory_pipe(g_fd);
}

uint_32 read_pipe(int_32 fd, void *buf, uint_32 count)
{
    char *buffer = buf;
    uint_32 bytes_read = 0;
    int_32 g_fd = fd_local2global(fd);
    struct ioqueue *queue = (struct ioqueue *) g_file_table[g_fd].fd_inode;
    uint_32 q_len = ioqueue_length(queue);
    uint_32 size = MIN(q_len, count);
    while (bytes_read < size) {
        *buffer = ioqueue_get_data(queue);
        bytes_read++;
        buffer++;
    }
    return bytes_read;
}

uint_32 write_pipe(int_32 fd, const void *buf, uint_32 count)
{
    char *buffer = buf;
    uint_32 bytes_write = 0;
    int_32 g_fd = fd_local2global(fd);
    struct ioqueue *queue = (struct ioqueue *) g_file_table[g_fd].fd_inode;

    uint_32 q_len = ioqueue_length(queue);
    uint_32 size = MIN(QUEUE_MAX - q_len, count);

    while (bytes_write < size) {
        ioqueue_put_data(*buffer, queue);
        bytes_write++;
        buffer++;
    }
    return bytes_write;
}

uint_32 pipe_length(int_32 fd)
{
    int_32 g_fd = fd_local2global(fd);
    struct ioqueue *queue = (struct ioqueue *) g_file_table[g_fd].fd_inode;
    return ioqueue_length(queue);
}
