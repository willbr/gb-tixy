typedef unsigned char u8;
typedef   signed char i8;


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

/* tables are stored in u8 order (0..127, -128..-1) so the cast is the index */

i8
gb_sin(i8 i)
{
    return sin_table[(u8)i];
}

i8
gb_cos(i8 i)
{
    return cos_table[(u8)i];
}


i8
gb_tan(i8 i)
{
    return tan_table[(u8)i];
}


i8
gb_atan(i8 i)
{
    return atan_table[(u8)i];
}


i8
gb_sqrt(i8 i)
{
    return sqrt_table[(u8)i];
}


#endif
