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

/*
 * The operators, one row each; the enum and every per-op table are
 * generated from this list. The order is the editor's Up/Down order.
 *
 *   VAR     push `arg` (x, y, i: ROM tables), or t when arg is 0
 *   UNARY   lookup: arg is the table
 *   BINARY  arg is the expression in a and b; fn is the rpn_gb.s loop,
 *           0 to use the C loop on the GB too
 *   STACK   handled in prog_eval
 *
 * Divide by zero gives 0. Shifts past 7 give 0 (<<) or the sign (>>).
 * Comparisons give 127 (a full circle) or 0.
 */
#define OPS(F)                                                         \
    /* name  text    in out kind    arg                       fn */    \
    F(T,    "t",    0, 1, VAR,    0,                         0)       \
    F(X,    "x",    0, 1, VAR,    cell_x,                    0)       \
    F(Y,    "y",    0, 1, VAR,    cell_y,                    0)       \
    F(I,    "i",    0, 1, VAR,    cell_i,                    0)       \
    F(SIN,  "sin",  1, 1, UNARY,  sin_table,                 0)       \
    F(COS,  "cos",  1, 1, UNARY,  cos_table,                 0)       \
    F(TAN,  "tan",  1, 1, UNARY,  tan_table,                 0)       \
    F(ATAN, "atan", 1, 1, UNARY,  atan_table,                0)       \
    F(SQRT, "sqrt", 1, 1, UNARY,  sqrt_table,                0)       \
    F(NEG,  "neg",  1, 1, UNARY,  neg_table,                 0)       \
    F(ABS,  "abs",  1, 1, UNARY,  abs_table,                 0)       \
    F(ADD,  "+",    2, 1, BINARY, a + b,                     asm_add) \
    F(SUB,  "-",    2, 1, BINARY, a - b,                     asm_sub) \
    F(MUL,  "*",    2, 1, BINARY, a * b,                     asm_mul) \
    F(DIV,  "/",    2, 1, BINARY, b ? a / b : 0,             0)       \
    F(MOD,  "%",    2, 1, BINARY, b ? a % b : 0,             0)       \
    F(AND,  "&",    2, 1, BINARY, a & b,                     asm_and) \
    F(OR,   "|",    2, 1, BINARY, a | b,                     asm_or)  \
    F(XOR,  "^",    2, 1, BINARY, a ^ b,                     asm_xor) \
    F(SHL,  "<<",   2, 1, BINARY, (u8)b > 7 ? 0 : a << b,    asm_shl) \
    F(SHR,  ">>",   2, 1, BINARY, a >> ((u8)b > 7 ? 7 : b),  asm_shr) \
    F(LT,   "<",    2, 1, BINARY, a < b ? 127 : 0,           asm_lt)  \
    F(GT,   ">",    2, 1, BINARY, a > b ? 127 : 0,           asm_gt)  \
    F(EQ,   "=",    2, 1, BINARY, a == b ? 127 : 0,          asm_eq)  \
    F(SEL,  "?",    3, 1, STACK,  0,                         0)       \
    F(DUP,  "dup",  1, 2, STACK,  0,                         0)       \
    F(SWAP, "swap", 2, 2, STACK,  0,                         0)

/*
 * Generators. `name` is always pasted (OP_##name, s_##name) before it
 * reaches another macro, so a header macro called ADD or I can't expand it.
 */
#define OP_ENUM(name, text, in, out, kind, arg, fn)  OP_##name,
enum { OPS(OP_ENUM) OP_COUNT };

enum { KIND_VAR, KIND_UNARY, KIND_BINARY, KIND_STACK };

/* tokens NUM_BASE + n push the literal n, 0..127 */
#define NUM_BASE 32
#define NUM_MAX  127
#define IS_NUM(tok) ((tok) >= NUM_BASE)

typedef char ops_fit_below_num_base[OP_COUNT <= NUM_BASE ? 1 : -1];

#define OP_TEXT(name, text, in, out, kind, arg, fn)  text,
#define OP_IN(name, text, in, out, kind, arg, fn)    in,
#define OP_OUT(name, text, in, out, kind, arg, fn)   out,
#define OP_KIND(name, text, in, out, kind, arg, fn)  KIND_##kind,

const char *const op_names[OP_COUNT] = { OPS(OP_TEXT) };
const u8 op_in[OP_COUNT]   = { OPS(OP_IN) };     /* values popped */
const u8 op_out[OP_COUNT]  = { OPS(OP_OUT) };    /* values pushed */
const u8 op_kind[OP_COUNT] = { OPS(OP_KIND) };


#define PROG_MAX  24
#define STACK_MAX 6

typedef struct {
    u8 len;
    u8 tok[PROG_MAX];
} prog_t;


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


/* VAR and UNARY rows: the table pushed or looked up; 0 for the rest */
#define ARG_VAR(arg)     arg
#define ARG_UNARY(arg)   arg
#define ARG_BINARY(arg)  0
#define ARG_STACK(arg)   0
#define OP_TABLE(name, text, in, out, kind, arg, fn)  ARG_##kind(arg),

const i8 *const op_table[OP_COUNT] = { OPS(OP_TABLE) };


/*
 * BINARY rows: a scalar version and a 256-wide version from one expression.
 * The 256-wide C loops are the SDL path and the reference for rpn_gb.s.
 */
#define DEFINE_VAR(s_fn, v_fn, expr)
#define DEFINE_UNARY(s_fn, v_fn, expr)
#define DEFINE_STACK(s_fn, v_fn, expr)
#define DEFINE_BINARY(s_fn, v_fn, expr)                            \
    i8 s_fn(i8 a, i8 b) { return (expr); }                         \
    void v_fn(i8 *d, const i8 *pa, const i8 *pb)                   \
    {                                                              \
        u8 n = 0;                                                  \
        i8 a, b;                                                   \
        do {                                                       \
            a = *pa++;                                             \
            b = *pb++;                                             \
            *d++ = (expr);                                         \
        } while (++n);                                             \
    }
#define OP_DEFINE(name, text, in, out, kind, arg, fn)  DEFINE_##kind(s_##name, v_##name, arg)

OPS(OP_DEFINE)

#define FN_VAR(f)     0
#define FN_UNARY(f)   0
#define FN_BINARY(f)  f
#define FN_STACK(f)   0
#define OP_S(name, text, in, out, kind, arg, fn)  FN_##kind(s_##name),
#define OP_V(name, text, in, out, kind, arg, fn)  FN_##kind(v_##name),

i8 (*const binop_s[OP_COUNT])(i8, i8) = { OPS(OP_S) };
void (*const binop_v[OP_COUNT])(i8 *, const i8 *, const i8 *) = { OPS(OP_V) };


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

/* the fn column; 0: no asm version, use the C loop */
#define OP_ASM(name, text, in, out, kind, arg, fn)  FN_##kind(fn),

void (*const binop_asm[OP_COUNT])(void) = { OPS(OP_ASM) };

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

        switch (op_kind[op]) {
        case KIND_VAR:
            stack_arr[sp] = op_table[op];    /* 0 for t: a scalar */
            stack_val[sp] = t;
            sp++;
            break;

        case KIND_UNARY:
            k = sp - 1;
            if (stack_arr[k]) {
                d = free_buf(sp);
                run_lut(d, stack_arr[k], op_table[op]);
                stack_arr[k] = d;
            } else {
                stack_val[k] = op_table[op][(u8)stack_val[k]];
            }
            break;

        case KIND_BINARY:
            k = sp - 2;
            if (op == OP_MUL && !stack_arr[k] != !stack_arr[k + 1]) {
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

        default: /* KIND_STACK */
            switch (op) {
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
            }
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
