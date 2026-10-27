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


/*
 * Every 256-value array starts on a 256-byte boundary (the ROM tables are
 * placed with TABLE_AT, the RAM buffers come from the aligned arena), so
 * element n of any array is page:n and the GB asm loops index them all
 * with the one register L.
 */
#define NUM_PAGES (STACK_MAX + 1 + 3 + 1)   /* pool, bcast, screen */

u8 arena[(NUM_PAGES + 1) * 256];
i8 *pool[STACK_MAX + 1];
i8 *bcast[3];
const i8 *stack_arr[STACK_MAX];   /* NULL: the slot is the scalar stack_val */
i8 stack_val[STACK_MAX];


/* carve the arena into pages; returns the page for the screen tiles */
u8 *
rpn_init(void)
{
    u8 *page = (u8 *)(((size_t)arena + 255) & ~(size_t)255);
    u8 k = 0;

    for (k = 0; k != STACK_MAX + 1; k += 1, page += 256)
        pool[k] = (i8 *)page;
    for (k = 0; k != 3; k += 1, page += 256)
        bcast[k] = (i8 *)page;
    return page;
}


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


/* unary ops are all table lookups */
const i8 *const unary_lut[] = {
    sin_table, cos_table, tan_table, atan_table, sqrt_table, neg_table, abs_table,
};


/*
 * Binary ops: a scalar version and a 256-wide version from one expression.
 * Divide by zero gives 0. Shifts past 7 give 0 (<<) or the sign (>>).
 * Comparisons give 127 (a full circle) or 0.
 * On the GB the 256-wide versions are the reference; the asm below is used
 * for everything but / and %.
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


#ifdef SDL

void
run_fill(void *d, i8 v)
{
    memset(d, v, 256);
}

/* d[v] = v * k for every v: array * scalar becomes a lookup */
void
run_mul_table(i8 *d, i8 k)
{
    u8 n = 0;
    i8 acc = 0;
    do {
        d[n] = acc;
        acc += k;
    } while (++n);
}

void
run_lut(void *d, const i8 *a, const void *lut)
{
    u8 *pd = d;
    const u8 *l = lut;
    u8 n = 0;
    do {
        *pd++ = l[(u8)*a++];
    } while (++n);
}

void
run_bin(u8 op, i8 *d, const i8 *a, const i8 *b)
{
    binop_v[op](d, a, b);
}

/* c ? a : b */
void
run_sel(i8 *d, const i8 *c, const i8 *a, const i8 *b)
{
    u8 n = 0;
    do {
        *d++ = *c++ ? *a : *b;
        a++;
        b++;
    } while (++n);
}

#else

/*
 * SM83 loops in rpn_gb.s. Arguments are page numbers (address >> 8) in
 * these globals; every array they touch is 256-byte aligned.
 */
u8 rpn_p0, rpn_p1, rpn_p2, rpn_p3;

#define PAGE(ptr) ((u8)((size_t)(ptr) >> 8))

void asm_add(void);
void asm_sub(void);
void asm_and(void);
void asm_or(void);
void asm_xor(void);
void asm_lt(void);
void asm_gt(void);
void asm_eq(void);
void asm_mul(void);
void asm_shl(void);
void asm_shr(void);
void asm_lut(void);
void asm_sel(void);
void asm_fill(void);
void asm_mul_table(void);

/* NULL: no asm version, use the C loop */
void (*const binop_asm[])(void) = {
    asm_add, asm_sub, asm_mul, NULL, NULL, asm_and, asm_or, asm_xor, asm_shl, asm_shr,
    asm_lt, asm_gt, asm_eq,
};

void
run_fill(void *d, i8 v)
{
    rpn_p0 = PAGE(d);
    rpn_p1 = v;
    asm_fill();
}

void
run_mul_table(i8 *d, i8 k)
{
    rpn_p0 = PAGE(d);
    rpn_p1 = k;
    asm_mul_table();
}

void
run_lut(void *d, const i8 *a, const void *lut)
{
    rpn_p0 = PAGE(a);
    rpn_p1 = PAGE(d);
    rpn_p2 = PAGE(lut);
    asm_lut();
}

void
run_bin(u8 op, i8 *d, const i8 *a, const i8 *b)
{
    if (!binop_asm[op]) {
        binop_v[op](d, a, b);
        return;
    }
    rpn_p0 = PAGE(a);
    rpn_p1 = PAGE(b);
    rpn_p2 = PAGE(d);
    binop_asm[op]();
}

void
run_sel(i8 *d, const i8 *c, const i8 *a, const i8 *b)
{
    rpn_p0 = PAGE(c);
    rpn_p1 = PAGE(a);
    rpn_p2 = PAGE(b);
    rpn_p3 = PAGE(d);
    asm_sel();
}

#endif


/*
 * A pool buffer that none of the first `live` slots point at. Pool pages are
 * consecutive, so a slot's pool index is its page number minus the first's.
 */
const u8 bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

i8 *
free_buf(u8 live)
{
    u8 first = (u8)((size_t)pool[0] >> 8);
    u8 used = 0;
    u8 k = 0;
    u8 b = 0;

    for (k = 0; k != live; k += 1) {
        b = (u8)((size_t)stack_arr[k] >> 8) - first;
        if (stack_arr[k] && b <= STACK_MAX)
            used |= bit[b];
    }
    /* live <= STACK_MAX slots, so one of the STACK_MAX + 1 pages is free */
    for (k = 0; used & 1; k += 1)
        used >>= 1;
    return pool[k];
}


/* slot k as an array; scalars are broadcast into bcast[j] */
const i8 *
slot_arr(u8 k, u8 j)
{
    if (stack_arr[k])
        return stack_arr[k];
    run_fill(bcast[j], stack_val[k]);
    return bcast[j];
}


/* run a program that passed prog_ok, leaving tile indices in out (a page) */
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
                run_lut(d, stack_arr[k], unary_lut[op]);
                stack_arr[k] = d;
            } else {
                stack_val[k] = unary_lut[op][(u8)stack_val[k]];
            }
            break;

        case OP_SEL:
            k = sp - 3;
            if (stack_arr[k] || stack_arr[k + 1] || stack_arr[k + 2]) {
                d = free_buf(sp);
                run_sel(d, slot_arr(k, 0), slot_arr(k + 1, 1), slot_arr(k + 2, 2));
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
            if (op == OP_MUL - OP_ADD && !stack_arr[k] != !stack_arr[k + 1]) {
                /* array * scalar: build v * k once, then a lookup */
                a = stack_arr[k] ? stack_arr[k] : stack_arr[k + 1];
                run_mul_table(bcast[0], stack_arr[k] ? stack_val[k + 1] : stack_val[k]);
                d = free_buf(sp);
                run_lut(d, a, bcast[0]);
                stack_arr[k] = d;
            } else if (stack_arr[k] || stack_arr[k + 1]) {
                d = free_buf(sp);
                run_bin(op, d, slot_arr(k, 0), slot_arr(k + 1, 1));
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
        run_lut(out, stack_arr[k], value_to_tile);
    } else {
        run_fill(out, value_to_tile[(u8)stack_val[k]]);
    }
}
