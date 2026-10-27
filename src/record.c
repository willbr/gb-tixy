/*
 * Headless recorder for the README gif: runs main.c through the SDL shim and
 * writes raw ARGB 160x144 frames to stdout.
 *
 * usage: record FRAMES IDX [IDX...]   (formula indices, FRAMES frames each)
 */
#define main tixy_main
#include "main.c"
#undef main

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
        formula_idx = atoi(argv[a]) % NUM_FORMULAS;
        t = 0;
        for (f = 0; f != frames; f += 1) {
            formulas[formula_idx]();
            t++;
            set_bkg_tiles(2, 1, 16, 16, screen);
            shim_render();
            for (y = 0; y != SCREEN_HEIGHT; y += 1)
                fwrite((UINT8 *)shim_lcd->pixels + y * shim_lcd->pitch, 4, SCREEN_WIDTH, stdout);
        }
    }

    return 0;
}
