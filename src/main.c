#define INCLUDE_TABLES
#include "shared.h"

#include <string.h>

#ifdef SDL
#include "shim_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#else
#include <stdio.h>
#include <gb/gb.h>
#include <gb/bgb_emu.h>
#include <gb/cgb.h>
#endif

#include "shim_shared.h"


/*#include "shim.h"*/

/* TODO
 *
 * tan
 * atan
 *
 * random
 *
 * colour
 * hyplot
 *
 */


/*#define TIXY_CMD x + (t << 2)*/
/*#define TIXY_CMD y - t*/
/*#define TIXY_CMD y - t << 2*/
/*#define TIXY_CMD y - t >> 3*/
/*#define TIXY_CMD x << 3*/
/*#define TIXY_CMD y << 3*/
/*#define TIXY_CMD (y % 8) << 2*/
/*#define TIXY_CMD (x % 4) << 5*/
/*#define TIXY_CMD (i % 4) << 5*/
/*#define TIXY_CMD y - x*/
/*#define TIXY_CMD ((y > x) && (14 - x  < y)) ? t : 0*/
/*#define TIXY_CMD (i%4 - y%4) << 4*/
/*#define TIXY_CMD (x%4 && y%4) ? (t << 4) : 0*/
/*#define TIXY_CMD (x%4 && y%4) ? t : 0*/
/*#define TIXY_CMD sin(y) + t*/
/*#define TIXY_CMD sin(x) + (t << 2)*/
/*#define TIXY_CMD sin(t)*/
/*#define TIXY_CMD i%4 - y%4*/
/*#define TIXY_CMD (x%4 && y%4) ? 0x7f : 0*/
/*#define TIXY_CMD sin(t) >> x*/
#define TIXY_CMD gb_sin(t + y) << gb_sqrt(x)
/*#define TIXY_CMD sin2((y + t>>1)<<4)*/
/*#define TIXY_CMD t*/
/*#define TIXY_CMD (sin(x) % (i >> 1)) + (t << 3)*/
/*#define TIXY_CMD sqrt(sin(x)) + t*/
/*#define TIXY_CMD sin(x) + t*/
/*#define TIXY_CMD (sin(x) << 2) + (cos(y) << 2) + (t << 1)*/
/*#define TIXY_CMD (sin(i) >> 1) + t*/
/*#define TIXY_CMD (y-x) << ((t >> 2) % 8)*/
/*#define TIXY_CMD x>3 && y>3 && x<12 && y<12 ? (t << 3) : (i + t)*/
/*#define TIXY_CMD cos(t + i + x * y)*/
/*#define TIXY_CMD (cos(x >> 3) - cos((x << 1) -(t<<2)) - y + 14) != 0 ? 0x7f : i*/
/*#define TIXY_CMD y-t*3+9+3*cos(x*3-t)-5*sin(x*7)*/
/*#define TIXY_CMD atan(i>>2) + sin(x) + cos(y) + (t << 2)*/
/*#define TIXY_CMD (x-y) - (sin(t << 1) >> 3) * 3*/
#define TIXY_CMD sin(t) * tan(t)

i8 t = 0;
i8 i = 0;
i8 x = 0;
i8 y = 0;

struct gb_tile tile;
u8 screen[256];
char msg_buf[128];

unsigned char *p = NULL;
unsigned char **p_start = NULL;
unsigned char **p_end = NULL;


void
init(void)
{
    unsigned char j = 0;
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

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;
}


void
update(void)
{
    p = screen;
    for (y = 0; y != 16; y += 1) {
        for (x = 0; x != 16; x += 1) {
            *p = i8_abs(TIXY_CMD);
            p++;
            i++;
        }
        /*BGB_MESSAGE_FMT(msg_buf, "*t: %d", t);*/
        /*BGB_MESSAGE_FMT(msg_buf, "*p: %d", *p);*/
    }
    t++;
}


void
render(void)
{
        wait_vbl_done();
        set_bkg_tiles(2,1, 16, 16, (u8*)&screen);

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


