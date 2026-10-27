#ifndef _SHIM_SDL_H
#define _SHIM_SDL_H

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG

#define SCRN_FPS 60
#define SCRN_TICKS_PER_FRAME (1000 / SCRN_FPS)

#define SCALE_FACTOR 5
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define BYTES_PER_TILE 16

/* same bits as GBDK */
#define J_RIGHT  0x01
#define J_LEFT   0x02
#define J_UP     0x04
#define J_DOWN   0x08
#define J_A      0x10
#define J_B      0x20
#define J_SELECT 0x40
#define J_START  0x80


typedef unsigned char UINT8;
typedef unsigned int UINT32;

UINT8 shim_memory[0x10000];
UINT8 shim_pad = 0;

/* GB colour index -> RGB, 0 = lightest like the DMG default palette */
UINT32 shim_palette[4];

UINT32 shim_prev_time = 0;

SDL_Window  *shim_window = NULL;
SDL_Surface *shim_screen_surface = NULL;
SDL_Surface *shim_lcd = NULL;


void
set_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data)
{
    memcpy(&shim_memory[0x8800 + first_tile * BYTES_PER_TILE],
           data,
           nb_tiles * BYTES_PER_TILE);
}


void
set_bkg_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, unsigned char *tiles)
{
    UINT8 i = 0;

    for (i = 0; i < h; i += 1) {
        memcpy(&shim_memory[0x9800 + (y + i) * 32 + x], tiles, w);
        tiles += w;
    }
}


void
enable_interrupts(void)
{
}


UINT8
joypad(void)
{
    return shim_pad;
}


void
wait_vbl_done(void)
{
    UINT32 time = SDL_GetTicks();
    UINT32 time_delta = time - shim_prev_time;

    if (time_delta < SCRN_TICKS_PER_FRAME) {
        SDL_Delay(SCRN_TICKS_PER_FRAME - time_delta);
    }

    shim_prev_time = SDL_GetTicks();
}


void
shim_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL failed to start: %s\n", SDL_GetError());
        exit(1);
    }

    shim_window = SDL_CreateWindow("SDL tixy",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SCREEN_WIDTH * SCALE_FACTOR,
                                   SCREEN_HEIGHT * SCALE_FACTOR,
                                   SDL_WINDOW_SHOWN);
    if (shim_window == NULL) {
        printf("SDL failed to create a window: %s\n", SDL_GetError());
        exit(1);
    }

    shim_screen_surface = SDL_GetWindowSurface(shim_window);

    shim_lcd = SDL_CreateRGBSurfaceWithFormat(0,
                                              SCREEN_WIDTH,
                                              SCREEN_HEIGHT,
                                              32,
                                              SDL_PIXELFORMAT_ARGB8888);
    if (shim_lcd == NULL) {
        printf("failed to create lcd surface: %s\n", SDL_GetError());
        exit(1);
    }

    shim_palette[0] = SDL_MapRGB(shim_lcd->format, 10, 255, 10);
    shim_palette[1] = SDL_MapRGB(shim_lcd->format, 10, 170, 10);
    shim_palette[2] = SDL_MapRGB(shim_lcd->format, 10,  85, 10);
    shim_palette[3] = SDL_MapRGB(shim_lcd->format, 10,  10, 10);
}


UINT8
shim_key_bit(SDL_Keycode key)
{
    switch (key) {
    case SDLK_RIGHT:     return J_RIGHT;
    case SDLK_LEFT:      return J_LEFT;
    case SDLK_UP:        return J_UP;
    case SDLK_DOWN:      return J_DOWN;
    case SDLK_SPACE:
    case SDLK_z:         return J_A;
    case SDLK_x:         return J_B;
    case SDLK_BACKSPACE: return J_SELECT;
    case SDLK_RETURN:    return J_START;
    default:             return 0;
    }
}


void
shim_update(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                exit(0);
            shim_pad |= shim_key_bit(event.key.keysym.sym);
            break;

        case SDL_KEYUP:
            shim_pad &= ~shim_key_bit(event.key.keysym.sym);
            break;

        default:
            break;
        }
    }
}


/* draw the visible 160x144 of the tilemap at scroll (0,0) */
void
shim_render(void)
{
    UINT8 *map   = &shim_memory[0x9800];
    UINT8 *tiles = &shim_memory[0x8800];
    UINT32 *row  = NULL;
    UINT8 *r     = NULL;
    UINT8 bit    = 0;
    int x = 0;
    int y = 0;

    for (y = 0; y != SCREEN_HEIGHT; y += 1) {
        row = (UINT32 *)((UINT8 *)shim_lcd->pixels + y * shim_lcd->pitch);
        for (x = 0; x != SCREEN_WIDTH; x += 1) {
            r = &tiles[map[(y >> 3) * 32 + (x >> 3)] * BYTES_PER_TILE + (y & 7) * 2];
            bit = 7 - (x & 7);
            row[x] = shim_palette[((r[0] >> bit) & 1) | (((r[1] >> bit) & 1) << 1)];
        }
    }

    if (SDL_BlitScaled(shim_lcd, NULL, shim_screen_surface, NULL))
        printf("failed to blit lcd: %s\n", SDL_GetError());

    SDL_UpdateWindowSurface(shim_window);
}


#endif
