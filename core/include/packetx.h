#ifndef __LIB_PACKAGEX_H
#include <ostype.h>

// order is important must same as packagefs.c type package_t;
typedef struct packetx {
	uint_32  size;
	uint_32 *source;
	uint_8 data[];
} packetx_t;

#define MAX_PACKET_DATA_SIZE 1024
#define PACKET_SIZE (sizeof(packetx_t) + MAX_PACKET_DATA_SIZE)

typedef struct packetx_header {
	uint_32 *target;
	uint_8 data[];
} packetx_header_t;

extern uint_32 pkx_send(int_32 sockfd, uint_32 *rcpt, uint_32 size, char * blob);
extern uint_32 pkx_broadcast(int_32  sockfd, uint_32 size, char * blob);
extern uint_32 pkx_listen(int_32 sockfd, packetx_t* packet);

extern uint_32 pkx_reply(int_32 sockfd, uint_32 size, char * blob);
extern uint_32 pkx_recv(int_32  sockfd, char * blob);
extern uint_32 pkx_query(int_32 sockfd);

extern int_32 pkx_bind(char * target);
extern int_32 pkx_connect(char * target);

#endif
