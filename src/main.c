#define INCLUDE_TABLES
#include "shared.h"

#include <string.h>

#ifdef SDL
#include "shim_sdl.h"
#else
#include <gb/gb.h>
#include <gb/cgb.h>
#endif

#include "rpn.h"


/*
 * Tile map: circles 0..12, font tiles are ASCII (32..127), and the
 * highlighted (red) font is ASCII + 0x80. Tile 0 is blank.
 */
#define TILE_HI 0x80

#define SCREEN_COLS 20

/* the gallery, in RPN; edits on the GB change the RAM copy in progs[] */
const char *const gallery[] = {
    "y 5 * t + sin",                                     /* Math.sin(y/8+t) */
    "x t 2 << +",
    "y t -",
    "y x -",
    "i 3 & y 3 & - 4 <<",
    "x 3 & 0 > y 3 & 0 > &",
    "t sin",
    "t y + sin x sqrt <<",
    "y sin t +",
    "x sin t +",
    "x sin 2 << y cos 2 << + t 1 << +",
    "i sin 1 >> t +",
    "t sin x >>",
    "x 3 > y 3 > & x 12 < & y 12 < & t 3 << i t + ?",
    "t i + x y * + cos",
    "x 3 >> cos x 1 << t 2 << - cos - y - 14 + 127 i ?",
    "y t 3 * - 9 + x 3 * t - cos 3 * + x 7 * sin 5 * -",
    "x y - t 1 << sin 3 >> 3 * -",
    "i 2 >> atan x sin + y cos + t 2 << +",
    "t sin t tan *",
    "y x - t 2 >> 7 & <<",
};

#define NUM_FORMULAS (sizeof(gallery) / sizeof(gallery[0]))

prog_t progs[NUM_FORMULAS];

i8 t = 0;
u8 *screen = NULL;   /* a page from the rpn arena */
u8 formula_idx = 0;
u8 prog_valid = 0;

u8 editing = 0;
u8 cursor = 0;
u8 last_op = OP_ADD;
u8 last_num = NUM_BASE + 1;

/*
 * Buttons are read every frame in the vblank interrupt, so taps shorter
 * than an update aren't lost and repeat timing doesn't depend on how long
 * the formula takes. pad_pressed collects new presses until input() takes
 * them.
 */
volatile u8 pad_now = 0;
volatile u8 pad_pressed = 0;
u8 repeat_at = 0;

#define DIRS          (J_UP | J_DOWN | J_LEFT | J_RIGHT)
#define REPEAT_DELAY  15    /* frames before a held direction repeats */
#define REPEAT_RATE   4     /* frames between repeats */

u8 status_row[SCREEN_COLS];
u8 prog_row[SCREEN_COLS];
u8 text_dirty = 1;
u8 loop_frames = 0;
u8 last_time = 0;           /* low byte of sys_time: one byte, no torn reads */

/* CGB background palette 0: background, positive, negative, unused */
const palette_color_t tixy_palette[4] = {
    RGB8(  0,   0,   0),
    RGB8(255, 255, 255),
    RGB8(255,  34,  68),
    RGB8( 85,  85,  85),
};

/* DMG shades for the same indices: black, white, light grey, dark grey */
#define TIXY_BGP 0x93


void
select_formula(u8 idx)
{
    formula_idx = idx;
    cursor = progs[idx].len ? progs[idx].len - 1 : 0;
    prog_valid = prog_ok(&progs[idx]);
    text_dirty = 1;
}


void
put_str(u8 *dst, const char *s)
{
    while (*s)
        *dst++ = *s++;
}


void
put_2digits(u8 *dst, u8 v)
{
    dst[0] = '0' + v / 10 % 10;
    dst[1] = '0' + v % 10;
}


/*
 * Row 0: "03/21 edit err   2f" (formula, mode, invalid program, frames per update)
 * Row 17: the program, scrolled so the cursor token is visible.
 */
void
draw_text(void)
{
    u8 full[PROG_MAX * (OP_TEXT_MAX + 1)];
    prog_t *p = &progs[formula_idx];
    u8 len = 0;
    u8 cur_end = 0;
    u8 scroll = 0;
    u8 n = 0;
    u8 w = 0;

    memset(status_row, ' ', SCREEN_COLS);
    put_2digits(status_row, formula_idx + 1);
    status_row[2] = '/';
    put_2digits(status_row + 3, NUM_FORMULAS);
    put_str(status_row + 6, editing ? "edit" : "run");
    if (!prog_valid)
        put_str(status_row + 11, "err");
    if (loop_frames) {
        put_2digits(status_row + 17, loop_frames);
        status_row[19] = 'f';
    }

    for (n = 0; n != p->len; n += 1) {
        if (n)
            full[len++] = ' ';
        w = tok_text(p->tok[n], full + len);
        if (editing && n == cursor) {
            cur_end = len + w;
            while (w--)
                full[len++] |= TILE_HI;
        } else {
            len += w;
        }
    }
    if (editing && p->len == 0) {
        full[0] = '_' | TILE_HI;
        len = 1;
        cur_end = 1;
    }

    if (cur_end > SCREEN_COLS)
        scroll = cur_end - SCREEN_COLS;
    for (n = 0; n != SCREEN_COLS; n += 1)
        prog_row[n] = scroll + n < len ? full[scroll + n] : ' ';
}


/* vblank handler: latch new presses */
void
read_pad(void)
{
    u8 pad = joypad();
    pad_pressed |= pad & ~pad_now;
    pad_now = pad;
}


void
init(void)
{
    u8 row[32];
    u8 y = 0;

    DISPLAY_OFF;

    screen = rpn_init();
    memset(screen, 0, 256);

    set_bkg_data(0,   13, (void*)circle_gfx);
    set_bkg_data(32,  96, (void*)font_gfx);
    set_bkg_data(160, 96, (void*)font_hi_gfx);

    memset(row, 0, sizeof row);
    for (y = 0; y != 32; y += 1)
        set_bkg_tiles(0, y, 32, 1, row);

    if (_cpu == CGB_TYPE) {
        /* double-speed CPU; frame timing (sys_time) is unchanged */
        cpu_fast();
        set_bkg_palette(0, 1, tixy_palette);

        /* every tile uses palette 0 */
        VBK_REG = VBK_ATTRIBUTES;
        for (y = 0; y != 32; y += 1)
            set_bkg_tiles(0, y, 32, 1, row);
        VBK_REG = VBK_TILES;
    } else {
        BGP_REG = TIXY_BGP;
    }

    for (y = 0; y != NUM_FORMULAS; y += 1)
        prog_parse(&progs[y], gallery[y]);
    select_formula(0);

    disable_interrupts();
    add_VBL(read_pad);
    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;
    last_time = (u8)sys_time;
}


/*
 * Run:  Left/Right (or B/A) change formula, Start edits it.
 * Edit: Left/Right move, Up/Down change the token, Select swaps between
 *       number and operator, A inserts a copy after the cursor, B deletes,
 *       Start runs. Directions repeat while held.
 */
void
input(void)
{
    u8 now = (u8)sys_time;
    u8 pressed = 0;
    u8 dirs = pad_now & DIRS;
    prog_t *p = &progs[formula_idx];
    u8 *tok = NULL;
    u8 n = 0;

    disable_interrupts();
    pressed = pad_pressed;
    pad_pressed = 0;
    enable_interrupts();

    /* a new direction press restarts the delay; held ones repeat on time */
    if (pressed & DIRS) {
        repeat_at = now + REPEAT_DELAY;
    } else if (dirs && (u8)(now - repeat_at) < 128) {
        pressed |= dirs;
        repeat_at = now + REPEAT_RATE;
    }

    if (!pressed)
        return;

    if (!editing) {
        if (pressed & (J_A | J_RIGHT))
            select_formula(formula_idx == NUM_FORMULAS - 1 ? 0 : formula_idx + 1);
        if (pressed & (J_B | J_LEFT))
            select_formula(formula_idx == 0 ? NUM_FORMULAS - 1 : formula_idx - 1);
        if (pressed & J_START) {
            editing = 1;
            text_dirty = 1;
        }
        return;
    }

    if (pressed & J_START)
        editing = 0;

    if ((pressed & J_LEFT) && cursor > 0)
        cursor--;
    if ((pressed & J_RIGHT) && cursor + 1 < p->len)
        cursor++;

    /* after moving, so a move and a change in the same frame act together */
    tok = &p->tok[cursor];

    if (p->len && (pressed & (J_UP | J_DOWN))) {
        if (IS_NUM(*tok)) {
            if (pressed & J_UP)
                *tok = *tok == NUM_BASE + NUM_MAX ? NUM_BASE : *tok + 1;
            else
                *tok = *tok == NUM_BASE ? NUM_BASE + NUM_MAX : *tok - 1;
        } else {
            if (pressed & J_UP)
                *tok = *tok == OP_COUNT - 1 ? 0 : *tok + 1;
            else
                *tok = *tok == 0 ? OP_COUNT - 1 : *tok - 1;
        }
    }

    if (p->len && (pressed & J_SELECT)) {
        if (IS_NUM(*tok)) {
            last_num = *tok;
            *tok = last_op;
        } else {
            last_op = *tok;
            *tok = last_num;
        }
    }

    if ((pressed & J_A) && p->len < PROG_MAX) {
        if (p->len == 0) {
            p->tok[0] = OP_T;
            p->len = 1;
        } else {
            for (n = p->len; n != cursor; n -= 1)
                p->tok[n] = p->tok[n - 1];
            p->len++;
            cursor++;
        }
    }

    if ((pressed & J_B) && p->len) {
        for (n = cursor; n + 1 < p->len; n += 1)
            p->tok[n] = p->tok[n + 1];
        p->len--;
        if (cursor && cursor >= p->len)
            cursor--;
    }

    prog_valid = prog_ok(p);
    text_dirty = 1;
}


void
update(void)
{
    input();
    t = (i8)sys_time;
    if (prog_valid)
        prog_eval(&progs[formula_idx], t, screen);
    else
        run_fill(screen, 0);
}


#ifdef SDL
#define copy_grid() set_bkg_tiles(2, 1, 16, 16, screen)
#else
/* rpn_gb.s: screen -> tilemap rows 1..16 at column 2, safe outside vblank */
void copy_grid(void);
#endif


void
draw_screen(void)
{
    copy_grid();
    if (text_dirty) {
        draw_text();
        set_bkg_tiles(0, 0, SCREEN_COLS, 1, status_row);
        set_bkg_tiles(0, 17, SCREEN_COLS, 1, prog_row);
        text_dirty = 0;
    }
}


void
render(void)
{
    u8 now = 0;

    /* no vblank wait: copy_grid checks STAT itself, so copy straight away */
    draw_screen();

    /* at most one update per frame; t wouldn't have changed */
    while ((now = (u8)sys_time) == last_time)
        wait_vbl_done();
    /* only the counter changes here; rebuilding the program line is slow */
    if ((u8)(now - last_time) != loop_frames) {
        loop_frames = now - last_time;
        put_2digits(status_row + 17, loop_frames);
        status_row[19] = 'f';
        set_bkg_tiles(17, 0, 3, 1, status_row + 17);
    }
    last_time = now;
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
