;
; SM83 inner loops for the RPN interpreter (src/rpn.h), Game Boy only.
; The SDL build uses the C loops in rpn.h, which are also the reference
; these must match exactly.
;
; Every array is 256 values on a 256-byte boundary, so element n of the
; array on page P is at P:n. L holds n for all of them; each access loads
; that array's page into H. Loops end when L wraps back to 0.
;
; Arguments are page numbers (address >> 8) in _rpn_p0.._rpn_p3, set by the
; run_* wrappers in rpn.h. Every register is caller-saved under SDCC.
;

        .module rpn_gb

        .globl  _rpn_p0, _rpn_p1, _rpn_p2, _rpn_p3
        .globl  _screen
        .globl  _STAT_REG

        .globl  _asm_add, _asm_sub, _asm_and, _asm_or, _asm_xor
        .globl  _asm_lt, _asm_gt, _asm_eq
        .globl  _asm_mul, _asm_shl, _asm_shr
        .globl  _asm_lut, _asm_sel, _asm_fill, _asm_mul_table
        .globl  _copy_grid

        .area   _CODE


; B = p0, C = p1, D = p2, L = 0
        .macro  ENTRY
        ld      a, (_rpn_p0)
        ld      b, a
        ld      a, (_rpn_p1)
        ld      c, a
        ld      a, (_rpn_p2)
        ld      d, a
        ld      l, #0
        .endm

; store A to dest (page D) and step to the next n
        .macro  NEXT
        ld      h, d
        ld      (hl), a
        inc     l
        jr      nz, 1$
        ret
        .endm


;
; d = a OP b for ops that take (hl): p0 = a, p1 = b, p2 = dest.
; 13 cycles per value.
;
        .macro  BINOP insn
        ENTRY
1$:
        ld      h, b            ; a[n]
        ld      a, (hl)
        ld      h, c            ; OP b[n]
        insn    a, (hl)
        NEXT
        .endm

_asm_add::
        BINOP   add
_asm_sub::
        BINOP   sub
_asm_and::
        BINOP   and
_asm_or::
        BINOP   or
_asm_xor::
        BINOP   xor


;
; Signed compares give 127 or 0: flip both sign bits so an unsigned
; compare orders them as signed, then turn the carry into 0xff/0 and
; mask to 127.
;
_asm_lt::                       ; a < b
        ENTRY
1$:
        ld      h, c            ; E = b ^ 0x80
        ld      a, (hl)
        xor     a, #0x80
        ld      e, a
        ld      h, b            ; A = a ^ 0x80
        ld      a, (hl)
        xor     a, #0x80
        cp      a, e            ; carry if a < b
        sbc     a, a
        and     a, #0x7f
        NEXT

_asm_gt::                       ; a > b, i.e. b < a
        ENTRY
1$:
        ld      h, b            ; E = a ^ 0x80
        ld      a, (hl)
        xor     a, #0x80
        ld      e, a
        ld      h, c            ; A = b ^ 0x80
        ld      a, (hl)
        xor     a, #0x80
        cp      a, e            ; carry if b < a
        sbc     a, a
        and     a, #0x7f
        NEXT

_asm_eq::                       ; a == b
        ENTRY
1$:
        ld      h, c
        ld      e, (hl)         ; E = b
        ld      h, b
        ld      a, (hl)         ; A = a
        sub     a, e
        sub     a, #1           ; borrows only when a - b == 0
        sbc     a, a
        and     a, #0x7f
        NEXT


;
; a * b, low byte. Shift-and-add: between the page loads H is free, so it
; holds the multiplier; stops early once the multiplier runs out of bits.
;
_asm_mul::
        ENTRY
1$:
        ld      h, c
        ld      a, (hl)         ; multiplier b
        ld      h, b
        ld      e, (hl)         ; multiplicand a
        ld      h, a
        xor     a, a            ; product
2$:
        srl     h
        jr      nc, 3$
        add     a, e
3$:
        sla     e
        inc     h               ; Z if the multiplier is now 0
        dec     h
        jr      nz, 2$
        NEXT


;
; Shifts by b[n]. Counts 8..255 (which includes negatives) give 0 for <<
; and shift by 7 for >>, matching the C reference.
;
_asm_shl::
        ENTRY
1$:
        ld      h, c
        ld      a, (hl)         ; count
        cp      a, #8
        jr      c, 2$
        xor     a, a
        jr      5$
2$:
        ld      e, a
        ld      h, b
        ld      a, (hl)         ; value
        inc     e
        jr      4$
3$:
        add     a, a
4$:
        dec     e
        jr      nz, 3$
5$:
        NEXT

_asm_shr::
        ENTRY
1$:
        ld      h, c
        ld      a, (hl)         ; count
        cp      a, #8
        jr      c, 2$
        ld      a, #7
2$:
        ld      e, a
        ld      h, b
        ld      a, (hl)         ; value
        inc     e
        jr      4$
3$:
        sra     a
4$:
        dec     e
        jr      nz, 3$
        NEXT


;
; d = lut[a]: p0 = a, p1 = dest, p2 = lut. DE addresses the lut, so the
; value itself is the low byte. 12 cycles per value.
;
_asm_lut::
        ENTRY
1$:
        ld      h, b
        ld      e, (hl)         ; a[n]
        ld      a, (de)         ; lut[a[n]]
        ld      h, c
        ld      (hl), a
        inc     l
        jr      nz, 1$
        ret


;
; d = c ? a : b: p0 = c, p1 = a, p2 = b, p3 = dest in B, C, D, E.
;
_asm_sel::
        ENTRY
        ld      a, (_rpn_p3)
        ld      e, a
1$:
        ld      h, b
        ld      a, (hl)         ; c[n]
        or      a, a
        ld      h, c            ; a's page (ld keeps the flags)
        jr      nz, 2$
        ld      h, d            ; b's page
2$:
        ld      a, (hl)
        ld      h, e
        ld      (hl), a
        inc     l
        jr      nz, 1$
        ret


;
; Fill page p0 with p1: 4 bytes per loop, ~3 cycles a byte.
;
_asm_fill::
        ld      a, (_rpn_p0)
        ld      h, a
        ld      l, #0
        ld      a, (_rpn_p1)
        ld      b, #64
1$:
        ld      (hl+), a
        ld      (hl+), a
        ld      (hl+), a
        ld      (hl+), a
        dec     b
        jr      nz, 1$
        ret


;
; Page p0 [v] = v * p1 for every v, by repeated addition, so
; array * scalar becomes a lookup.
;
_asm_mul_table::
        ld      a, (_rpn_p0)
        ld      h, a
        ld      l, #0
        ld      a, (_rpn_p1)
        ld      e, a
        xor     a, a
1$:
        ld      (hl), a
        add     a, e
        inc     l
        jr      nz, 1$
        ret


;
; screen (a page) -> tilemap rows 1..16 at column 2.
; VRAM is writable in modes 0 and 1. After seeing one there are at least
; ~20 cycles (mode 2) before mode 3, enough for 2 bytes, so STAT is checked
; every 2 bytes. Row starts are 0x9822 + 32 * row; no row crosses a
; 256-byte boundary, so the column step is just inc e.
;
_copy_grid::
        ld      a, (_screen + 1)  ; screen is page-aligned: high byte only
        ld      h, a
        ld      l, #0
        ld      de, #0x9822
        ld      c, #16          ; rows
1$:
        ld      b, #8           ; pairs of bytes per row
2$:
        ldh     a, (_STAT_REG + 0)
        and     a, #2           ; mode 2 or 3: wait
        jr      nz, 2$
        ld      a, (hl+)
        ld      (de), a
        inc     e
        ld      a, (hl+)
        ld      (de), a
        inc     e
        dec     b
        jr      nz, 2$
        ld      a, e            ; skip the other 16 columns of the row
        add     a, #16
        ld      e, a
        jr      nc, 3$
        inc     d
3$:
        dec     c
        jr      nz, 1$
        ret
