#include <stdio.h>
#include <math.h>
#include <string.h>

/*
 * Angles are in units of pi/128, so 128 == pi and a full turn is 256: angles
 * wrap exactly with i8/u8 arithmetic.
 * Outputs are scaled by 127 to fit an i8.
 * Every table has 256 entries in u8 order: index k holds f((signed char)k),
 * i.e. 0..127 first, then -128..-1. Look up with table[(u8)i].
 */

typedef double (*fn_t)(double);

/*
 * Tables the interpreter indexes as page:n live on 256-byte boundaries in
 * ROM, so the GB asm loops can address any of them with one index register.
 */
#define PAGE_CELL_X   0x7500
#define PAGE_CELL_Y   0x7600
#define PAGE_CELL_I   0x7700
#define PAGE_TO_TILE  0x7800
#define PAGE_SIN      0x7900
#define PAGE_COS      0x7a00
#define PAGE_TAN      0x7b00
#define PAGE_ATAN     0x7c00
#define PAGE_SQRT     0x7d00
#define PAGE_NEG      0x7e00
#define PAGE_ABS      0x7f00


/* print one 8x8 tile of colour indices as GB 2bpp: low bitplane, then high */
void
emit_tile(int px[8][8], const char *comment)
{
    int x = 0;
    int y = 0;
    unsigned char low = 0;
    unsigned char high = 0;

    printf("    ");
    for (y = 0; y != 8; y += 1) {
        low = 0;
        high = 0;
        for (x = 0; x != 8; x += 1) {
            low  |= (px[y][x] & 1)        << (7 - x);
            high |= ((px[y][x] >> 1) & 1) << (7 - x);
        }
        if (y == 4)
            printf("\n    ");
        printf("0x%02x,0x%02x,", low, high);
    }
    printf(" /* %s */\n", comment);
}


/*
 * tixy.land circles around pixel (3,3), leaving a 1px gap between cells.
 * Squared radii that give round pixel shapes: dot, plus, 3x3, 5, 7, 7+.
 * Tile 0 is empty, 1..6 are positive sizes (colour 1), 7..12 negative (colour 2).
 */
#define NUM_SIZES 6
int round_r2[NUM_SIZES] = {0, 1, 2, 5, 10, 13};

void
calc_circles(void)
{
    int px[8][8];
    int k = 0;
    int v = 0;
    int s = 0;
    int x = 0;
    int y = 0;
    int c = 0;
    double r = 0;
    char comment[32];

    printf("const unsigned char circle_gfx[] = {\n");
    for (k = 0; k != 1 + 2 * NUM_SIZES; k += 1) {
        s = k == 0 ? -1 : (k - 1) % NUM_SIZES;
        c = k <= NUM_SIZES ? 1 : 2;
        for (y = 0; y != 8; y += 1)
            for (x = 0; x != 8; x += 1)
                px[y][x] = s >= 0 && (x-3)*(x-3) + (y-3)*(y-3) <= round_r2[s] ? c : 0;
        snprintf(comment, sizeof comment, "%d", k);
        emit_tile(px, comment);
    }
    printf("};\n\n");

    /* radius grows linearly with |v|, snapped down to the largest round shape */
    printf("const unsigned char TABLE_AT(0x%04x) value_to_tile[256] = {\n", PAGE_TO_TILE);
    for (k = 0; k != 256; k += 1) {
        v = (signed char)k;
        r = (v < 0 ? -v : v) * (3.7 / 128.0);
        s = -1;
        while (v != 0 && s + 1 != NUM_SIZES && round_r2[s + 1] <= r*r)
            s += 1;
        printf("    /* %+4d */ %2d%s\n", v,
               s < 0 ? 0 : (v > 0 ? 1 + s : 1 + NUM_SIZES + s),
               k != 255 ? "," : "");
    }
    printf("};\n\n");
}


/*
 * 5x8 font, one string per glyph: 8 rows of 5 pixels separated by spaces,
 * row 7 is the descender. Drawn at columns 1..5 of the tile.
 * Characters not listed are blank.
 */
struct glyph {
    char c;
    const char *rows;
};

struct glyph glyphs[] = {
    {'0', ".###. #...# #..## #.#.# ##..# #...# .###. ....."},
    {'1', "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###. ....."},
    {'2', ".###. #...# ....# ...#. ..#.. .#... ##### ....."},
    {'3', "##### ...#. ..#.. ...#. ....# #...# .###. ....."},
    {'4', "...#. ..##. .#.#. #..#. ##### ...#. ...#. ....."},
    {'5', "##### #.... ####. ....# ....# #...# .###. ....."},
    {'6', "..##. .#... #.... ####. #...# #...# .###. ....."},
    {'7', "##### ....# ...#. ..#.. .#... .#... .#... ....."},
    {'8', ".###. #...# #...# .###. #...# #...# .###. ....."},
    {'9', ".###. #...# #...# .#### ....# ...#. .##.. ....."},
    {'a', "..... ..... .###. ....# .#### #...# .#### ....."},
    {'b', "#.... #.... #.##. ##..# #...# #...# ####. ....."},
    {'c', "..... ..... .###. #.... #.... #...# .###. ....."},
    {'d', "....# ....# .##.# #..## #...# #...# .#### ....."},
    {'e', "..... ..... .###. #...# ##### #.... .###. ....."},
    {'f', "..##. .#..# .#... ###.. .#... .#... .#... ....."},
    {'g', "..... ..... .#### #...# #...# .#### ....# .###."},
    {'h', "#.... #.... #.##. ##..# #...# #...# #...# ....."},
    {'i', "..#.. ..... .##.. ..#.. ..#.. ..#.. .###. ....."},
    {'j', "...#. ..... ..##. ...#. ...#. ...#. #..#. .##.."},
    {'k', "#.... #.... #..#. #.#.. ##... #.#.. #..#. ....."},
    {'l', ".##.. ..#.. ..#.. ..#.. ..#.. ..#.. .###. ....."},
    {'m', "..... ..... ##.#. #.#.# #.#.# #...# #...# ....."},
    {'n', "..... ..... #.##. ##..# #...# #...# #...# ....."},
    {'o', "..... ..... .###. #...# #...# #...# .###. ....."},
    {'p', "..... ..... ####. #...# #...# ####. #.... #...."},
    {'q', "..... ..... .#### #...# #...# .#### ....# ....#"},
    {'r', "..... ..... #.##. ##..# #.... #.... #.... ....."},
    {'s', "..... ..... .#### #.... .###. ....# ####. ....."},
    {'t', ".#... .#... ###.. .#... .#... .#..# ..##. ....."},
    {'u', "..... ..... #...# #...# #...# #..## .##.# ....."},
    {'v', "..... ..... #...# #...# #...# .#.#. ..#.. ....."},
    {'w', "..... ..... #...# #...# #.#.# #.#.# .#.#. ....."},
    {'x', "..... ..... #...# .#.#. ..#.. .#.#. #...# ....."},
    {'y', "..... ..... #...# #...# #...# .#### ....# .###."},
    {'z', "..... ..... ##### ...#. ..#.. .#... ##### ....."},
    {'+', "..... ..#.. ..#.. ##### ..#.. ..#.. ..... ....."},
    {'-', "..... ..... ..... ##### ..... ..... ..... ....."},
    {'*', "..... ..#.. #.#.# .###. #.#.# ..#.. ..... ....."},
    {'/', "..... ....# ...#. ..#.. .#... #.... ..... ....."},
    {'%', "##... ##..# ...#. ..#.. .#... #..## ...## ....."},
    {'&', ".##.. #..#. #.#.. .#... #.#.# #..#. .##.# ....."},
    {'|', "..#.. ..#.. ..#.. ..#.. ..#.. ..#.. ..#.. ....."},
    {'^', "..#.. .#.#. #...# ..... ..... ..... ..... ....."},
    {'<', "...#. ..#.. .#... #.... .#... ..#.. ...#. ....."},
    {'>', ".#... ..#.. ...#. ....# ...#. ..#.. .#... ....."},
    {'=', "..... ..... ##### ..... ##### ..... ..... ....."},
    {'?', ".###. #...# ....# ...#. ..#.. ..... ..#.. ....."},
    {'!', "..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#.. ....."},
    {':', "..... .##.. .##.. ..... .##.. .##.. ..... ....."},
    {'.', "..... ..... ..... ..... ..... .##.. .##.. ....."},
    {'(', "...#. ..#.. .#... .#... .#... ..#.. ...#. ....."},
    {')', ".#... ..#.. ...#. ...#. ...#. ..#.. .#... ....."},
    {'_', "..... ..... ..... ..... ..... ..... ##### ....."},
};


/* ASCII 32..127 in colour c */
void
calc_font(const char *name, int c)
{
    int px[8][8];
    int ch = 0;
    int g = 0;
    int x = 0;
    int y = 0;
    const char *rows = NULL;
    char comment[8];

    printf("const unsigned char %s[] = {\n", name);
    for (ch = 32; ch != 128; ch += 1) {
        memset(px, 0, sizeof px);
        for (g = 0; g != (int)(sizeof glyphs / sizeof glyphs[0]); g += 1) {
            if (glyphs[g].c != ch)
                continue;
            rows = glyphs[g].rows;
            for (y = 0; y != 8; y += 1)
                for (x = 0; x != 5; x += 1)
                    px[y][1 + x] = rows[y * 6 + x] == '#' ? c : 0;
        }
        snprintf(comment, sizeof comment, "'%c'", ch);
        emit_tile(px, comment);
    }
    printf("};\n\n");
}


/* x, y and i for each of the 256 cells, so the interpreter can point at them */
void
calc_cells(void)
{
    int k = 0;

    printf("const signed char TABLE_AT(0x%04x) cell_x[256] = {", PAGE_CELL_X);
    for (k = 0; k != 256; k += 1)
        printf("%s%d,", k % 16 ? "" : "\n    ", k % 16);
    printf("\n};\n\nconst signed char TABLE_AT(0x%04x) cell_y[256] = {", PAGE_CELL_Y);
    for (k = 0; k != 256; k += 1)
        printf("%s%d,", k % 16 ? "" : "\n    ", k / 16);
    printf("\n};\n\nconst signed char TABLE_AT(0x%04x) cell_i[256] = {", PAGE_CELL_I);
    for (k = 0; k != 256; k += 1)
        printf("%s%d,", k % 16 ? "" : "\n    ", (signed char)k);
    printf("\n};\n\n");
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
double angle_cos(double i) { return cos(i * (M_PI/128.0)) * 127; }
double angle_tan(double i) { return tan(i * (M_PI/128.0)) * 127; }
/* input scaled like an angle, output is an angle: range about +-51 */
double angle_atan(double i) { return atan(i * (M_PI/128.0)) * (128.0/M_PI); }
double int_sqrt(double i) { return i < 0 ? 0 : floor(sqrt(i)); }
/* saturating, so neg/abs of -128 give 127 */
double neg(double i) { return -i; }
double absolute(double i) { return fabs(i); }


void
calc_table(const char *name, int page, fn_t f)
{
    int k;
    int i;
    double d;

    printf("const signed char TABLE_AT(0x%04x) %s[256] = {\n", page, name);
    for (k = 0; k != 256; k += 1) {
        i = (signed char)k;
        d = f(i);
        printf("    /* %+4d %+9.2f */ %+4d", i, fabs(d) < 1e6 ? d : 0, to_i8(d));
        printf("%s\n", k != 255 ? "," : "");
    }
    printf("};\n\n");
}


int
main(void)
{
    printf("#ifdef SDL\n#define TABLE_AT(addr)\n#else\n#define TABLE_AT(addr) __at(addr)\n#endif\n\n");
    calc_circles();
    calc_font("font_gfx", 1);
    calc_font("font_hi_gfx", 2);
    calc_cells();
    calc_table("sin_table",  PAGE_SIN,  angle_sin);
    calc_table("cos_table",  PAGE_COS,  angle_cos);
    calc_table("tan_table",  PAGE_TAN,  angle_tan);
    calc_table("atan_table", PAGE_ATAN, angle_atan);
    calc_table("sqrt_table", PAGE_SQRT, int_sqrt);
    calc_table("neg_table",  PAGE_NEG,  neg);
    calc_table("abs_table",  PAGE_ABS,  absolute);
    return 0;
}
