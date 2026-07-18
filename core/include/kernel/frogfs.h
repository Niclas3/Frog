#ifndef __KERNEL_FROGFS_H
#define __KERNEL_FROGFS_H

#include <frog/types.h>

/* Explicitly discard and reformat the target before mounting it. */
#define FROGFS_MOUNT_FORMAT 0x1

int frogfs_init(void);
int frogfs_init_rollback(void);

#ifdef CONFIG_QEMU_TEST
#define FROGFS_TEST_IO_READ 0x01U
#define FROGFS_TEST_IO_WRITE 0x02U
#define FROGFS_TEST_IO_METADATA 0x04U
#define FROGFS_TEST_IO_DATA 0x08U

/* fail_after is one-based; failures selects consecutive matching I/O calls. */
void frogfs_test_fail_io_after(uint_32 fail_after,
                               uint_32 failures,
                               uint_32 mask);
void frogfs_test_clear_io_failpoint(void);
#endif

#endif
