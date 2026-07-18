#include <frog/bitmap.h>
#include <kernel/assert.h>

void init_bitmap(struct bitmap *bmap)
{
    uint_8 *p_map = bmap->bits;
    uint_32 length = bmap->map_bytes_length;
    for (uint_32 i = 0; i < length; i++) {
        *p_map++ = 0x0;
    }
}

// pos represents bits position
void set_value_bitmap(struct bitmap *bmap, uint_32 bit_pos, uint_8 value)
{
    ASSERT(bmap->map_bytes_length * 8 > bit_pos);
    ASSERT(value == 0 || value == 1);
    uint_32 byte_id = bit_pos / 8;         // find the target byte
    uint_32 bit_in_byte_id = bit_pos % 8;  // find the right bits in target byte
    if (value) {
        *((bmap->bits) + byte_id) |= (FULL_MASK << bit_in_byte_id);
    } else {
        *((bmap->bits) + byte_id) &= ~(FULL_MASK << bit_in_byte_id);
    }
    return;
}

// count as a bit
uint_32 find_block_bitmap(struct bitmap *bmap, uint_32 cnt)
{
    if (!bmap || !bmap->bits || cnt == 0)
        return (uint_32) -1;

    uint_32 bit_count = bmap->map_bytes_length * 8;
    uint_32 run_start = 0;
    uint_32 run_length = 0;

    for (uint_32 bit = 0; bit < bit_count; bit++) {
        if (!get_value_bitmap(bmap, bit)) {
            if (run_length == 0)
                run_start = bit;
            run_length++;
            if (run_length == cnt)
                return run_start;
        } else {
            run_length = 0;
        }
    }

    return (uint_32) -1;
}

uint_32 get_value_bitmap(struct bitmap *bmap, uint_32 bit_pos)
{
    ASSERT(bmap->map_bytes_length * 8 > bit_pos);
    uint_32 byte_id = bit_pos / 8;         // find the target byte
    uint_32 bit_in_byte_id = bit_pos % 8;  // find the right bits in target byte

    return *((bmap->bits) + byte_id) & (FULL_MASK << bit_in_byte_id);
}
