#ifndef __LIB_BITMAP_H
#define __LIB_BITMAP_H

#include <ostype.h>
#define FULL_MASK 1
struct bitmap{
    uint_8 *bits;
    uint_32 map_bytes_length; // length count in byte
};

void init_bitmap(struct bitmap *bmap);
void set_value_bitmap(struct bitmap *bmap, uint_32 bit_pos, uint_8 value);
uint_32 set_block_value_bitmap(struct bitmap *bmap, uint_32 cnt, uint_8 value);
uint_32 get_value_bitmap( struct bitmap *bmap,  uint_32 bit_pos);

#endif
