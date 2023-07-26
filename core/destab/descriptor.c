#include "../include/descriptor.h"

extern void _save_gdtr(int_32 *data);
extern void _load_gdtr(int_16 limit, int_32 addr);

extern void _save_idtr(int_32 *data);
extern void _load_idtr(int_16 limit, int_32 addr);
typedef enum DescriptorTable_Type{
    GDT=0,
    IDT,
    LDT
} DT_type;

void __get_from_descriptor_table_register(Descriptor_REG *data, DT_type type){
    //e.g.
    //0xc820 0057 low  data[0]
    //0x0000 abcd high data[1]
    
    //0xabcd c820 address
    //0x0057      limit
    int_32 reg_data[2] = {0};
    // Use this get 64 bits data in gdtr.
    if(type == GDT){
        _save_gdtr(reg_data); 
    }else if(type == IDT){
        _save_idtr(reg_data); 
    }else if(type == LDT){
    }else{
        return;
    }
    // The lower 16 bits represents limits. It's about 16bits
    int_16 limit = reg_data[0] & 0x0000ffff; 
    // Higher 16 bits plus next 32 bits' lower 16bits equal address. it's about
    // 32 bits
    int_32 addr  = (reg_data[0] >> 16) | (reg_data[1] << 16);
    data->address = addr;
    data->limit   = limit;
    return;
}

void save_gdtr(Descriptor_REG *data){
    __get_from_descriptor_table_register(data, GDT);
}

void load_gdtr(Descriptor_REG *data){
    int_16 limit = data->limit;
    int_32 addr  = data->address;
    _load_gdtr(limit, addr);
    return;
}

void save_idtr(Descriptor_REG *data){
    __get_from_descriptor_table_register(data, IDT);
}

void load_idtr(Descriptor_REG *data){
    int_16 limit = data->limit;
    int_32 addr  = data->address;
    _load_idtr(limit, addr);
    return;
}

void create_gate(Gate_Descriptor *gd,
                 Selector selector,
                 int_32 offset,
                 int_8 attribute,
                 int_8 dcount)
{
    gd->offset_15_0 = offset & 0xffff;
    gd->selector = selector;
    gd->dw_count = dcount;
    /* gd->access_right = (attribute << 8) & 0xff00; */
    gd->access_right = attribute ;
    gd->offset_32_16 = (offset >> 16) & 0xffff;
    return;
}

/* Params:
 * int_32 limit;          20 bits=16+4 bits 
   int_8 attribute;        4 bits; G,B/D,L,AVL 
   int_8 access_right);    8 bits; p,DPL,S,TYPE 
 */
void create_descriptor(Segment_Descriptor *sd,
                       int_32 base_address,
                       int_32 limit,
                       int_8 access_right,//P, DPL ,S and type
                       int_8 attribute) //G, B/D, L, AVL,
{
    sd->base_address_15_00 = base_address & 0xffff;
    sd->limit_15_00= limit & 0xffff;
    sd->base_address_24_31 = base_address >> 24;
    sd->base_address_16_23 = (base_address >> 16) & 0xff;

    sd->access_right = access_right;
    sd->attribute_and_limit_16_19 = (attribute << 4) + ((limit >> 16) & 0xf); // limit 16~19
    return;
}

//  15                                  3  2  1     0
// ┌─────────────────────────────────────┬───┬──────┐
// │                                     │ T │ RPL  │
// │                Index                │ I │      │
// └─────────────────────────────────────┴───┴──────┘
//
// TI : Table Indicator
// RPL : 0 -> GDT
//       1 -> LDT
// Requested Privilege Level(RPL)
// 2 bytes
Selector create_selector(int_16 index, char ti, char rpl) 
{
    return (index << 3) + ti + rpl;
}

