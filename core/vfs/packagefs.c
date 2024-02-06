// For client - server IPC
#include <debug.h>
#include <device/devno-base.h>
#include <device/ide.h>
#include <errno-base.h>
#include <fs/dir.h>
#include <fs/fcntl.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/packagefs.h>
#include <list.h>
#include <string.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/semaphore.h>
#include <sys/threads.h>

#include <ioqueue.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

#define CLIENT_MARK 0xdeadbeef
#define MAX_PACKET_SIZE 1024

#define intr_lock(lock, status)                   \
    do {                                          \
        enum intr_status status = intr_disable(); \
        lock_fetch(lock);                         \
    } while (0)

#define intr_unlock(lock, status) \
    do {                          \
        lock_release((lock));     \
        intr_set_status(status);  \
    } while (0)

// contain all exist servers list

// server structure
typedef struct {
    char *name;  // server name
    struct list_head server_target;
    struct lock lock;
    CircleQueue *msg_list;
    struct list_head clients;  // all clients communicate to this server
} pkg_server_t;

// client structure
typedef struct {
    pkg_server_t *server;            // server this client to connect
    struct list_head client_target;  // target to server clients list
    CircleQueue *msg_list;           // event message list to this client
} pkg_client_t;

typedef struct {
    uint_32 size;
    pkg_client_t *source;
    char data[];
} package_t;

typedef struct server_write_header {
    pkg_client_t *target;
    uint_8 data[];
} header_t;

typedef struct {
    struct list_head servers;
    pkg_server_t *cur_server;
} pkg_monitor;


bool is_pkg_file(struct partition *part, int_32 inode_nr)
{
    // can not find client from inode_open
    struct inode *inode_char_file = inode_open(part, inode_nr);
    return IS_FT_FIFO(inode_char_file);
}

bool is_pkg_fd(int_32 fd)
{
    uint_32 g_fd = fd_local2global(fd);
    return IS_FT_FIFO(g_file_table[g_fd].fd_inode);
}

/**
 * Receive a packet from giving msg_list
 *
 *****************************************************************************/
static uint_32 receive_packet(CircleQueue *msg_list, package_t **out)
{
    // 4 to read first 4 bytes get real size
    uint_32 data_size = 0;
    ioqueue_get_data(msg_list, (char *) &data_size, 4);
    package_t *tmp = sys_malloc(data_size + sizeof(package_t));
    tmp->size = data_size;
    uint_32 res = ioqueue_get_data(msg_list, (char *) tmp + 4,
                                   data_size + sizeof(package_t) - 4);
    *out = tmp;
    return res + 4;
}

// Client will send message to server
// Only client process will call this function
static int send_to_server(pkg_server_t *serv,
                          pkg_client_t *c,
                          uint_32 size,
                          void *data)
{
    // make a package
    uint_32 psize = sizeof(package_t) + size;
    package_t *pack = sys_malloc(psize);
    pack->size = size;
    pack->source = c;
    memcpy(pack->data, data, size);
    // add to server FIFO
    // 2. If this server message list is full, block self process to wait server
    //    flush server message list
    // test serv->msg_list left size is bigger than this package size send the
    // data
    uint_32 res = ioqueue_put_data(serv->msg_list, (char *) pack, psize);
    return res;
}

static int send_to_client(pkg_server_t *serv,
                          pkg_client_t *c,
                          uint_32 size,
                          void *data)
{
    // make a package
    uint_32 psize = sizeof(package_t) + size;
    package_t *pack = sys_malloc(psize);
    pack->size = size;
    pack->source = NULL;  // AKA server
    memcpy(pack->data, data, size);
    // add to Client FIFO
    // 2. if this client message list is full
    uint_32 res = ioqueue_put_data(c->msg_list, (char *) pack, psize);
    return res;
}

// create more clients
static pkg_client_t *create_client(pkg_server_t *server)
{
    TCB_t *cur = running_thread();
    uint_32 *cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    // this memory at kernel for share
    pkg_client_t *client = sys_malloc(sizeof(pkg_client_t));
    cur->pgdir = cur_pagedir_bak;

    client->msg_list = init_ioqueue(4000);
    client->server = server;
    list_add_tail(&client->client_target, &server->clients);
    return client;
}

/* static pkg_server_t *create_server(char *name, pkg_monitor *monitor) */
static pkg_server_t *create_server(char *name)
{
    TCB_t *cur = running_thread();
    uint_32 *cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    // this memory at kernel for share
    pkg_server_t *server = sys_malloc(sizeof(pkg_server_t));
    cur->pgdir = cur_pagedir_bak;

    server->msg_list = init_ioqueue(4000);
    INIT_LIST_HEAD(&server->clients);
    server->name = sys_malloc(strlen(name));
    strcpy(server->name, name);
    lock_init(&server->lock);
    /* list_add_tail(&server->server_target, &monitor->servers); */
    return server;
}

/**
 * create a char type file
 *
 * @return reture a global file table index
 *****************************************************************************/
int_32 packagefs_create(struct partition *part,
                        struct dir *parent_d,
                        char *name,
                        void *target)
{
    uint_8 rollback_step = 0;
    char *buf = sys_malloc(1024);
    if (!buf) {
        //  kprint("Not enough memory for io buf");
        return -1;
    }

    // 1. Need new inode aka create a inode (inode_open())
    uint_32 inode_nr = inode_bitmap_alloc(part);
    if (inode_nr == -1) {
        // TODO:
        //  kprint("Not enough inode bitmap position.");
        return -1;
    }
    ASSERT(inode_nr != -1);

    //--------------------------------------------------------------------
    // alloc memory struct inode at kernel memory
    TCB_t *cur = running_thread();
    uint_32 *cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    // this memory at kernel for share
    struct inode *new_f_inode = sys_malloc(sizeof(struct inode));
    cur->pgdir = cur_pagedir_bak;
    //--------------------------------------------------------------------

    if (!new_f_inode) {
        // TODO:
        //  kprint("Not enough memory for inode .");
        // Recover! Need recover inode bitmap set
        rollback_step = 1;
        goto roll_back;
    }
    new_inode(inode_nr, new_f_inode);

    new_f_inode->i_mode = FT_FIFO << 11;
    new_f_inode->i_zones[0] = (uint_32 *) target;
    new_f_inode->i_count++;
    new_f_inode->i_dev = DNOPKGFS;
    // 2. new dir_entry
    struct dir_entry new_entry;
    new_dir_entry(name, inode_nr, FT_FIFO, &new_entry);
    // 3.get file slot form global file_table
    lock_fetch(&g_ft_lock);
    // global file table index
    uint_32 fd_idx = occupy_file_table_slot();
    if (fd_idx == -1) {
        // TODO:
        //  kprint("Not enough slot at file table.");
        // Recover! Need recover inode bitmap set
        //        ! free new_f_inode
        rollback_step = 2;
        goto roll_back;
    }

    g_file_table[fd_idx].fd_pos = 0;
    g_file_table[fd_idx].fd_inode = new_f_inode;
    g_file_table[fd_idx].fd_inode->i_lock = false;
    lock_release(&g_ft_lock);

    // 4. flush dir_entry to parents directory
    // search new entry first
    int_32 failed = search_dir_entry(part, name, parent_d, &new_entry);
    if (!failed) {
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    } else {
        if (flush_dir_entry(part, parent_d, &new_entry, buf)) {
            // TODO:
            //  kprint("Failed at flush directory entry");
            rollback_step = 3;
            // Recover! Need recover inode bitmap set
            //        ! free new_f_inode
            //        ! clear g_file_table[fd_idx]
            goto roll_back;
        }
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    }

roll_back:
    switch (rollback_step) {
    case 3:
        memset(&g_file_table[fd_idx], 0, sizeof(struct file));
    case 2:
        sys_free(new_f_inode);
    case 1:
        set_value_bitmap(&part->inode_bitmap, inode_nr, 0);
        break;
    }
    sys_free(buf);
    return -1;
}

int_32 open_pkg(struct partition *part,
                struct dir *parent_d,
                char *name,
                uint_32 inode_nr,
                uint_8 flags)
{
    if ((flags & O_CREAT) == O_CREAT) {
        // create server
        /* int_32 fd = sys_open("/dev/pkg", O_RDONLY); */
        /* if (fd == -1) { */
        /*     return -1; */
        /* } */
        /* pkg_monitor *m = (pkg_monitor *) get_file(fd)->fd_inode->i_zones[0];
         */
        /* pkg_server_t *server = server = create_server(name, m); */
        pkg_server_t *server = create_server(name);
        // create a dir entry /dev/pkg/****(name)
        int_32 gidx = packagefs_create(part, parent_d, name, server);
        return install_thread_fd(gidx);
    } else {
        // create client
        pkg_server_t *ser =
            (pkg_server_t *) inode_open(part, inode_nr)->i_zones[0];
        pkg_client_t *client = create_client(ser);
        lock_fetch(&g_ft_lock);
        int_32 gidx = occupy_file_table_slot();
        if (gidx == -1) {
            // TODO:
            // kprint("Not enough global file table slots. when open file");
            return -1;
        }

        TCB_t *cur = running_thread();
        uint_32 *cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        // this memory at kernel for share
        struct inode *client_inode = sys_malloc(sizeof(struct inode));
        cur->pgdir = cur_pagedir_bak;

        client_inode->i_zones[0] = client;
        client_inode->i_mode = FT_FIFO << 11;
        client_inode->i_count++;
        client_inode->i_dev = DNOPKGFS;

        g_file_table[gidx].fd_inode = client_inode;
        g_file_table[gidx].fd_pos = CLIENT_MARK;  // client fd_pos as a mark
        g_file_table[gidx].fd_flag = flags;
        lock_release(&g_ft_lock);
        return install_thread_fd(gidx);
    }
}

uint_32 read_server(struct file *file, void *buf, uint_32 count)
{
    package_t *packet = NULL;
    pkg_server_t *server = (pkg_server_t *) file->fd_inode->i_zones[0];
    uint_32 response_size = receive_packet(server->msg_list, &packet);
    if (response_size < 0)
        return response_size;
    if (!packet)
        return -1;

    if (packet->size + sizeof(package_t) > count) {
        return -1;
    }

    memcpy(buf, packet->data, packet->size);
    uint_32 out = packet->size;

    sys_free(packet);
    return out;
}

uint_32 write_server(struct file *file, const void *buf, uint_32 count)
{
    pkg_server_t *p = (pkg_server_t *) file->fd_inode->i_zones[0];
    header_t *head = (header_t *) buf;

    if (count - sizeof(header_t) > MAX_PACKET_SIZE) {
        return -1;
    }

    if (head->target == NULL) {
        /* Brodcast packet */
        lock_fetch(&p->lock);
        struct list_head *pos;
        list_for_each (pos, &p->clients) {
            pkg_client_t *c = list_entry(pos, pkg_client_t, client_target);
            send_to_client(p, (pkg_client_t *) c, count - sizeof(header_t),
                           head->data);
        }
        lock_release(&p->lock);
        return count;
    } else if (head->target->server != p) {
        return -1;
    }

    return send_to_client(p, head->target, count - sizeof(header_t),
                          head->data) + sizeof(header_t);
}

int_32 ioctl_server(struct file *file, unsigned long request, void *argp)
{
    switch (request) {
    case IO_PACKAGEFS_QUEUE: {
        validate(argp);
        pkg_server_t *s = (pkg_server_t *) file->fd_inode->i_zones[0];
        uint_32 size = ioqueue_size(s->msg_list);

        return size;
    }
    default:
        return -EINVAL;
    }
}
/* close_server() */
/* wait_server()  */
/* check_server() */

uint_32 read_client(struct file *file, void *buf, uint_32 count)
{
    pkg_client_t *c = (pkg_client_t *) file->fd_inode->i_zones[0];
    package_t *packet = NULL;
    uint_32 response_size = receive_packet(c->msg_list, &packet);

    if (response_size < 0)
        return response_size;
    if (!packet)
        return -1;

    if (packet->size + sizeof(package_t) > count) {
        /* printf("pex: read in server would be incomplete\n"); */
        return -1;
    }

    memcpy((char *) buf, packet->data, packet->size);
    uint_32 out = packet->size;

    sys_free(packet);
    return out;
}

uint_32 write_client(struct file *file, const void *buf, uint_32 count)
{
    pkg_client_t *c = (pkg_client_t *) file->fd_inode->i_zones[0];

    if (count > MAX_PACKET_SIZE) {
        /* debug_print(WARNING, "Size of %lu is too big.", size); */
        return -EINVAL;
    }
    /* send_to_client(c->server, c, count, buf); */
    send_to_server(c->server, c, count, buf);
    return count;
}

int_32 ioctl_client(struct file *file, unsigned long request, void *argp)
{
    switch (request) {
    case IO_PACKAGEFS_QUEUE: {
        validate(argp);
        pkg_client_t *c = (pkg_client_t *) file->fd_inode->i_zones[0];
        uint_32 size = ioqueue_size(c->msg_list);

        return size;
    }
    default:
        return -EINVAL;
    }
}
/* close_client()   */
/* wait_client()  */
/* check_client() */


static pkg_monitor *create_packetfs_monitor()
{
    pkg_monitor *monitor = sys_malloc(sizeof(pkg_monitor));
    INIT_LIST_HEAD(&monitor->servers);
    monitor->cur_server = NULL;
    return monitor;
}

void packagefs_init(void)
{
    pkg_monitor *m = create_packetfs_monitor();
    sys_mount_device("/dev/pkg/monitor", DNOPKGFS, m);
}
