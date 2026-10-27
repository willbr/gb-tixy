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

u8 prev_pad = 0;
u8 hold = 0;

u8 status_row[SCREEN_COLS];
u8 prog_row[SCREEN_COLS];
u8 text_dirty = 1;
u8 loop_frames = 0;
u16 last_time = 0;

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
    u8 full[PROG_MAX * 5];
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

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;
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
    u8 pad = joypad();
    u8 pressed = pad & ~prev_pad;
    u8 dirs = pad & (J_UP | J_DOWN | J_LEFT | J_RIGHT);
    prog_t *p = &progs[formula_idx];
    u8 *tok = &p->tok[cursor];
    u8 n = 0;

    prev_pad = pad;

    if (dirs) {
        if (hold != 255)
            hold++;
        if (hold > 12 && (hold & 1))
            pressed |= dirs;
    } else {
        hold = 0;
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
/*
 * screen (a page) -> tilemap rows 1..16 at column 2. VRAM is writable in
 * modes 0 and 1; after seeing one there are at least ~20 cycles (mode 2)
 * before mode 3, enough for 2 bytes, so STAT is checked every 2 bytes.
 * Row starts are 0x9822 + 32 * row, and no row crosses a 256-byte boundary.
 */
void
copy_grid(void) __naked
{
    __asm__(
        "ld a,(_screen + 1)\n ld h,a\n ld l,#0\n"
        "ld de,#0x9822\n ld c,#16\n"
        "1$:\n"
        " ld b,#8\n"
        "2$:\n"
        " ldh a,(_STAT_REG + 0)\n and a,#2\n jr nz,2$\n"
        " ld a,(hl+)\n ld (de),a\n inc e\n"
        " ld a,(hl+)\n ld (de),a\n inc e\n"
        " dec b\n jr nz,2$\n"
        " ld a,e\n add a,#16\n ld e,a\n jr nc,3$\n inc d\n"
        "3$:\n"
        " dec c\n jr nz,1$\n"
        " ret\n");
}
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
    u16 now = 0;

    wait_vbl_done();
    draw_screen();

    now = sys_time;
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
