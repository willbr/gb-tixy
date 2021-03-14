#ifndef _SHIiM_SDL_H
#define _SHIM_SDL_H

#include <SDL.h>

#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG

#define SCRN_VX   256
#define SCRN_VY   256
#define SCRN_VX_B 32
#define SCRN_VY_B 32

#define SCALE_FACTOR 4
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define BYTES_PER_TILE 16

typedef unsigned char UINT8;
typedef   signed char  INT8;

typedef unsigned short UINT16;
typedef   signed short  INT16;

UINT8 shim_memory[0x10000];

SDL_Window  *shim_window = NULL;
SDL_Surface *shim_screen_surface = NULL;

SDL_Surface *shim_gb_bkg = NULL;
//SDL_Surface *shim_gb_win = NULL;

void set_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data);
void get_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data);


void
set_bkg_data(UINT8 first_tile, UINT8 nb_tiles, unsigned char *data)
{
    //puts(__func__);
    UINT8 *t = &shim_memory[0x8800];
    //printf("%d, %d\n", first_tile, nb_tiles);

   while (first_tile--)
        t += BYTES_PER_TILE;

    while  (nb_tiles--) {
        UINT8 i = BYTES_PER_TILE;
        while (i-- != 0) {
            //printf("%x", i);
            *t++ = *data++;
        }
        //puts("");
    }
    //puts("done");
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
    //puts(__func__);
}

void
enable_interrupts(void) 	
{
    //puts(__func__);
}


void
wait_vbl_done(void)
{
    //puts(__func__);
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


    shim_gb_bkg = SDL_CreateRGBSurface(0,
                                       SCRN_VX,
                                       SCRN_VY,
                                       32,
                                       0, 0, 0, 0);

    if (shim_gb_bkg == NULL) {
        printf("failed to create background surface: %s\n", SDL_GetError());
        exit(1);
    }


        //SDL_Delay(2000);
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

            default:
                break;
        }
    }
}


void
shim_render(void)
{
    SDL_FillRect(shim_screen_surface,
                 NULL,
                 SDL_MapRGB(shim_screen_surface->format,
                            0x00,
                            0x00,
                            0x11));

    SDL_LockSurface(shim_gb_bkg);

    int bpp = shim_gb_bkg->format->BytesPerPixel;
    UINT8 *pixels = shim_gb_bkg->pixels;
    UINT8 *pixel = pixels;

    UINT8 bg_x = 0;
    UINT8 bg_y = 0;
    unsigned int p = 0;
    char *tile_id = &shim_memory[0x9800];
    struct gb_tile tile;

    for (bg_y = 0; bg_y != 32; bg_y += 1) {
        for (bg_x = 0; bg_x != 32; bg_x += 1) {
            UINT8 t_x = 0;
            UINT8 t_y = 0;
            for (t_y = 0; t_y != 8; t_y += 1) {
                for (t_x = 0; t_x != 8; t_x += 1) {
                    *pixel = rand() % 255;
                    pixel += 1;
                }
            }

            tile_id += 1;
        }
    }


    SDL_UnlockSurface(shim_gb_bkg);

    //SDL_FillRect(shim_gb_bkg,
                 //NULL,
                 //SDL_MapRGB(shim_gb_bkg->format, 0x21, 0xFF, 0x61));

    if (SDL_BlitScaled(shim_gb_bkg, NULL, shim_screen_surface, NULL)) {
        printf("failed to Blit gb bkg", SDL_GetError());
    }

    
    SDL_UpdateWindowSurface(shim_window);

    SDL_Delay(1000);
    exit(0);
}


#endif
