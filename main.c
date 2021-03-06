#ifdef SDL

#else

#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <gb/cgb.h>
#include <string.h>
#include <stdio.h>

#endif

#include "shared.h"

u8 t = 0;
i8 i = 0;
u8 x = 0;
u8 y = 0;

struct gb_tile tile;
u8 *screen = NULL;
u8 *iscreen[256];
u8 j = 0;
char msg_buf[128];


void
init(void)
{
    unsigned int j = 0;
    u8 i = 0;
    u8 x = 0;
    u8 y = 0;
    u8 c = 0;

    memset(&tile, 0, sizeof tile);

    for (i = 0; i != 128; i += 1) {
        if (x == 15) {
            x = 0;
            y += 1;
        }

        if (y == 15) {
            y = 0;
        }

        /*pset(&tile, x, y, ++c & 3);*/
        /*x += 1;*/

        pset(&tile, ++x & 15, ++y % 15, ++c & 3);
        for (y = 0; y != 16; y += 1) {
            for (x = 0; x != 16; x += 1) {
                c = closest_colour(x, y, i);
                pset(&tile, x, y, c);
            }
        }

        set_bkg_data(i + 0, 1, (void*)&tile);
    }

    screen = (unsigned char *)0x9800;
    for (j = 1024; j; j -= 1)  {
        *screen = 0x7f;
        screen += 1;
    }

}


void
main(void)
{
    unsigned char **p = NULL;
    unsigned char **p_start = NULL;
    unsigned char **p_end = NULL;

    DISPLAY_OFF;
    init();

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;

    screen = (unsigned char *)0x9800;
    screen += MAP_WIDTH;

    for (y = 0; y != 16; y += 1) {
        screen += 2;
        for (x = 0; x != 16; x += 1) {
            iscreen[j++] = screen++;
        }
        screen += 14;
    }

    p_start = &iscreen[0];
    p_end = &iscreen[255] + 1;

    p = p_start;
    i = 0;

    for (;;) {

#define tiles_per_frame 16

        j = tiles_per_frame;

        /*BGB_MESSAGE_FMT(msg_buf, "i: %d", i);*/

        wait_vbl_done();
        while (j--) {
            if (p == p_end)
                p = p_start;

            if (x == 16) {
                x = 0;
                y += 1;
            }

            y &= 0x0f;

#define CMD (i + t) + x >> 3
            **(p++) = (CMD) & 0x7f;

            x += 1;
            i += 1;
        }
        t += 1;
    }
}

