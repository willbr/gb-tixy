#include <gb/gb.h>
#include <gb/cgb.h>

#define MAP_WIDTH 32;
#define MAP_HEIGHT 32;

unsigned char t = 0;
unsigned char i = 0;
unsigned char x = 0;
unsigned char y = 0;

unsigned char *screen = NULL;
unsigned char *iscreen[256];
unsigned char tile[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned char j = 0;

enum direction {
    right,
    down,
    left,
    up,
};

unsigned char
pget(char *t, char x, char y)
{
    unsigned char mask;
    unsigned char *row;
    mask = 0b10000000 >> x;
    row = t + (y * 2);
    return (*row & mask ? 1 : 0) + (*(row+1) & mask ? 2 : 0);
}

void
pset(char *t, char x, char y, char p)
{
    unsigned char mask;
    unsigned char *row;
    mask = 0b10000000 >> x;
    row = t + (y * 2);
    *row |= (mask & p);
    row += 1;
    *row |= (mask & p);
}

void
init(void)
{
    signed char k = 0;
    char x = 3;
    char y = 3;
    unsigned char mask = 0;
    unsigned char *row = NULL;
    unsigned char p = 0;
    enum direction d = right;
    unsigned char i = 0;
    unsigned int j = 0;


    k = -127;
    do {
        row = &tile[y * 2];

        mask = 0b10000000 >> x;

        p  = *row & mask ? 1 : 0;
        p += *(row+1) & mask ? 2 : 0;

        switch (p) {
            case 0:
                *row |= mask;
                break;
            case 1:
                /*turn off*/
                *row &= ~mask;
                *(row+1) |= mask;
                /*turn on*/
                break;
            case 2:
                *row |= mask;
            case 3:
                /*move cursor*/
                switch (i) {
                    case 0:
                        x += 1;
                        break;
                    case 1:
                        y += 1;
                        break;
                    case 2:
                    case 3:
                        x -= 1;
                        break;
                    case 4:
                    case 5:
                        y -= 1;
                        break;
                    case 6:
                    case 7:
                    case 8:
                        x += 1;
                        break;
                    case 9:
                    case 10:
                    case 11:
                        y += 1;
                        break;
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                        x -= 1;
                        break;
                    case 16:
                    case 17:
                    case 18:
                    case 19:
                        y -= 1;
                        break;
                    case 20:
                    case 21:
                    case 22:
                    case 23:
                    case 24:
                        x += 1;
                        break;
                    case 25:
                    case 26:
                    case 27:
                    case 28:
                    case 29:
                        y += 1;
                        break;
                    case 30:
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                        x -= 1;
                        break;
                    case 36:
                    case 37:
                    case 38:
                    case 39:
                    case 40:
                    case 41:
                        y -= 1;
                        break;
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                    case 48:
                        x += 1;
                        break;
                    case 49:
                    case 50:
                    case 51:
                    case 52:
                    case 53:
                    case 54:
                    case 55:
                        y += 1;
                        break;
                    case 56:
                    case 57:
                    case 58:
                    case 59:
                    case 60:
                    case 61:
                    case 62:
                        x -= 1;
                        break;
                    default:
                        /*y += 1;*/
                        break;
                }
                i += 1;
                break;

        }


        set_bkg_data(k, 1, (void*)&tile);
        k += 1;
    } while (k != -128);

    screen = (unsigned char *)0x9800;
    for (j = 1024; j; j -= 1)  {
        *screen = 0x4f;
        screen += 1;
    }

}


void
main(void)
{
    unsigned char **p = NULL;
    unsigned char **p_start = NULL;
    unsigned char **p_end = NULL;

    DISPLAY_OFF;
    init();

    SHOW_BKG;
    enable_interrupts();
    DISPLAY_ON;

    screen = (unsigned char *)0x9800;
    screen += MAP_WIDTH;
    for (y = 0; y != 16; y += 1) {
        screen += 2;
        for (x = 0; x != 16; x += 1) {
            iscreen[j++] = screen++;
        }
        screen += 14;
    }

    p_start = &iscreen[0];
    p_end = &iscreen[255] + 1;

    p = p_start;
    i = 0;

    for (;;) {

#define tiles_per_frame 41

        j = tiles_per_frame;

        wait_vbl_done();
        while (j--) {
            if (p >= p_end)
                p = p_start;

            if (x == 16) {
                x = 0;
                y += 1;
            }

            y &= 0x0f;
            /*if (y == 16)*/
                /*y = 0;*/

            **(p++) = i + t + x >> 2;

            x += 1;
        }
        t += 1;
    }
}

