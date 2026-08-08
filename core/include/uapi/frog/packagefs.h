#ifndef _UAPI_FROG_PACKAGEFS_H
#define _UAPI_FROG_PACKAGEFS_H

#include <frog/types.h>

#define FROG_PKG_NAME_MAX       31U
#define FROG_PKG_PAYLOAD_MAX    1024U
#define FROG_PKG_HEADER_SIZE    12U
#define FROG_PKG_RECORD_MAX     (FROG_PKG_HEADER_SIZE + FROG_PKG_PAYLOAD_MAX)
#define FROG_PKG_SERVICE_MAX    16U
#define FROG_PKG_CLIENT_MAX     16U

enum frog_pkg_event {
        FROG_PKG_DATA = 1,
        FROG_PKG_DISCONNECT = 2,
        FROG_PKG_WRITABLE = 3,
};

struct frog_pkg_record {
        uint_32 peer_id;
        uint_32 event;
        uint_32 payload_size;
        uint_8 payload[];
};

#endif
