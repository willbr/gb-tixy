#include <stdio.h>
#include <math.h>

/*
 * Angles are in units of pi/127, so 127 == pi.
 * Every table has 256 entries in u8 order: index k holds f((signed char)k),
 * i.e. 0..127 first, then -128..-1. Look up with table[(u8)i].
 */

typedef double (*fn_t)(double);

unsigned char dither_42[8][8] = {
    {  0,  20,   5,  26,   1,  22,   6,  27},
    { 31,  10,  36,  15,  32,  11,  37,  16},
    {  7,  28,   2,  23,   9,  29,   3,  24},
    { 39,  18,  33,  13,  40,  19,  35,  14},
    {  1,  22,   7,  27,   0,  21,   5,  26},
    { 33,  12,  38,  17,  31,  11,  37,  16},
    {  9,  30,   4,  25,   8,  29,   3,  24},
    { 41,  20,  35,  14,  39,  18,  34,  13}
};


#define number_of_colours 4
#define number_of_sectors (number_of_colours - 1)
#define max_val 127
#define sector_size (max_val / number_of_sectors)


/* returns a GB colour index, 0 = lightest */
int
closest_colour(int x, int y, int c)
{
    int limit = dither_42[y % 8][x % 8];
    int s = 0;

    if (c < 0)
        c = -c;

    s = c / sector_size;
    if (c % sector_size > limit)
        s += 1;

    return s > 3 ? 3 : s;
}


void
calc_dither(void)
{
    int i = 0;
    int x = 0;
    int y = 0;
    int c = 0;
    unsigned char low = 0;
    unsigned char high = 0;

    printf("const unsigned char dither_gfx[] = {\n");
    for (i = 0; i != 128; i += 1) {
        printf("    ");
        for (y = 0; y != 8; y += 1) {
            low = 0;
            high = 0;
            for (x = 0; x != 8; x += 1) {
                c = closest_colour(x, y, i);
                low  |= (c & 1)        << (7 - x);
                high |= ((c >> 1) & 1) << (7 - x);
            }

            if (y == 4)
                printf("\n    ");

            /* GB tile row: low bitplane first, then high */
            printf("0x%02x,0x%02x,", low, high);
        }
        printf(" /* %d */\n\n", i);
    }
    printf("};\n");
}


int
to_i8(double d)
{
    d = floor(d + 0.5);
    if (d > 127)  return 127;
    if (d < -128) return -128;
    return (int)d;
}


double angle_sin(double i) { return sin(i * (M_PI/127.0)) * 127; }
double angle_cos(double i) { return cos(i * (M_PI/127.0)) * 127; }
double angle_tan(double i) { return tan(i * (M_PI/127.0)) * 127; }
/* input scaled like an angle, output is an angle: range about +-51 */
double angle_atan(double i) { return atan(i * (M_PI/127.0)) * (127.0/M_PI); }
double int_sqrt(double i) { return i < 0 ? 0 : floor(sqrt(i)); }


void
calc_table(const char *name, fn_t f)
{
    int k;
    int i;
    double d;

    printf("const signed char %s[] = {\n", name);
    for (k = 0; k != 256; k += 1) {
        i = (signed char)k;
        d = f(i);
        printf("    /* %+4d %+9.2f */ %+4d", i, d, to_i8(d));
        printf("%s\n", k != 255 ? "," : "");
    }
    printf("};\n");
}


int
main(void)
{
    calc_dither();
    calc_table("sin_table",  angle_sin);
    calc_table("cos_table",  angle_cos);
    calc_table("tan_table",  angle_tan);
    calc_table("atan_table", angle_atan);
    calc_table("sqrt_table", int_sqrt);
    return 0;
}
