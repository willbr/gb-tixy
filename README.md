# gb-tixy

A clone of [tixy.land](https://tixy.land) for the Game Boy and Game Boy Color.

![four tixy formulas, rendered by the SDL preview](docs/tixy.gif)

A 16×16 grid of circles, each driven by one expression of `t`, `i`, `x`, `y`.
Positive values draw white circles, negative values draw red ones, and the
size follows the magnitude. See [tixy.md](tixy.md) for the original.

## Controls

| Button       | Action           |
|--------------|------------------|
| A / Right    | next formula     |
| B / Left     | previous formula |

In the SDL preview: arrows, Space/Z = A, X = B, Esc quits.

## Building (macOS)

Needs [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020/releases) (found via
`$GBDK_HOME`, `PATH`, or `~/opt/gbdk`), Homebrew `sdl2`, and optionally
[SameBoy](https://sameboy.github.io) and `ffmpeg`.

    ./build.py          # build/tixy.gb
    ./build.py run      # build and open in SameBoy
    ./build.py sdl      # desktop preview
    ./build.py watch    # rebuild on change (watch sdl restarts the preview)
    ./build.py gif      # re-record docs/tixy.gif

The ROM is CGB-compatible: colour on a Game Boy Color, greys on the original.

## Writing formulas

Formulas live in `src/main.c`:

    FORMULA(f_sin_y8, gb_sin((y << 2) + y + t))

Everything is 8-bit: values are `i8` (−128..127, wrapping), ±127 is a full
size circle, and `t` counts frames. There's no FPU, so trig comes from lookup
tables (`gb_sin`, `gb_cos`, `gb_tan`, `gb_atan`, `gb_sqrt`) where 127 ≈ π.
To port a tixy expression, convert radians to those units, e.g.
`Math.sin(y/8+t)` → `gb_sin(y*5 + t)`.

Add the function to `formulas[]` to put it in the gallery.

## How it works

`src/build_tables.c` runs at build time and generates the trig tables plus
256 circle tiles, one per `i8` value, so each frame a formula's result is
written straight into the background tilemap. The same `main.c` builds for the
Game Boy with GBDK or for the desktop with `-DSDL`, through a small shim
(`src/shim_sdl.h`) that emulates the few GBDK calls used.
