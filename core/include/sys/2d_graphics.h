#pragma onces

#include <ostype.h>
#include <sys/FSK_Color.h>

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
typedef struct {
    // Mandatory info for all VBE revisions
    uint_16 mode_attributes;
    uint_8 window_a_attributes;
    uint_8 window_b_attributes;
    uint_16 window_granularity;
    uint_16 window_size;
    uint_16 window_a_segment;
    uint_16 window_b_segment;
    uint_32 window_function_pointer;
    uint_16 bytes_per_scanline;

    // Mandatory info for VBE 1.2 and above
    uint_16 x_resolution;
    uint_16 y_resolution;
    uint_8 x_charsize;
    uint_8 y_charsize;
    uint_8 number_of_planes;
    uint_8 bits_per_pixel;
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

typedef enum {
    BOOT_VBE_MODE,
    BOOT_VGA_MODE,
    BOOT_CGA_MODE,
    BOOT_UNKNOW
} BOOT_GFX_MODE_t;

typedef uint_32 FSK_ARGB_t;
typedef uint_32 FSK_BBP_t;

// global variable for 2d graphics
extern vbe_mode_info_t *g_gfx_mode;

typedef struct {
    uint_32 X;
    uint_32 Y;
} Point;

BOOT_GFX_MODE_t boot_graphics_mode(void);
void twoD_graphics_init(void);
// for test
void clear_screen(uint_32 color);
void draw_pixel(uint_16 X, uint_16 Y, uint_32 color);
void fill_rect_solid(Point top_left, Point bottom_right, uint_32 color);

void draw_2d_gfx_asc_char(int font_size, int x, int y, FSK_BBP_t color, char num);
uint_32 draw_2d_gfx_hex(int font_size, int x, int y, FSK_BBP_t color, int_32 num);
uint_32 draw_2d_gfx_dec(int font_size, int x, int y, FSK_BBP_t color, int_32 num);
void draw_2d_gfx_string(int font_size,
                        int x,
                        int y,
                        FSK_BBP_t color,
                        char *str,
                        uint_32 str_len);

FSK_BBP_t convert_argb(const uint_32 argbcolor);
FSK_ARGB_t convert_bbp(const FSK_BBP_t bbpcolor);

FSK_BBP_t fetch_color(uint_32 X, uint_32 Y);

//Components
void draw_2d_gfx_cursor(uint_32 pos_x, uint_32 pos_y, uint_32 *color);
