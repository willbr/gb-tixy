#define INCLUDE_TABLES
#include "shared.h"

#include <string.h>

#ifdef SDL
#include "shim_sdl.h"
#else
#include <gb/gb.h>
#include <gb/cgb.h>
#endif


i8 t = 0;
u8 screen[256];
u8 formula_idx = 0;
u8 prev_pad = 0;

/* CGB background palette 0, colour 0 = lightest like the DMG */
const palette_color_t tixy_palette[4] = {
    RGB8(255, 255, 255),
    RGB8(255, 120, 130),
    RGB8(200,  20,  40),
    RGB8( 10,  10,  10),
};


/*
 * Each formula gets its own 16x16 loop so the expression is inlined:
 * no call per cell, and x, y, i are locals.
 */
#define FORMULA(name, expr)                       \
    void name(void)                               \
    {                                             \
        u8 *p = screen;                           \
        i8 i = 0;                                 \
        i8 x;                                     \
        i8 y;                                     \
        for (y = 0; y != 16; y += 1) {            \
            for (x = 0; x != 16; x += 1) {        \
                *p++ = i8_abs(expr);              \
                i++;                              \
            }                                     \
        }                                         \
    }

FORMULA(f_scroll,   x + (t << 2))
FORMULA(f_wipe,     y - t)
FORMULA(f_diagonal, y - x)
FORMULA(f_stripes,  ((i&3) - (y&3)) << 4)
FORMULA(f_grid,     ((x&3) && (y&3)) ? 0x7f : 0)
FORMULA(f_pulse,    gb_sin(t))
FORMULA(f_sin_t_y,  gb_sin(t + y) << gb_sqrt(x))
FORMULA(f_sin_y,    gb_sin(y) + t)
FORMULA(f_sin_x,    gb_sin(x) + t)
FORMULA(f_wave,     (gb_sin(x) << 2) + (gb_cos(y) << 2) + (t << 1))
FORMULA(f_sin_i,    (gb_sin(i) >> 1) + t)
FORMULA(f_sin_t_x,  gb_sin(t) >> x)
FORMULA(f_square,   x>3 && y>3 && x<12 && y<12 ? (t << 3) : (i + t))
FORMULA(f_noise,    gb_cos(t + i + x * y))
FORMULA(f_checker,  (gb_cos(x >> 3) - gb_cos((x << 1) - (t << 2)) - y + 14) != 0 ? 0x7f : i)
FORMULA(f_blood,    y - t*3 + 9 + 3*gb_cos(x*3-t) - 5*gb_sin(x*7))
FORMULA(f_softwipe, (x-y) - (gb_sin(t << 1) >> 3) * 3)
FORMULA(f_atan,     gb_atan(i>>2) + gb_sin(x) + gb_cos(y) + (t << 2))
FORMULA(f_tangle,   gb_sin(t) * gb_tan(t))
FORMULA(f_shift,    (y-x) << ((t >> 2) & 7))

void (*formulas[])(void) = {
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


void
init(void)
{
    u8 row[32];
    u8 y = 0;

    DISPLAY_OFF;

    set_bkg_data(0, 128, (void*)&dither_gfx);

    memset(row, 0x7f, sizeof row);
    for (y = 0; y != 32; y += 1)
        set_bkg_tiles(0, y, 32, 1, row);

    if (_cpu == CGB_TYPE) {
        set_bkg_palette(0, 1, tixy_palette);

        /* every tile uses palette 0 */
        memset(row, 0, sizeof row);
        VBK_REG = VBK_ATTRIBUTES;
        for (y = 0; y != 32; y += 1)
            set_bkg_tiles(0, y, 32, 1, row);
        VBK_REG = VBK_TILES;
    }

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;
}


void
update(void)
{
    u8 pad = joypad();
    u8 pressed = pad & ~prev_pad;
    prev_pad = pad;

    if (pressed & (J_A | J_RIGHT))
        formula_idx = formula_idx == NUM_FORMULAS - 1 ? 0 : formula_idx + 1;
    if (pressed & (J_B | J_LEFT))
        formula_idx = formula_idx == 0 ? NUM_FORMULAS - 1 : formula_idx - 1;

    formulas[formula_idx]();
    t++;
}


void
render(void)
{
    wait_vbl_done();
    set_bkg_tiles(2,1, 16, 16, screen);
}


int
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
