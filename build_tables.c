#include "shared.h"
#include <stdio.h>
#include <math.h>

void calc_dither(void);
void calc_sin_table(void);

int
main(int argc, char **argv)
{
    calc_dither();
    calc_sin_table();
    return 0;
}


void
calc_dither(void)
{
    int i = 0;
    int x = 0;
    int y = 0;
    char c = 0;
    struct gb_tile t = {0};

    printf("const unsigned char dither_gfx[] = {\n");
    for (i = 0; i != 128; i += 1) {
        printf("    ");
        for (y = 0; y != 8; y += 1) {
            for (x = 0; x != 8; x += 1) {
                c = closest_colour(x, y, i);
                tile_pset(&t, x, y, c);
            }

            if (y == 4)
                printf("\n    ");

            printf("0x%02x,", t.row[y].low);
            printf("0x%02x,", t.row[y].high);
        }
        printf(" /* %d */\n\n", i);
    }
    printf("};\n");
}


void
calc_sin_table(void)
{
    signed int i;
    double d;
    /*puts("sin");*/
    /*printf("%f\n", sin(127.0 * (M_PI/127.0)));*/

    printf("const signed char sin_table[] = {\n");
    for (i = -127; i <= 127; i += 1) {
        /*printf("%f\n", (float)i);*/
        d = sin((double)i * (M_PI/127.0)) * 127;
        /*printf("%3.1f, %3d\n", d, (signed char) d);*/
        printf("    /* %+3d */ %+3d", i, (signed char) d);
        printf("%s\n", i != 127 ? "," : "");
    }

    printf("};");
}


