#include "poudland_p0.h"

#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>

#define BMP_HEADER_BYTES 122U
#define BMP_FILE_HEADER_BYTES 14U
#define BMP_INFO_MIN_BYTES 40U
#define BMP_COMPRESSION_RGB 0U
#define BMP_COMPRESSION_BITFIELDS 3U
#define POUDLAND_P0_SEEK_SET 1U

static uint_16 read_le16(const uint_8 *data)
{
        return (uint_16) data[0] | (uint_16) data[1] << 8;
}

static uint_32 read_le32(const uint_8 *data)
{
        return (uint_32) data[0] | (uint_32) data[1] << 8 |
               (uint_32) data[2] << 16 | (uint_32) data[3] << 24;
}

static int_32 read_exact(int_32 fd, void *buffer, uint_32 size)
{
        uint_8 *next = buffer;
        uint_32 remaining = size;

        while (remaining != 0) {
                int_32 result = read(fd, next, remaining);

                if (result <= 0)
                        return result == 0 ? -EPROTO : result;
                next += result;
                remaining -= (uint_32) result;
        }
        return 0;
}

static bool bmp_header_valid(const uint_8 *header, uint_32 *width,
                             uint_32 *height, uint_32 *pixel_offset,
                             bool *top_down, bool *has_alpha)
{
        uint_32 file_size = read_le32(header + 2);
        uint_32 offset = read_le32(header + 10);
        uint_32 info_size = read_le32(header + 14);
        uint_32 raw_width = read_le32(header + 18);
        uint_32 raw_height = read_le32(header + 22);
        int_32 signed_height = (int_32) raw_height;
        uint_32 compression = read_le32(header + 30);
        uint_32 red_mask;
        uint_32 green_mask;
        uint_32 blue_mask;
        uint_32 alpha_mask = 0;
        unsigned long long pixel_bytes;

        if (header[0] != 'B' || header[1] != 'M' ||
            info_size < BMP_INFO_MIN_BYTES ||
            raw_width == 0 || raw_width > POUDLAND_P0_CURSOR_LIMIT ||
            (raw_width & 0x80000000U) != 0 || raw_height == 0 ||
            raw_height == 0x80000000U || read_le16(header + 26) != 1U ||
            read_le16(header + 28) != 32U ||
            (compression != BMP_COMPRESSION_RGB &&
             compression != BMP_COMPRESSION_BITFIELDS))
                return false;
        *height = signed_height < 0 ? (uint_32) -signed_height : raw_height;
        if (*height > POUDLAND_P0_CURSOR_LIMIT ||
            offset < BMP_FILE_HEADER_BYTES + info_size)
                return false;
        pixel_bytes = (unsigned long long) raw_width * *height * 4U;
        if ((unsigned long long) offset + pixel_bytes > file_size)
                return false;

        if (compression == BMP_COMPRESSION_BITFIELDS) {
                if (info_size < 56U)
                        return false;
                red_mask = read_le32(header + 54);
                green_mask = read_le32(header + 58);
                blue_mask = read_le32(header + 62);
                alpha_mask = read_le32(header + 66);
                if (red_mask != 0x00ff0000U ||
                    green_mask != 0x0000ff00U ||
                    blue_mask != 0x000000ffU ||
                    (alpha_mask != 0 && alpha_mask != 0xff000000U))
                        return false;
        }
        *width = raw_width;
        *pixel_offset = offset;
        *top_down = signed_height < 0;
        *has_alpha = compression == BMP_COMPRESSION_BITFIELDS &&
                     alpha_mask == 0xff000000U;
        return true;
}

int_32 poudland_p0_bmp_load_cursor(const char *path,
                                   struct poudland_p0_cursor *cursor)
{
        uint_8 header[BMP_HEADER_BYTES];
        uint_8 row[POUDLAND_P0_CURSOR_LIMIT * sizeof(uint_32)];
        uint_32 width;
        uint_32 height;
        uint_32 pixel_offset;
        bool top_down;
        bool has_alpha;
        int_32 fd;
        int_32 result;

        if (path == NULL || cursor == NULL)
                return -EINVAL;
        cursor->width = 0;
        cursor->height = 0;
        cursor->pixels = NULL;
        fd = open(path, O_RDONLY);
        if (fd < 0)
                return fd;
        result = read_exact(fd, header, sizeof(header));
        if (result != 0 ||
            !bmp_header_valid(header, &width, &height, &pixel_offset,
                              &top_down, &has_alpha)) {
                (void) close(fd);
                return result != 0 ? result : -EPROTO;
        }
        cursor->pixels = malloc(width * height * sizeof(uint_32));
        if (cursor->pixels == NULL) {
                (void) close(fd);
                return -ENOMEM;
        }
        if (lseek(fd, (int_32) pixel_offset, POUDLAND_P0_SEEK_SET) !=
            (int_32) pixel_offset) {
                result = -EPROTO;
                goto fail;
        }
        for (uint_32 source_y = 0; source_y < height; source_y++) {
                uint_32 target_y = top_down ? source_y :
                                             height - source_y - 1U;

                result = read_exact(fd, row, width * sizeof(uint_32));
                if (result != 0)
                        goto fail;
                for (uint_32 x = 0; x < width; x++) {
                        uint_32 pixel = read_le32(row + x * sizeof(uint_32));

                        if (!has_alpha)
                                pixel |= 0xff000000U;
                        cursor->pixels[target_y * width + x] = pixel;
                }
        }
        result = close(fd);
        if (result != 0)
                goto release;
        cursor->width = width;
        cursor->height = height;
        return 0;

fail:
        (void) close(fd);
release:
        free(cursor->pixels);
        cursor->pixels = NULL;
        return result;
}

void poudland_p0_cursor_release(struct poudland_p0_cursor *cursor)
{
        if (cursor == NULL)
                return;
        free(cursor->pixels);
        cursor->pixels = NULL;
        cursor->width = 0;
        cursor->height = 0;
}
