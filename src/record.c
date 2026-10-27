/*
 * Headless recorder for the README gif: runs main.c through the SDL shim and
 * writes raw ARGB 160x144 frames to stdout.
 *
 * usage: record FRAMES IDX [IDX...]   (gallery indices, FRAMES frames each)
 */
#define main tixy_main
#include "main.c"
#undef main

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    int frames = 0;
    int a = 0;
    int f = 0;
    int y = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: record FRAMES IDX [IDX...]\n");
        return 1;
    }

    frames = atoi(argv[1]);
    shim_init();
    init();

    for (a = 2; a < argc; a += 1) {
        select_formula(atoi(argv[a]) % NUM_FORMULAS);
        for (f = 0; f != frames; f += 1) {
            if (prog_valid)
                prog_eval(&progs[formula_idx], (i8)f, screen);
            draw_screen();
            shim_render();
            for (y = 0; y != SCREEN_HEIGHT; y += 1)
                fwrite((UINT8 *)shim_lcd->pixels + y * shim_lcd->pitch, 4, SCREEN_WIDTH, stdout);
        }
    }

    return 0;
}
