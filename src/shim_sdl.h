#ifndef _SHIM_SDL_H
#define _SHIM_SDL_H

#include <SDL.h>

#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG

#define SCRN_VX   256
#define SCRN_VY   256
#define SCRN_VX_B 32
#define SCRN_VY_B 32

#define SCRN_FPS 60
#define SCRN_TICKS_PER_FRAME (1000 / SCRN_FPS)

#define SCALE_FACTOR 5
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define BITS_PER_BYTE 8
#define BYTES_PER_TILE 16

#define TILES_VX_B 16
#define TILES_VY_B 24
#define TILES_VX   TILES_VX_B * 8
#define TILES_VY   TILES_VY_B * 8


typedef unsigned char UINT8;
typedef   signed char  INT8;

typedef unsigned short UINT16;
typedef   signed short  INT16;

typedef unsigned int UINT32;
typedef   signed int  INT32;

typedef  UINT8 u8;
typedef   INT8 i8;
typedef UINT16 u16;
typedef  INT16 i16;
typedef UINT32 u32;
typedef  INT32 i32;

UINT8 shim_memory[0x10000];

UINT32 shim_colour_0 = 0;
UINT32 shim_colour_1 = 0;
UINT32 shim_colour_2 = 0;
UINT32 shim_colour_3 = 0;

u32 shim_prev_time = 0;

SDL_Window  *shim_window = NULL;
SDL_Surface *shim_screen_surface = NULL;

SDL_Surface *shim_gb_tiles = NULL;
SDL_Surface *shim_gb_bkg = NULL;

void set_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data);
void get_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data);

void shim_render_gb_tiles(void);
void shim_render_gb_background(void);
void shim_render_gb_sprites(void);

void
set_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data)
{
    UINT8 *t = &shim_memory[0x8800];
    //printf("%d, %d\n", first_tile, nb_tiles);

   while (first_tile--)
        t += BYTES_PER_TILE;

    while  (nb_tiles--) {
        UINT8 i = BYTES_PER_TILE;
        while (i-- != 0) {
            *t++ = *data++;
        }
    }
}


void
get_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data) 	
{
    UINT8 *t = &shim_memory[0x8800];

    while (first_tile--)
        t += BYTES_PER_TILE;

    while  (nb_tiles--) {
        UINT8 i = BYTES_PER_TILE;
        while (i--)
            *data++ = *t++;
    }
}


void
set_bkg_tiles(UINT8 x, UINT8 y, UINT8 w,UINT8 h, unsigned char *tiles)
{
    UINT8 i = 0;
    UINT8 j = 0;
    UINT8 *base = &shim_memory[0x9800];
    UINT8 *t    = NULL;

    for (i = 0; i < h; i += 1) {
        t = base + ((y + i) * SCRN_VY_B) + x;
        for (j = 0; j < w; j += 1) {
            t += 1;
            *t = *tiles;
            tiles += 1;
        }
    }
}


void
enable_interrupts(void) 	
{
    ;
}


void
wait_vbl_done(void)
{
    u32 time = SDL_GetTicks();
    u32 time_delta = time - shim_prev_time;

    if (time_delta < SCRN_TICKS_PER_FRAME) {
        SDL_Delay(SCRN_TICKS_PER_FRAME - time_delta);
    }

    shim_prev_time = time;
}


void
shim_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL failed to start: %s\n",
               SDL_GetError());
        exit(1);
    }

    shim_window = SDL_CreateWindow("SDL tixy",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SCREEN_WIDTH * SCALE_FACTOR, 
                                   SCREEN_HEIGHT * SCALE_FACTOR,
                                   SDL_WINDOW_SHOWN);
    if (shim_window == NULL) {
        printf("SDL failed to create a window: %s\n",
               SDL_GetError());
        exit(1);
    }

    shim_screen_surface = SDL_GetWindowSurface(shim_window);

    shim_gb_tiles = SDL_CreateRGBSurface(0,
                                       TILES_VX,
                                       TILES_VY,
                                       32,
                                       0, 0, 0, 0);

    if (shim_gb_tiles == NULL) {
        printf("failed to create tile map surface: %s\n",
               SDL_GetError());
        exit(1);
    }

    shim_gb_bkg = SDL_CreateRGBSurface(0,
                                       SCRN_VX,
                                       SCRN_VY,
                                       32,
                                       0, 0, 0, 0);

    if (shim_gb_bkg == NULL) {
        printf("failed to create background surface: %s\n",
               SDL_GetError());
        exit(1);
    }

    
    shim_colour_0 = SDL_MapRGB(shim_gb_tiles->format, 10,  10, 10);
    shim_colour_1 = SDL_MapRGB(shim_gb_tiles->format, 10,  85, 10);
    shim_colour_2 = SDL_MapRGB(shim_gb_tiles->format, 10, 170, 10);
    shim_colour_3 = SDL_MapRGB(shim_gb_tiles->format, 10, 255, 10);
}


void
shim_update(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                exit(0);
                break;

            default:
                break;
            }
            break;

        default:
            break;
        }
    }
}


void
shim_render(void)
{
    static i = 0;

    SDL_FillRect(shim_screen_surface,
                 NULL,
                 SDL_MapRGB(shim_screen_surface->format,
                            0x11,
                            0x11,
                            0x11));

    shim_render_gb_tiles();
    shim_render_gb_background();
    shim_render_gb_sprites();
    
    SDL_UpdateWindowSurface(shim_window);

    if (i++ > 60 * 10)
        exit(0);
}


void
shim_render_gb_tiles(void)
{
    int bpp = shim_gb_tiles->format->BytesPerPixel;
    UINT32 *pixels = shim_gb_tiles->pixels;
    UINT32 *pixel = pixels;
    int x = 0;
    int y = 0;
    int px = 0;
    int py = 0;
    int tmx = 0;
    int tmy = 0;
    u8  c = 0;
    struct gb_tile *base = (struct gb_tile *)&shim_memory[0x8800];
    struct gb_tile *t = NULL;

    for (py = 0; py < TILES_VY; py += 1) {
        pixel = pixels + (py * shim_gb_tiles->w);
        for (px = 0; px < TILES_VX; px += 1) {
            *pixel = SDL_MapRGB(shim_gb_tiles->format,
                                rand() * 255,
                                rand() * 255,
                                rand() * 255);
            pixel += 1;
        }
    }

    t = base;
    for (tmy = 0; tmy < TILES_VY_B; tmy += 1) {
        for (tmx = 0; tmx < TILES_VX_B; tmx += 1) {
            py = tmy * 8;
            px = tmx * 8;
            for (y = 0; y < 8; y += 1) {
                pixel = pixels + ((py + y) * shim_gb_tiles->w) + px;
                for (x = 0; x < 8; x += 1) {
                    c = tile_pget(t, x, y);
                    switch (c) {
                    case 0:
                        *pixel = shim_colour_0;
                        break;

                    case 1:
                        *pixel = shim_colour_1;
                        break;

                    case 2:
                        *pixel = shim_colour_2;
                        break;

                    case 3:
                        *pixel = shim_colour_3;
                        break;

                    default:
                        break;
                    }
                    pixel += 1;
                }
            }
            t += 1;
        }
    }
}


void
shim_render_gb_background(void)
{
    SDL_Rect src = {0};
    SDL_Rect dst = {0};

    int bpp = shim_gb_bkg->format->BytesPerPixel;
    UINT8 *pixels = shim_gb_bkg->pixels;
    UINT8 *pixel = pixels;

    UINT8 bg_x = 0;
    UINT8 bg_y = 0;
    unsigned int p = 0;
    char *tile_id = &shim_memory[0x9800];

    if (SDL_MUSTLOCK(shim_gb_bkg))
        SDL_LockSurface(shim_gb_bkg);

    for (bg_y = 0; bg_y != 32; bg_y += 1) {
        for (bg_x = 0; bg_x != 32; bg_x += 1) {
            UINT8 t_x = ((*tile_id) % TILES_VX_B) * 8;
            UINT8 t_y = ((*tile_id) / TILES_VX_B) * 8;

            src.x = t_x;
            src.y = t_y;
            src.w = 8;
            src.h = 8;

            dst.x = 8 * bg_x;
            dst.y = 8 * bg_y;
            dst.w = 8;
            dst.h = 8;

            if (SDL_BlitSurface(shim_gb_tiles, &src, shim_gb_bkg, &dst)) {
                printf("failed to Blit tile to bkg", SDL_GetError());
            }

            tile_id += 1;
        }
    }


    if (SDL_MUSTLOCK(shim_gb_bkg))
        SDL_UnlockSurface(shim_gb_bkg);

    src.x = 8;
    src.y = 0;
    src.w = SCREEN_WIDTH;
    src.h = SCREEN_HEIGHT;

    if (SDL_BlitScaled(shim_gb_bkg, &src, shim_screen_surface, NULL)) {
        printf("failed to Blit gb bkg", SDL_GetError());
    }
}


void
shim_render_gb_sprites(void)
{
    //puts("render sprites");
}


#endif

