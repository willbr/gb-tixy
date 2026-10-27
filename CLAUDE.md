# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A clone of [tixy.land](https://tixy.land) for the Game Boy (currently a DMG ROM, no CGB flag/palettes), with an SDL preview build for desktop iteration.

## Build & run

`build.py` (macOS, stdlib only) is the build script. Needs `cc`, `sdl2-config` (Homebrew `sdl2`), GBDK-2020's `lcc` via `$GBDK_HOME/bin`, `PATH`, or `~/opt/gbdk/bin`, and SameBoy.app for `run`.

- `./build.py` / `./build.py gb` — regenerate `src/tables.h`, build `build/tixy.gb`.
- `./build.py sdl` — regenerate tables, build and run `build/tixy_sdl`.
- `./build.py tables` — compile and run `src/build_tables.c` > `src/tables.h` (every build does this; it's instant).
- `./build.py run` — build the ROM and open it in SameBoy.
- `./build.py watch [gb|sdl]` — poll `src/` and rebuild on change; `sdl` restarts the preview.

`build/` and `tables.h` are gitignored.

## Architecture

The same `src/main.c` compiles for two targets, switched by `-DSDL`:

- **Game Boy** (default): includes `<gb/gb.h>`.
- **SDL** (`-DSDL`): includes `src/shim_sdl.h`, which reimplements the GBDK calls used (`set_bkg_data`, `set_bkg_tiles`, `joypad`, `wait_vbl_done`, …) against `shim_memory[0x10000]` (tile data at `0x8800`, tilemap at `0x9800`) and draws the visible 160×144 at scroll (0,0). Keyboard maps to GBDK `J_*` bits: arrows, Space/Z = A, X = B.

### Formulas

Each visualisation is `FORMULA(name, expr)` in `main.c`, which expands to a function containing the whole 16×16 loop with `x`, `y`, `i` as locals and `t` global. Add new ones there and to `formulas[]`. A/Right = next, B/Left = previous (both targets). `t` increments once per frame.

### Tables

`src/build_tables.c` runs at build time and emits `src/tables.h`:

1. `dither_gfx[]` — 128 8×8 tiles; tile 0 = colour index 0 everywhere (lightest on DMG), tile 127 = darkest. Uses an 8×8 ordered-dither matrix.
2. `sin_table`, `cos_table`, `tan_table`, `atan_table`, `sqrt_table` — 256 `i8` entries, **stored in u8 order** (index k holds f((i8)k)), looked up as `table[(u8)i]` by `gb_sin` etc. in `shared.h`. Angles are in units of π/127 (127 ≈ π); `atan` returns an angle (±51). Values are rounded and clamped to i8.

Inside formulas use `gb_sin(t)` etc., not `<math.h>` — the GB has no FPU.

### Game Boy constraints

- Grid values are `i8`; assume 8-bit wraparound.
- `i8_abs(-128)` returns `127`.
- Avoid `%`, `/` and `*` on signed values in formulas where `&`/shifts work — SDCC calls software routines for them on the SM83.
