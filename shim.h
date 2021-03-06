#ifndef _SHIM_H
#define _SHIM_H

#include <SDL.h>

#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

typedef unsigned char UINT8;
typedef unsigned int UINT;

UINT8 shim_memory[0x10000];

SDL_Window  *shim_window = NULL;
SDL_Surface *shim_screen_surface = NULL;


void
set_bkg_data(UINT first_tile, UINT8 nb_tiles, unsigned char *data) 	
{
    ;
}



void
enable_interrupts(void) 	
{
    ;
}


void
wait_vbl_done(void)
{
    ;
}


void
shim_init(void)
{

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL failed to start: %s\n", SDL_GetError());
        exit(1);
    }

    shim_window = SDL_CreateWindow("SDL Tutorial",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SCREEN_WIDTH,
                                   SCREEN_HEIGHT,
                                   SDL_WINDOW_SHOWN);
    if (shim_window == NULL) {
        printf("SDL failed to create a window: %s\n", SDL_GetError());
        exit(1);
    }

    shim_screen_surface = SDL_GetWindowSurface(shim_window);


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
        };
    }
}


void
shim_render(void)
{
    SDL_FillRect(shim_screen_surface,
                 NULL,
                 SDL_MapRGB(shim_screen_surface->format,
                            0xff,
                            0xff,
                            0xff));

    SDL_UpdateWindowSurface(shim_window);
}


#endif
