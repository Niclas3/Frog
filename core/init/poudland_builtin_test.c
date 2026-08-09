#include <frog/fcntl.h>
#include <frog/types.h>
#include <kernel/poudland_builtin_test.h>
#include <kernel/vfs.h>

extern const uint_8 _binary_compositor_start[];
extern const uint_8 _binary_compositor_end[];
extern const uint_8 _binary_b_bmp_start[];
extern const uint_8 _binary_b_bmp_end[];

static int install_file(const char *path, const uint_8 *start,
                        const uint_8 *end, bool replace)
{
        uint_32 size = (uint_32) (end - start);
        struct file *file = NULL;
        uint_32 flags = O_WRONLY;
        int result;

        if (replace && vfs_lookup(path) != NULL)
                flags |= O_TRUNC;
        else
                flags |= O_CREAT | O_EXCL;
        result = vfs_open_file(path, flags, &file);
        if (result == 0 && vfs_write(file, start, size) != (int_32) size)
                result = -1;
        if (file != NULL && vfs_close(file) != 0 && result == 0)
                result = -1;
        return result;
}

int poudland_builtin_test_install_assets(void)
{
        int result = install_file("/test/compositor",
                                  _binary_compositor_start,
                                  _binary_compositor_end, true);

        if (result != 0)
                return result;
        if (vfs_lookup("/test/b.bmp") != NULL)
                return 0;
        return install_file("/test/b.bmp", _binary_b_bmp_start,
                            _binary_b_bmp_end, false);
}
