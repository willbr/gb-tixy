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


typedef i8 (*formula_t)(void);

i8 t = 0;
u8 frame = 0;
i8 i = 0;
i8 x = 0;
i8 y = 0;

u8 formula_idx = 0;

i8 f_scroll(void)   { return x + (t << 2); }
i8 f_wipe(void)     { return y - t; }
i8 f_diagonal(void) { return y - x; }
i8 f_stripes(void)  { return (i%4 - y%4) << 4; }
i8 f_grid(void)     { return (x%4 && y%4) ? 0x7f : 0; }
i8 f_pulse(void)    { return gb_sin(t); }
i8 f_sin_t_y(void)  { return gb_sin(t + y) << gb_sqrt(x); }
i8 f_sin_y(void)    { return gb_sin(y) + t; }
i8 f_sin_x(void)    { return gb_sin(x) + t; }
i8 f_wave(void)     { return (gb_sin(x) << 2) + (gb_cos(y) << 2) + (t << 1); }
i8 f_sin_i(void)    { return (gb_sin(i) >> 1) + t; }
i8 f_sin_t_x(void)  { return gb_sin(t) >> x; }
i8 f_square(void)   { return x>3 && y>3 && x<12 && y<12 ? (t << 3) : (i + t); }
i8 f_noise(void)    { return gb_cos(t + i + x * y); }
i8 f_checker(void)  { return (gb_cos(x >> 3) - gb_cos((x << 1) - (t << 2)) - y + 14) != 0 ? 0x7f : i; }
i8 f_blood(void)    { return y - t*3 + 9 + 3*gb_cos(x*3-t) - 5*gb_sin(x*7); }
i8 f_softwipe(void) { return (x-y) - (gb_sin(t << 1) >> 3) * 3; }
i8 f_atan(void)     { return gb_atan(i>>2) + gb_sin(x) + gb_cos(y) + (t << 2); }
i8 f_tangle(void)   { return gb_sin(t) * gb_tan(t); }
i8 f_shift(void)    { return (y-x) << ((t >> 2) % 8); }

formula_t formulas[] = {
    f_scroll,
    f_wipe,
    f_diagonal,
    f_stripes,
    f_grid,
    f_pulse,
    f_sin_t_y,
    f_sin_y,
    f_sin_x,
    f_wave,
    f_sin_i,
    f_sin_t_x,
    f_square,
    f_noise,
    f_checker,
    f_blood,
    f_softwipe,
    f_atan,
    f_tangle,
    f_shift,
};

#define NUM_FORMULAS (sizeof(formulas) / sizeof(formulas[0]))

struct gb_tile tile;
u8 screen[256];

unsigned char *p = NULL;


void
init(void)
{
    unsigned char j = 0;

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
#ifdef SDL
    if (shim_next_example) {
        shim_next_example = 0;
        formula_idx = (formula_idx + 1) % NUM_FORMULAS;
    }
#endif

    p = screen;
    for (y = 0; y != 16; y += 1) {
        for (x = 0; x != 16; x += 1) {
            *p = i8_abs(formulas[formula_idx]());
            p++;
            i++;
        }
    }

    frame++;
    if (frame >= 60) {
        frame = 0;
        t++;
    }
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
