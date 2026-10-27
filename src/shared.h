typedef unsigned char u8;
typedef   signed char i8;


#ifdef INCLUDE_TABLES
#include "tables.h"

/*
 * Macros, not functions: SDCC won't inline, so each lookup would be a call.
 * Tables are stored in u8 order (0..127, -128..-1) so the cast is the index.
 * Angles: 128 == pi, so cos is sin a quarter turn (64) later.
 */
#define gb_sin(i)  sin_table[(u8)(i)]
#define gb_cos(i)  sin_table[(u8)((i) + 64)]
#define gb_tan(i)  tan_table[(u8)(i)]
#define gb_atan(i) atan_table[(u8)(i)]
#define gb_sqrt(i) sqrt_table[(u8)(i)]

#endif
