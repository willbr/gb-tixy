/*
 * Formulas as RPN token programs, evaluated a whole grid at a time:
 * each token runs once over all 256 cells instead of once per cell, so the
 * dispatch cost is paid per token per frame and the work is tight loops.
 *
 * A stack slot is either a scalar (t, literals, ops on scalars) or a pointer
 * to 256 values. x, y, i point straight at ROM tables, so pushing them is
 * free. Results always go to a pool buffer no live slot points at, so dup
 * and swap can share buffers safely.
 */

enum {
    OP_T, OP_X, OP_Y, OP_I,
    OP_SIN, OP_COS, OP_TAN, OP_ATAN, OP_SQRT, OP_NEG, OP_ABS,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
    OP_LT, OP_GT, OP_EQ,
    OP_SEL,
    OP_DUP, OP_SWAP,
    OP_COUNT
};

/* tokens NUM_BASE + n push the literal n, 0..127 */
#define NUM_BASE 32
#define NUM_MAX  127
#define IS_NUM(tok) ((tok) >= NUM_BASE)

#define PROG_MAX  24
#define STACK_MAX 6

typedef struct {
    u8 len;
    u8 tok[PROG_MAX];
} prog_t;

const char *const op_names[OP_COUNT] = {
    "t", "x", "y", "i",
    "sin", "cos", "tan", "atan", "sqrt", "neg", "abs",
    "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
    "<", ">", "=",
    "?",
    "dup", "swap",
};

/* stack effect: values popped, values pushed */
const u8 op_in[OP_COUNT] = {
    0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2,
    3,
    1, 2,
};

const u8 op_out[OP_COUNT] = {
    1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1,
    1,
    2, 2,
};


i8 pool[STACK_MAX + 1][256];
i8 bcast[3][256];
const i8 *stack_arr[STACK_MAX];   /* NULL: the slot is the scalar stack_val */
i8 stack_val[STACK_MAX];


/* 1 if the program never underflows or overflows and leaves a value */
u8
prog_ok(const prog_t *p)
{
    u8 n = 0;
    u8 tok = 0;
    u8 in = 0;
    u8 depth = 0;

    for (n = 0; n != p->len; n += 1) {
        tok = p->tok[n];
        in = IS_NUM(tok) ? 0 : op_in[tok];
        if (depth < in)
            return 0;
        depth = depth - in + (IS_NUM(tok) ? 1 : op_out[tok]);
        if (depth > STACK_MAX)
            return 0;
    }
    return depth != 0;
}


/* "y 5 * t + sin" -> tokens; unknown words are skipped */
void
prog_parse(prog_t *p, const char *s)
{
    const char *w = NULL;
    u8 len = 0;
    u8 op = 0;
    u8 v = 0;

    p->len = 0;
    for (;;) {
        while (*s == ' ')
            s++;
        if (*s == 0 || p->len == PROG_MAX)
            return;

        w = s;
        while (*s && *s != ' ')
            s++;
        len = s - w;

        if (*w >= '0' && *w <= '9') {
            v = 0;
            while (w != s)
                v = v * 10 + (*w++ - '0');
            p->tok[p->len++] = NUM_BASE + (v > NUM_MAX ? NUM_MAX : v);
            continue;
        }

        for (op = 0; op != OP_COUNT; op += 1) {
            if (strlen(op_names[op]) == len && memcmp(op_names[op], w, len) == 0) {
                p->tok[p->len++] = op;
                break;
            }
        }
    }
}


/* the token's text into out (no terminator), returns its length */
u8
tok_text(u8 tok, u8 *out)
{
    const char *s = NULL;
    u8 n = 0;

    if (IS_NUM(tok)) {
        tok -= NUM_BASE;
        if (tok >= 100) out[n++] = '0' + tok / 100;
        if (tok >= 10)  out[n++] = '0' + tok / 10 % 10;
        out[n++] = '0' + tok % 10;
        return n;
    }

    for (s = op_names[tok]; *s; s++)
        out[n++] = *s;
    return n;
}


/* unary ops are all table lookups: sin(a), cos(a) = sin(a + 64), ... */
const i8 *const unary_lut[] = {
    sin_table, sin_table, tan_table, atan_table, sqrt_table, neg_table, abs_table,
};
const u8 unary_off[] = { 0, 64, 0, 0, 0, 0, 0 };

void
lut_loop(i8 *d, const i8 *a, const i8 *lut, u8 off)
{
    u8 n = 0;
    do {
        *d++ = lut[(u8)(*a++ + off)];
    } while (++n);
}


/*
 * Binary ops: a scalar version and a 256-wide version from one expression.
 * Divide by zero gives 0. Shifts past 7 give 0 (<<) or the sign (>>).
 * Comparisons give 127 (a full circle) or 0.
 */
#define BINOP(name, expr)                                          \
    i8 s_##name(i8 a, i8 b) { return (expr); }                     \
    void v_##name(i8 *d, const i8 *pa, const i8 *pb)               \
    {                                                              \
        u8 n = 0;                                                  \
        i8 a, b;                                                   \
        do {                                                       \
            a = *pa++;                                             \
            b = *pb++;                                             \
            *d++ = (expr);                                         \
        } while (++n);                                             \
    }

BINOP(add, a + b)
BINOP(sub, a - b)
BINOP(mul, a * b)
BINOP(div, b ? a / b : 0)
BINOP(mod, b ? a % b : 0)
BINOP(and, a & b)
BINOP(or,  a | b)
BINOP(xor, a ^ b)
BINOP(shl, (u8)b > 7 ? 0 : a << b)
BINOP(shr, a >> ((u8)b > 7 ? 7 : b))
BINOP(lt,  a < b ? 127 : 0)
BINOP(gt,  a > b ? 127 : 0)
BINOP(eq,  a == b ? 127 : 0)

i8 (*const binop_s[])(i8, i8) = {
    s_add, s_sub, s_mul, s_div, s_mod, s_and, s_or, s_xor, s_shl, s_shr,
    s_lt, s_gt, s_eq,
};
void (*const binop_v[])(i8 *, const i8 *, const i8 *) = {
    v_add, v_sub, v_mul, v_div, v_mod, v_and, v_or, v_xor, v_shl, v_shr,
    v_lt, v_gt, v_eq,
};


/* c ? a : b */
void
sel_loop(i8 *d, const i8 *c, const i8 *a, const i8 *b)
{
    u8 n = 0;
    do {
        *d++ = *c++ ? *a : *b;
        a++;
        b++;
    } while (++n);
}


/*
 * A pool buffer that none of the first `live` slots point at. live <= STACK_MAX
 * slots can hold at most STACK_MAX buffers, so if the first STACK_MAX are all
 * taken the last one is free.
 */
i8 *
free_buf(u8 live)
{
    u8 b = 0;
    u8 k = 0;

    for (b = 0; b != STACK_MAX; b += 1) {
        for (k = 0; k != live; k += 1)
            if (stack_arr[k] == pool[b])
                break;
        if (k == live)
            break;
    }
    return pool[b];
}


/* slot k as an array; scalars are broadcast into bcast[j] */
const i8 *
slot_arr(u8 k, u8 j)
{
    if (stack_arr[k])
        return stack_arr[k];
    memset(bcast[j], stack_val[k], 256);
    return bcast[j];
}


/* run a program that passed prog_ok, leaving tile indices in out[256] */
void
prog_eval(const prog_t *p, i8 t, u8 *out)
{
    const u8 *tok = p->tok;
    const u8 *end = p->tok + p->len;
    u8 sp = 0;
    u8 op = 0;
    u8 k = 0;
    i8 *d = NULL;
    const i8 *a = NULL;

    for (; tok != end; tok++) {
        op = *tok;

        if (IS_NUM(op)) {
            stack_arr[sp] = NULL;
            stack_val[sp] = op - NUM_BASE;
            sp++;
            continue;
        }

        switch (op) {
        case OP_T:
            stack_arr[sp] = NULL;
            stack_val[sp] = t;
            sp++;
            break;

        case OP_X: stack_arr[sp++] = cell_x; break;
        case OP_Y: stack_arr[sp++] = cell_y; break;
        case OP_I: stack_arr[sp++] = cell_i; break;

        case OP_SIN: case OP_COS: case OP_TAN: case OP_ATAN:
        case OP_SQRT: case OP_NEG: case OP_ABS:
            k = sp - 1;
            op -= OP_SIN;
            if (stack_arr[k]) {
                d = free_buf(sp);
                lut_loop(d, stack_arr[k], unary_lut[op], unary_off[op]);
                stack_arr[k] = d;
            } else {
                stack_val[k] = unary_lut[op][(u8)(stack_val[k] + unary_off[op])];
            }
            break;

        case OP_SEL:
            k = sp - 3;
            if (stack_arr[k] || stack_arr[k + 1] || stack_arr[k + 2]) {
                d = free_buf(sp);
                sel_loop(d, slot_arr(k, 0), slot_arr(k + 1, 1), slot_arr(k + 2, 2));
                stack_arr[k] = d;
            } else {
                stack_val[k] = stack_val[k] ? stack_val[k + 1] : stack_val[k + 2];
            }
            sp -= 2;
            break;

        case OP_DUP:
            stack_arr[sp] = stack_arr[sp - 1];
            stack_val[sp] = stack_val[sp - 1];
            sp++;
            break;

        case OP_SWAP:
            a = stack_arr[sp - 1];
            stack_arr[sp - 1] = stack_arr[sp - 2];
            stack_arr[sp - 2] = a;
            k = stack_val[sp - 1];
            stack_val[sp - 1] = stack_val[sp - 2];
            stack_val[sp - 2] = k;
            break;

        default: /* binary */
            k = sp - 2;
            op -= OP_ADD;
            if (stack_arr[k] || stack_arr[k + 1]) {
                d = free_buf(sp);
                binop_v[op](d, slot_arr(k, 0), slot_arr(k + 1, 1));
                stack_arr[k] = d;
            } else {
                stack_val[k] = binop_s[op](stack_val[k], stack_val[k + 1]);
            }
            sp--;
            break;
        }
    }

    k = sp - 1;
    if (stack_arr[k]) {
        a = stack_arr[k];
        k = 0;
        do {
            *out++ = value_to_tile[(u8)*a++];
        } while (++k);
    } else {
        memset(out, value_to_tile[(u8)stack_val[k]], 256);
    }
}
