#ifndef _MM_ARDS_H
#define _MM_ARDS_H
#include <global.h>  // MMAP_INFO_POINTER
/** Address Range Descriptor Structure
 *  
 *  Offset in Bytes		Name		Description
 *  	0	    BaseAddrLow		Low 32 Bits of Base Address
 *  	4	    BaseAddrHigh	High 32 Bits of Base Address
 *  	8	    LengthLow		Low 32 Bits of Length in Bytes
 *  	12	    LengthHigh		High 32 Bits of Length in Bytes
 *  	16	    Type		Address type of  this range.
 *  
 *  The BaseAddrLow and BaseAddrHigh together are the 64 bit BaseAddress of this
 *  range. The BaseAddress is the physical address of the start of the range
 *  being specified.
 *  
 *  The LengthLow and LengthHigh together are the 64 bit Length of this range.
 *  The Length is the physical contiguous length in bytes of a range being
 *  specified.
 *  
 *  The Type field describes the usage of the described address range as defined
 *  in the table below.
 *  
 *  Value	Pneumonic		Description
 *  1   	AddressRangeMemory	This run is available RAM usable by the
 *  				        operating system.
 *  2   	AddressRangeReserved	This run of addresses is in use or reserved
 *  				        by the system, and must not be used by the
 *  				        operating system.
 *  Other	Undefined		Undefined - Reserved for future use.  Any
 *  				        range of this type must be treated by the
 *  				        OS as if the type returned was
 *  				        AddressRangeReserved.
 *
 *  The BIOS can use the AddressRangeReserved address range type to block out
 *  various addresses as "not suitable" for use by a programmable device.
 *
 *  Some of the reasons a BIOS would do this are:
 *
 *    The address range contains system ROM.
 *    The address range contains RAM in use by the ROM.
 *    The address range is in use by a memory mapped system device.
 *    The address range is for whatever reason are unsuitable for a standard
 *    device to use as a device memory space.
 ***/
typedef enum {
    ARDS_address_range_memory = 1,
    ARDS_address_range_reserved = 2,
    ARDS_Undefined
} ARDS_t;

struct memory_map_descriptor {
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    ARDS_t type;
} __attribute__((packed));

#endif
