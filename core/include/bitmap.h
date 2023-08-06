#ifndef __LIB_BITMAP_H
#define __LIB_BITMAP_H

#include <ostype.h>
#define FULL_MASK 1
struct bitmap{
    int_8 *bits;
    int_32 map_bytes_length; // length count in byte
};

void init_bitmap(struct bitmap *bmap);
void set_value_bitmap(struct bitmap *bmap, int_32 bit_pos, int_8 value);
int_32 set_block_value_bitmap(struct bitmap *bmap, int_32 cnt, int_8 value);
int_32 get_value_bitmap( struct bitmap *bmap,  int_32 bit_pos);

#endif
