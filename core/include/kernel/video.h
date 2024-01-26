#pragma once
#include <ostype.h>

#define IO_VID_WIDTH  0x5001
#define IO_VID_HEIGHT 0x5002
#define IO_VID_DEPTH  0x5003
#define IO_VID_STRIDE 0x5007
#define IO_VID_ADDR   0x5004
#define IO_VID_VBE_MODE 0x500a

#define IO_VID_SIGNAL 0x5005
#define IO_VID_SET    0x5006
#define IO_VID_DRIVER 0x5008
#define IO_VID_REINIT 0x5009

#ifndef VBE_THINGS
#define VBE_THINGS
// VBE infomation
typedef struct vbe_info_structure {
    char signature[4];  // must be "VESA" to indicate valid VBE support
    uint_16 version;    // VBE version; high byte is major version, low byte is
                        // minor version
    uint_32 oem_str_pointer;      // segment:offset pointer to OEM
    uint_32 capabilities;         // bitfield that describes card capabilities
    uint_32 video_modes_pointer;  // segment:offset pointer to list of supported
                                  // video modes
    uint_16 video_memory;         // amount of video memory in 64KB blocks
    uint_16 software_rev;         // software revision
    uint_32 vendor_pointer;       // segment:offset to card vendor string
    uint_32 product_name_pointer;  // segment:offset to card model name
    uint_32 product_rev_pointer;   // segment:offset pointer to product revision
    char reserved[222];            // reserved for future expansion
    char oem_data[256];  // OEM BIOSes store their strings in this area
} __attribute__((packed)) vbe_info_t;

// VBE Mode info block - holds current graphics mode values
typedef struct vbe_mode_info {
    // Mandatory info for all VBE revisions
    uint_16 mode_attributes;
    uint_8 window_a_attributes;
    uint_8 window_b_attributes;
    uint_16 window_granularity;
    uint_16 window_size;
    uint_16 window_a_segment;
    uint_16 window_b_segment;
    uint_32 window_function_pointer;
    uint_16 bytes_per_scanline;  // stride

    // Mandatory info for VBE 1.2 and above
    uint_16 x_resolution;
    uint_16 y_resolution;
    uint_8 x_charsize;
    uint_8 y_charsize;
    uint_8 number_of_planes;
    uint_8 bits_per_pixel;  // depth
    uint_8 number_of_banks;
    uint_8 memory_model;
    uint_8 bank_size;
    uint_8 number_of_image_pages;
    uint_8 reserved1;

    // Direct color fields (required for direct/6 and YUV/7 memory models)
    uint_8 red_mask_size;
    uint_8 red_field_position;
    uint_8 green_mask_size;
    uint_8 green_field_position;
    uint_8 blue_mask_size;
    uint_8 blue_field_position;
    uint_8 reserved_mask_size;
    uint_8 reserved_field_position;
    uint_8 direct_color_mode_info;

    // Mandatory info for VBE 2.0 and above
    uint_32
        physical_base_pointer;  // Physical address for flat memory frame buffer
    uint_32 reserved2;          // off_screen_mem_off
    uint_16 reserved3;          // off_screen_mem_size

    // Mandatory info for VBE 3.0 and above
    uint_16 linear_bytes_per_scanline;
    uint_8 bank_number_of_image_pages;
    uint_8 linear_number_of_image_pages;
    uint_8 linear_red_mask_size;
    uint_8 linear_red_field_position;
    uint_8 linear_green_mask_size;
    uint_8 linear_green_field_position;
    uint_8 linear_blue_mask_size;
    uint_8 linear_blue_field_position;
    uint_8 linear_reserved_mask_size;
    uint_8 linear_reserved_field_position;
    uint_32 max_pixel_clock;

    uint_8 reserved4[190];  // Remainder of mode info block

} __attribute__((packed)) vbe_mode_info_t;

#endif

