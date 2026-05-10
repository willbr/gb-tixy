# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A clone of [tixy.land](https://tixy.land) for the Game Boy Color, with an SDL preview build for desktop iteration.

## Build & run

The Makefile assumes Windows + `mingw32-make` (aliased as `MAKE`). It expects `lcc` (GBDK), `tcc`, `bgb64.exe` (BGB emulator), `watchexec`, and `sdl2-config` on `PATH`.

- `make gb` — build `build/tixy.gb` (depends on `tables.h`).
- `make sdl` — compile and immediately run the SDL preview (`build/tixy_sdl`); stderr is filtered to drop pulse/dbus/libGL noise.
- `make tables.h` — regenerate `src/tables.h` by `tcc -run`-ing `src/build_tables.c` and redirecting stdout.
- `make watch-gb` — open the ROM in BGB with `-watch` (auto-reload on rebuild).
- `make watch-c` / `make watch-sdl` — watchexec loops that re-run `make` / `make sdl` on `.c`/`.h` changes.

`make` invokes `tcc -run ./src/win_mkdir.c` for the `mkdir` step. That file is in-progress (see commit `f061751`) and not yet committed — if `gb`/`sdl` fails on the mkdir line, create `build/` manually or substitute `mkdir -p` (the line is commented in the Makefile).

`tables.h`, `build/tixy.gb`, and `build/tixy_sdl` are gitignored. `tables.h` is required for both targets — generate it before building.

## Architecture

The same `src/main.c` compiles for two targets, switched by `-DSDL`:

- **Game Boy** (default): includes `<gb/gb.h>` and friends, uses GBDK's `set_bkg_data` / `set_bkg_tiles` / `wait_vbl_done` directly.
- **SDL** (`-DSDL`): pulls in `src/shim_sdl.h`, which reimplements those GBDK entry points against an in-memory VRAM image (`shim_memory[0x10000]`) and an SDL surface. The shim mirrors the GB memory map (tile data at `0x8800`, tilemap at `0x9800`) so the GB and SDL paths share identical drawing logic.

Note: `struct gb_row` swaps `low`/`high` byte order between the two targets in `shared.h` — keep that `#ifdef SDL` intact when touching tile structs.

### The render loop

`main()` calls `init()` once and then loops `update()` → `render()` (plus `shim_update`/`shim_render` under SDL).

- `init()` uploads the 128-tile `dither_gfx` palette as background tile data and fills the entire 32×32 tilemap with tile `0x7f`.
- `update()` walks a 16×16 grid. For each cell it evaluates `TIXY_CMD` (a macro at the top of `main.c`) with `t`, `i`, `x`, `y` in scope, takes `i8_abs` of the result, and stores it as a tile index in `screen[]`. `t` increments per frame; `i` increments per cell.
- `render()` waits for vblank then blits `screen` into the visible region of the tilemap at offset `(2,1)`.

To change the visualisation, edit the `#define TIXY_CMD ...` line — many one-liners are kept commented above it as a gallery.

### Dithering & tables

`src/build_tables.c` is run at build time (via `tcc -run`) to produce `src/tables.h`. It emits five things:

1. `dither_gfx[]` — 128 pre-rendered 8×8 GB tiles, one per "brightness" level 0..127. `closest_colour()` in `shared.h` uses an 8×8 ordered-dither matrix (`dither_42`) and 4 GB greyscale levels to choose each pixel's colour.
2. `sin_table` / `cos_table` / `tan_table` / `atan_table` — 256-entry `i8` lookup tables, mapping `i8` input through `i * (π/127)` then scaling the result back to `i8`. Wrapped by `gb_sin` / `gb_cos` / `gb_tan` / `gb_atan` in `shared.h` (define `INCLUDE_TABLES` to pull these in — `main.c` already does).
3. `sqrt_table` — `i8` sqrt; wrapped by `gb_sqrt`.

Inside `TIXY_CMD`, prefer `gb_sin(t)` etc. over `<math.h>` — the GB target has no FPU and these are the only math primitives available there.

`src/tables.py` is an unused alternative Python generator (only sin + sqrt) and is not wired into the build.

### Game Boy constraints worth knowing

- All grid values are `i8` (`signed char`); arithmetic in `TIXY_CMD` should assume 8-bit wraparound semantics.
- `i8_abs(-128)` returns `127` (not `128`) to stay in range — used on every cell.
- Tile `0` = brightest, tile `127` = darkest in `dither_gfx`. Tile `0x7f` (127) is used as the "empty" fill colour.
