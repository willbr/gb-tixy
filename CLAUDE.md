# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A clone of [tixy.land](https://tixy.land) for the Game Boy. The ROM is CGB-compatible (`-Wm-yc`): on a CGB it loads `tixy_palette` (black, white, red) from `main.c` into background palette 0, on a DMG it sets `BGP_REG` to the matching greys, with an SDL preview build for desktop iteration.

## Build & run

`build.py` (macOS, stdlib only) is the build script. Needs `cc`, `sdl2-config` (Homebrew `sdl2`), GBDK-2020's `lcc` via `$GBDK_HOME/bin`, `PATH`, or `~/opt/gbdk/bin`, and SameBoy.app for `run`.

- `./build.py` / `./build.py gb` — regenerate `src/tables.h`, build `build/tixy.gb`.
- `./build.py sdl` — regenerate tables, build and run `build/tixy_sdl`.
- `./build.py tables` — compile and run `src/build_tables.c` > `src/tables.h` (every build does this; it's instant).
- `./build.py run` — build the ROM and open it in SameBoy.
- `./build.py watch [gb|sdl]` — poll `src/` and rebuild on change; `sdl` restarts the preview.
- `./build.py gif` — record `docs/tixy.gif` via `src/record.c` (headless SDL shim) piped to ffmpeg; formulas chosen by `GIF_FORMULAS` indices in `build.py`.

`build/` and `tables.h` are gitignored.

## Architecture

The same `src/main.c` compiles for two targets, switched by `-DSDL` (the GB build also links `src/rpn_gb.s`):

- **Game Boy** (default): includes `<gb/gb.h>`.
- **SDL** (`-DSDL`): includes `src/shim_sdl.h`, which reimplements the GBDK calls used (`set_bkg_data`, `set_bkg_tiles`, `joypad`, `wait_vbl_done`, `set_bkg_palette`, …) against `shim_memory[0x10000]` (tile data at `0x8800`, tilemap at `0x9800`), always acts as a CGB (palette 0 only, attribute writes ignored) counts `sys_time` per `wait_vbl_done`, and draws the visible 160×144 at scroll (0,0). Keyboard maps to GBDK `J_*` bits: arrows, Space/Z = A, X = B, Enter = Start, Backspace = Select.

### Formulas (RPN)

Formulas are RPN token programs, editable on the device. `gallery[]` in `main.c` holds them as strings (`"y 5 * t + sin"`), parsed into `progs[]` (RAM) at boot; the editor changes those copies. `src/rpn.h` defines every operator once in the `OPS(F)` X-macro list (name, text, stack effect, kind VAR/UNARY/BINARY/STACK, table or expression, asm routine); the enum, `op_names`, `op_in`/`op_out`, `op_kind`, `op_table`, the `s_`/`v_` functions and `binop_s`/`binop_v`/`binop_asm` are all generated from it, and row order is the editor's Up/Down order. To add an op: one row (plus a routine in `rpn_gb.s` if it should be fast on the GB). It also has `prog_parse`, `prog_ok` (static stack check; invalid programs show `err` and a blank grid), and `prog_eval`.

`prog_eval` runs each token once over all 256 cells. A stack slot is a scalar (`t`, literals, and anything computed only from them — evaluated once) or a pointer to 256 `i8`s; `x`/`y`/`i` point at ROM tables `cell_x`/`cell_y`/`cell_i`. Results go to a `pool` buffer that no live slot references (`free_buf`), so `dup`/`swap` can share buffers; scalars are broadcast into `bcast` only when mixed with arrays. Unary ops are all LUTs (`run_lut`). The final pass maps values through `value_to_tile`.

`t` is `(i8)sys_time`, so animation speed doesn't depend on how long evaluation takes. Only the low byte of `sys_time` is read (one byte, so no torn read against the VBL ISR). Buttons are read in a VBL handler (`read_pad`, added with `add_VBL`) that latches new presses into `pad_pressed`; `input()` takes them with interrupts off, and held directions repeat on frame time (`REPEAT_DELAY`/`REPEAT_RATE`), so neither depends on formula speed. The shim calls the handler from `wait_vbl_done` and latches key-downs. Screen: row 0 status (`draw_text`: formula, run/edit, `err`, frames per update), rows 1–16 the grid at column 2, row 17 the program scrolled to the cursor.

On the GB the 256-wide loops are SM83 asm in `src/rpn_gb.s` (sdas syntax, `.macro` for the shared entry/store and the binary-op template), declared in `rpn.h`; args are page numbers in the C globals `rpn_p0..p3`, set by the `run_*` wrappers. `copy_grid` is there too. `build.py gb` links `main.c` + `rpn_gb.s`; the SDL build doesn't use it. Every array is page-aligned: ROM tables via `TABLE_AT(addr)` (`__at` on GB, empty under SDL; pages 0x7500–0x7FFF, see `PAGE_*` in `build_tables.c`), RAM buffers from `arena` aligned in `rpn_init()`. The loops keep `n` in L and swap the array's page into H per access. Asm covers `+ - * & | ^ << >> < > = ?`, lookups (`asm_lut`), fill and the `v*k` table used for array×scalar multiply; `/ %` fall back to the C `v_` loops. `copy_grid` writes the screen page to VRAM checking STAT every 2 bytes (GBDK's `set_bkg_tiles` was ~1 frame). The C loops (SDL) are the reference: any asm change must still match them exactly. `build.py gb` fails if ROM code grows past `TABLES_START` (0x7500) or RAM variables past `RAM_LIMIT` (0xdc00, keeping 1 KB of stack). `OP_TEXT_MAX` (4) bounds token text and is checked per op at compile time; the editor's line buffer is sized from it.

On CGB `init()` calls `cpu_fast()` (double speed). `render()` doesn't wait for vblank before `copy_grid` (it checks STAT itself); it only waits when an update finished within the same frame, capping at 60 updates/s (in SDL that wait is the frame limiter).

Performance (PyBoy): 30–60 fps on CGB, 15–60 on DMG across the gallery. Comparison operators return 127/0.

`src/record.c` includes `main.c` and renders headless for `./build.py gif`.

### Tables

`src/build_tables.c` runs at build time and emits `src/tables.h`:

1. `circle_gfx[]` — 13 tiles: 0 empty, 1–6 positive sizes (colour 1), 7–12 negative (colour 2). `value_to_tile[256]` maps an `i8` value to its tile (radius ∝ |v|, snapped to round shapes, max 7px, 1px gap).
2. `font_gfx` / `font_hi_gfx` — 5×8 font for ASCII 32–127 in colour 1 / colour 2, uploaded to tiles 32–127 and 160–255, so text is ASCII and `| 0x80` highlights. Glyphs are ASCII-art strings in `glyphs[]`.
3. `cell_x`, `cell_y`, `cell_i` — per-cell coordinates for the interpreter.
4. `sin_table`, `cos_table`, `tan_table`, `atan_table`, `sqrt_table`, `neg_table`, `abs_table` — 256 `i8` entries **in u8 order** (index k holds f((i8)k)), looked up as `table[(u8)v]`. Angles are π/128 units (128 = π, full turn = 256). Outputs ×127, rounded and clamped (neg/abs of −128 give 127).

`tables.h` starts with the `TABLE_AT` definition; the page-aligned tables (cells, value_to_tile, the seven LUTs) are declared `TABLE_AT(0x7500..0x7f00)`.

### Game Boy constraints

- Grid values are `i8`; assume 8-bit wraparound.
- A formula's value sets circle size: ±127 is full size, so small-range expressions (e.g. `y x -`) draw small dots — scale them up (`3 <<`) to taste.
- `* / %` call SDCC software routines per element; prefer `&`/shifts.
- To measure on real (emulated) hardware, PyBoy can run the ROM headless; `build.py gb` writes `build/tixy.noi` with symbol addresses (e.g. `_loop_frames`, `_render` for hooks).
