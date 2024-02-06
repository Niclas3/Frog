#include <debug.h>
#include <packetx.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syscall.h>

/**
 * Send packet between server and client
 *
 *****************************************************************************/
uint_32 pkx_send(int_32 sockfd, uint_32 *rcpt, uint_32 size, char *blob)
{
    ASSERT(size <= MAX_PACKET_DATA_SIZE);
    packetx_header_t *broadcast = malloc(sizeof(packetx_header_t) + size);
    broadcast->target = rcpt;
    memcpy(broadcast->data, blob, size);
    uint_32 out = write(sockfd, broadcast, sizeof(packetx_header_t) + size);
    free(broadcast);
    return out;
}

/**
 * Server broadcast a packet to all clients connect to the server
 *****************************************************************************/
uint_32 pkx_broadcast(int_32 sockfd, uint_32 size, char *blob)
{
    return pkx_send(sockfd, 0, size, blob);
}

/**
 * Server check self ioqueue if some packet arrived
 *
 *****************************************************************************/
uint_32 pkx_listen(int_32 sockfd, packetx_t *packet)
{
    return read(sockfd, packet, PACKET_SIZE);
}

/**
 * Send data to target sockfd ioqueue
 *****************************************************************************/
uint_32 pkx_reply(int_32 sockfd, uint_32 size, char *blob)
{
    return write(sockfd, blob, MAX_PACKET_DATA_SIZE);
}

/**
 * Receive data from target sockfd ioqueue
 *
 *****************************************************************************/
uint_32 pkx_recv(int_32 sockfd, char *blob)
{
    memset(blob, 0, MAX_PACKET_DATA_SIZE);
    return read(sockfd, blob, MAX_PACKET_DATA_SIZE);
}


uint_32 pkx_query(int_32 sockfd)
{
    return ioctl(sockfd, IO_PACKAGEFS_QUEUE, NULL);
}

/**
 * Start a server and return a server fd if success
 *****************************************************************************/
int_32 pkx_bind(char *target)
{
    char tmp[100];
    if (strlen(target) > 80)
        return -1;
    sprintf(tmp, "/dev/pkg/%s", target);
    int_32 fd = open(tmp, O_CREAT);
    return fd;
}

/**
 * Start a client and return a client fd if success
 *****************************************************************************/
int_32 pkx_connect(char *target)
{
    char tmp[100];
    if (strlen(target) > 80)
        return -1;
    sprintf(tmp, "/dev/pkg/%s", target);
    int_32 fd = open(tmp, O_RDONLY);
    return fd;
}

