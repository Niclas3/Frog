#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <frog/types.h>
#include <kernel/frogfs_image_test.h>
#include <kernel/vfs.h>

#define FROGFS_IMAGE_TEST_BUFFER_SIZE 512U

extern const uint_8 _binary_b_bmp_start[];
extern const uint_8 _binary_b_bmp_end[];

int frogfs_image_test_verify_manifest(void)
{
        const uint_8 *expected = _binary_b_bmp_start;
        uint_32 expected_size =
            (uint_32) (_binary_b_bmp_end - _binary_b_bmp_start);
        uint_8 *buffer = kmalloc(FROGFS_IMAGE_TEST_BUFFER_SIZE);
        struct file *file = NULL;
        uint_32 offset = 0;
        int result;

        if (!buffer)
                return -ENOMEM;
        result = vfs_open_file("/test/share/poudland/cursor.bmp", O_RDONLY,
                               &file);
        if (result < 0)
                goto out;
        while (offset < expected_size) {
                uint_32 remaining = expected_size - offset;
                uint_32 request = remaining < FROGFS_IMAGE_TEST_BUFFER_SIZE
                                       ? remaining
                                       : FROGFS_IMAGE_TEST_BUFFER_SIZE;

                result = vfs_read(file, buffer, request);
                if (result != (int_32) request ||
                    memcmp(buffer, expected + offset, request) != 0) {
                        result = -EIO;
                        goto out;
                }
                offset += request;
        }
        result = vfs_read(file, buffer, 1) == 0 ? 0 : -EIO;
out:
        if (file && vfs_close(file) != 0 && result == 0)
                result = -EIO;
        kfree(buffer);
        return result;
}
