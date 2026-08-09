#include "../core/apps/poudland_p0/poudland_p0.h"
#include "../core/apps/poudland_p0/protocol.h"

static uint_8 framebuffer[16U * 16U * 4U];
static uint_32 backbuffer[16U * 16U];

uint_32 poudland_p0_pack_pixel(const struct frog_fb_info *info,
                              uint_32 xrgb)
{
        (void) info;
        return xrgb;
}

static uint_32 pixel(uint_32 x, uint_32 y)
{
        return backbuffer[y * 16U + x];
}

int main(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_scene scene = {0};

        poudland_p0_protocol_init(&protocol, 16, 16);
        protocol.window_count = 2;
        protocol.windows[0].active = true;
        protocol.windows[0].id = 10;
        protocol.windows[0].z_index = 1;
        protocol.windows[0].bounds = (struct poudland_p0_rect) {
            .x = 1, .y = 1, .width = 10, .height = 10,
        };
        protocol.windows[0].color = 0x00cc5533U;
        protocol.windows[1].active = true;
        protocol.windows[1].id = 20;
        protocol.windows[1].z_index = 0;
        protocol.windows[1].bounds = (struct poudland_p0_rect) {
            .x = 2, .y = 2, .width = 10, .height = 10,
        };
        protocol.windows[1].color = 0x00339966U;

        scene.client_protocol = &protocol;
        scene.display.info.width = 16;
        scene.display.info.height = 16;
        scene.display.info.pitch = 16U * 4U;
        scene.display.info.bits_per_pixel = 32;
        scene.display.framebuffer = framebuffer;
        scene.display.backbuffer = backbuffer;
        poudland_p0_damage(&scene.display,
            (struct poudland_p0_rect) {
                .x = 0, .y = 0, .width = 16, .height = 16,
            });
        if (!poudland_p0_render(&scene) ||
            pixel(2, 2) != 0x00cc5533U ||
            pixel(5, 5) != 0x00cc5533U)
                return 1;

        /* Task 13's initial no-focus frame stays color-only.  Focus adds a
         * border without changing z-order or the window interior. */
        protocol.focused_window_id = 10;
        poudland_p0_damage(&scene.display, protocol.windows[0].bounds);
        if (!poudland_p0_render(&scene) ||
            pixel(2, 2) != 0x00ffffffU ||
            pixel(5, 5) != 0x00cc5533U)
                return 2;
        return 0;
}
