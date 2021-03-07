#include "shared.h"

int
main(int argc, char **argv)
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
                pset(&t, x, y, c);
            }

            if (y == 4)
                printf("\n    ");

            printf("0x%02x,", t.row[y].low);
            printf("0x%02x,", t.row[y].high);
        }
        printf(" /* %d */\n\n", i);
    }
    printf("};\n");
    return 0;
}


