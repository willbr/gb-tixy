#define MAP_WIDTH 32;
#define MAP_HEIGHT 32;


typedef unsigned char u8;
typedef   signed char i8;

struct gb_row {
#ifdef SDL
    u8 high;
    u8 low;
#else
    u8 low;
    u8 high;
#endif
};


struct gb_tile {
    struct gb_row row[8];
};


void init(void);
void update(void);
void render(void);

i8 i8_abs(i8);


u8 dither_42[8][8] = {
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


u8
closest_colour(u8 x, u8 y, i8 c)
{
    u8 s = 0;
    i8 limit = dither_42[y % 8][x % 8];
    i8 sect = c / sector_size;

    if (c < 0)
        c = -c;

    s = (c % sector_size) > limit ? sect + 1 : sect;
    s = s > 3 ? 3 : s;

    return s;
}


u8
tile_pget(struct gb_tile *t, u8 x, u8 y)
{
    struct gb_row *r = &t->row[y];
    u8 mask = (1 << 7) >> x;
    u8 l = r->low  & mask ? 1 : 0;
    u8 h = r->high & mask ? 2 : 0;
    u8 c = l + h;
    return c;
}


void
tile_pset(struct gb_tile *t, u8 x, u8 y, u8 c)
{
    struct gb_row *r = &t->row[y];
    u8 mask = (1 << 7) >> x;
    u8 *l = &r->low;
    u8 *h = &r->high;

    *l = (c & 1) ? (*l | mask) : (*l & ~mask);
    *h = (c & 2) ? (*h | mask) : (*h & ~mask);
}

i8
i8_abs(i8 n)
{
    if (n == -128)
        return 127;
    else if (n < 0)
        return -n;
    else
        return n;
}


#ifdef INCLUDE_TABLES
#include "tables.h"

i8
gb_sin(i8 i)
{
    return sin_table[i];
}

i8
gb_cos(i8 i)
{
    return cos_table[i];
}


i8
gb_tan(i8 i)
{
    return tan_table[i];
}


i8
gb_atan(i8 i)
{
    return atan_table[i];
}


i8
gb_sqrt(i8 i)
{
    return sqrt_table[i];
}


#endif

