#include <stdio.h>
#include <math.h>

/*
 * Angles are in units of pi/128, so 128 == pi and a full turn is 256: angles
 * wrap exactly with i8/u8 arithmetic, and cos(a) == sin(a + 64).
 * Outputs are scaled by 127 to fit an i8.
 * Every table has 256 entries in u8 order: index k holds f((signed char)k),
 * i.e. 0..127 first, then -128..-1. Look up with table[(u8)i].
 */

typedef double (*fn_t)(double);

/*
 * Tile k draws the value (signed char)k as a tixy.land circle around pixel
 * (3,3), leaving a 1px gap between cells. Radius grows linearly with |v|,
 * snapped down to a squared radius that gives a round pixel shape
 * (dot, plus, 3x3, 5, 7, 7+). Colour 0 = background, 1 = positive, 2 = negative.
 */
int round_r2[] = {0, 1, 2, 5, 10, 13};

void
calc_circles(void)
{
    int k = 0;
    int v = 0;
    int x = 0;
    int y = 0;
    int c = 0;
    int n = 0;
    int r2 = 0;
    double r = 0;
    unsigned char low = 0;
    unsigned char high = 0;

    printf("const unsigned char circle_gfx[] = {\n");
    for (k = 0; k != 256; k += 1) {
        v = (signed char)k;
        r = (v < 0 ? -v : v) * (3.7 / 128.0);
        r2 = -1;
        for (n = 0; n != 6 && v != 0 && round_r2[n] <= r*r; n += 1)
            r2 = round_r2[n];
        printf("    ");
        for (y = 0; y != 8; y += 1) {
            low = 0;
            high = 0;
            for (x = 0; x != 8; x += 1) {
                c = (x-3)*(x-3) + (y-3)*(y-3) <= r2 ? (v > 0 ? 1 : 2) : 0;
                low  |= (c & 1)        << (7 - x);
                high |= ((c >> 1) & 1) << (7 - x);
            }

            if (y == 4)
                printf("\n    ");

            /* GB tile row: low bitplane first, then high */
            printf("0x%02x,0x%02x,", low, high);
        }
        printf(" /* %d */\n\n", v);
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


double angle_sin(double i) { return sin(i * (M_PI/128.0)) * 127; }
double angle_tan(double i) { return tan(i * (M_PI/128.0)) * 127; }
/* input scaled like an angle, output is an angle: range about +-51 */
double angle_atan(double i) { return atan(i * (M_PI/128.0)) * (128.0/M_PI); }
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
    calc_circles();
    calc_table("sin_table",  angle_sin);
    calc_table("tan_table",  angle_tan);
    calc_table("atan_table", angle_atan);
    calc_table("sqrt_table", int_sqrt);
    return 0;
}
