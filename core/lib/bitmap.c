#include <bitmap.h>
#include <debug.h>

void init_bitmap(struct bitmap *bmap)
{
    uint_8 *p_map = bmap->bits;
    uint_32 lenght = bmap->map_bytes_length;
    for (uint_32 i = 0; i < lenght; i++) {
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
        /* uint_8 *target = (bmap->bits)+byte_id; */
        /* *target |= (FULL_MASK << bit_in_byte_id); */
        /* bmap->bits[byte_id] |= (FULL_MASK << bit_in_byte_id);          //???
         */
        *((bmap->bits) + byte_id) |= (FULL_MASK << bit_in_byte_id);
    } else {
        /* bmap->bits[byte_id] &= ~(FULL_MASK << bit_in_byte_id);  //??? */
        *((bmap->bits) + byte_id) &= ~(FULL_MASK << bit_in_byte_id);
    }
    return;
}

// count as a bit
uint_32 set_block_value_bitmap(struct bitmap *bmap, uint_32 cnt, uint_8 value)
{
    if(cnt == 0){ return -1; }
    // 1.If cnt bigger than 8 aka 1 byte, then to find continued a byte.
    uint_8 *map_p = bmap->bits;
    uint_32 byte_idx = cnt / 8;
    uint_32 bit_in_byte = cnt % 8;
    if (bit_in_byte) {
        byte_idx++;
    }  // poor ceil(cnt/8)
    while ((*map_p == 0xff) &&
           ((map_p - (bmap->bits)) < bmap->map_bytes_length)) {
        map_p++;
    }
    uint_32 pass_count = map_p - bmap->bits;
    if (pass_count == bmap->map_bytes_length) {
        return -1;
    }
    ASSERT(pass_count < bmap->map_bytes_length);
    // Find maybe start of
    uint_8 *maybe_start = map_p;
    int bit_idx = 0;
    while ((FULL_MASK << bit_idx) & *maybe_start) {
        bit_idx++;
    }
    uint_32 start_idx = 8 * (maybe_start - bmap->bits) + bit_idx;
    if(cnt == 1){ return start_idx; }

    uint_32 other_bits = bmap->map_bytes_length * 8 - start_idx;

    uint_32 next_bit = start_idx + 1;
    uint_32 count = 1;

    start_idx = -1;
    if(next_bit >= bmap->map_bytes_length*8){ return start_idx; }

    while(other_bits-- > 0){
        if(next_bit >= bmap->map_bytes_length*8) break;
        if(!get_value_bitmap(bmap, next_bit)){ // next_bit is unset aka 0
            count++;
        } else {
            count = 0;
        }
        if(count == cnt){
            start_idx = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return start_idx;
}

uint_32 get_value_bitmap(struct bitmap *bmap, uint_32 bit_pos)
{
    ASSERT(bit_pos >= 0);
    ASSERT(bmap->map_bytes_length * 8 > bit_pos);
    uint_32 byte_id = bit_pos / 8;         // find the target byte
    uint_32 bit_in_byte_id = bit_pos % 8;  // find the right bits in target byte

    return *((bmap->bits) + byte_id) & (FULL_MASK << bit_in_byte_id);
}
