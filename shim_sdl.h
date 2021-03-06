#ifndef _SHIiM_SDL_H
#define _SHIM_SDL_H

#include <SDL.h>

#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define BYTES_PER_TILE 16

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;

UINT8 shim_memory[0x10000];

SDL_Window  *shim_window = NULL;
SDL_Surface *shim_screen_surface = NULL;


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
                            0x00,
                            0x00,
                            0xff));

    int bpp = shim_screen_surface->format->BytesPerPixel;
    UINT8 *pixels = shim_screen_surface->pixels;
    UINT8 *pixel = NULL;

    UINT8 bg_x = 20;
    UINT8 bg_y = 20;
    unsigned int p = 0;
    char *tile_id = &shim_memory[0x9800];
    struct gb_tile tile;

    for (bg_y = 0; bg_y != 32; bg_y += 1) {
        for (bg_x = 0; bg_x != 32; bg_x += 1) {
            UINT8 t_x = 0;
            UINT8 t_y = 0;
            get_bkg_data(*tile_id, 1, (char *)&tile);
            printf("%x", *tile_id);
            puts("");
            printf("%x\n", tile.row[0].low);
            printf("%x\n", tile.row[0].high);
            for (t_y = 0; t_y != 8; t_y += 1) {
                for (t_x = 0; t_x != 8; t_x += 1) {
                    UINT8 c = pget(&tile, t_x, t_y);
                    printf("%d", c);
                    ;
                }
                puts("");
            }
            //exit(0);

            tile_id += 1;
        }
        puts("");
    }
    puts("");
    exit(0);

    //for (y = 20; y != 41; y += 1) {
        //for (x = 20; x != 41; x += 1) {

            //pixel = &pixels[(y * shim_screen_surface->pitch) + x *bpp];
            ////printf("%p\n", pixel);
            //*pixel++ = 0x00;
            //*pixel++ = 0x00;
            //*pixel++ = 0x00;
            ///[>pixel++ = 0xff;
            ///[>pixel++ = 0xff;
        //}
    //}
    //exit(1);
    //*pixel++ = 0xff;
    //*pixel++ = 0xff;

    
    SDL_UpdateWindowSurface(shim_window);
}


#endif
