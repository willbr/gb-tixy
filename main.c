#define INCLUDE_TABLES
#include "shared.h"

#ifdef SDL
#include "shim_sdl.h"
#else
#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <gb/cgb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shim.h"

/* TODO
 *
 * cos
 * tan
 * atan
 *
 * random
 *
 * colour
 * sqrt
 * hyplot
 *
 */

/*#define TIXY_CMD x - t*/
/*#define TIXY_CMD y - t*/
/*#define TIXY_CMD y - t << 2*/
/*#define TIXY_CMD y - t >> 3*/
/*#define TIXY_CMD x << 3*/
/*#define TIXY_CMD y << 3*/
/*#define TIXY_CMD (y % 4) << 5*/
/*#define TIXY_CMD (x % 4) << 5*/
/*#define TIXY_CMD (i % 4) << 5*/
/*#define TIXY_CMD y - x*/
/*#define TIXY_CMD ((y > x) && (14 - x  < y)) ? t : 0*/
/*#define TIXY_CMD i%4 - y%4*/
/*#define TIXY_CMD (x%4 && y%4) ? 0x7f : 0*/
/*#define TIXY_CMD sin(t) >> x*/
#define TIXY_CMD sin(t + y) << sqrt(x)

u8 t = 0;
i8 i = 0;
u8 x = 0;
u8 y = 0;

struct gb_tile tile;
u8 screen[256];
char msg_buf[128];

unsigned char *p = NULL;
unsigned char **p_start = NULL;
unsigned char **p_end = NULL;


void
init(void)
{
    unsigned int j = 0;
    u8 c = 0;

    DISPLAY_OFF;

    memset(&tile, 0, sizeof tile);
    memset(&screen, 0, sizeof screen);

    set_bkg_data(0, 128, (void*)&dither_gfx);

    j = 0x7f;

    for (y = 0; y != 32; y += 1) {
        for (x = 0; x != 32; x += 1) {
            set_bkg_tiles(x,y,1,1, &j);
        }
    }

    /*screen = get_memory(0x9800);*/
    /*for (j = 1024; j; j -= 1)  {*/
        /**screen = 0x7f;*/
        /*screen += 1;*/
    /*}*/

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;

    /*screen = get_memory(0x9800);*/
    /*screen += MAP_WIDTH;*/

    /*for (y = 0; y != 16; y += 1) {*/
        /*screen += 2;*/
        /*for (x = 0; x != 16; x += 1) {*/
            /*iscreen[j++] = screen++;*/
        /*}*/
        /*screen += 14;*/
    /*}*/

    /*p_start = &iscreen[0];*/
    /*p_end = &iscreen[255] + 1;*/

    /*p = p_start;*/
    /*i = 0;*/

    /*BGB_MESSAGE_FMT(msg_buf, "i: %d, %d", i, (unsigned char)i);*/
    /*i = -127;*/
    /*do {*/
        /*BGB_MESSAGE_FMT(msg_buf, "i: %d, %u", i, i);*/
    /*} while (i++ != 127);*/
}


void
update(void)
{
    p = screen;
    for (y = 0; y != 16; y += 1) {
        for (x = 0; x != 16; x += 1) {
            *p = (TIXY_CMD) & 0x7f;
            p++;
            i++;
        }
    }
    t++;
}


void
render(void)
{
        wait_vbl_done();
        set_bkg_tiles(2,1, 16, 16, (u8*)&screen);

        /*BGB_MESSAGE_FMT(msg_buf, "i: %d", i);*/
}


void
main(void)
{
#ifdef SDL
    shim_init();
#endif
    init();

    for (;;) {
#ifdef SDL
        shim_update();
#endif
        update();
        render();
#ifdef SDL
        shim_render();
#endif

    }
}

