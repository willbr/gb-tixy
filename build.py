#!/usr/bin/env python3
"""
usage: ./build.py [gb|sdl|tables|run|watch [gb|sdl]]

  gb      build build/tixy.gb (default)
  sdl     build and run the SDL preview
  tables  regenerate src/tables.h
  run     build the ROM and open it in SameBoy
  watch   rebuild on src/ changes; 'watch sdl' restarts the preview

lcc is found via $GBDK_HOME/bin/lcc, else PATH, else ~/opt/gbdk/bin/lcc.
"""

import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, "src")
BUILD = os.path.join(ROOT, "build")
ROM = os.path.join(BUILD, "tixy.gb")
SDL_BIN = os.path.join(BUILD, "tixy_sdl")


def run(cmd, **kw):
    print(" ".join(cmd))
    return subprocess.run(cmd, check=True, **kw)


def tables():
    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "build_tables")
    run(["cc", "-O1", "-o", exe, os.path.join(SRC, "build_tables.c"), "-lm"])
    with open(os.path.join(SRC, "tables.h"), "w") as f:
        run([exe], stdout=f)


def lcc():
    home = os.environ.get("GBDK_HOME")
    if home:
        return os.path.join(home, "bin", "lcc")
    path = shutil.which("lcc") or os.path.expanduser("~/opt/gbdk/bin/lcc")
    if not os.path.exists(path):
        sys.exit("lcc not found: install GBDK-2020 and set GBDK_HOME or add its bin/ to PATH")
    return path


def gb():
    tables()
    run([lcc(), "-o", ROM, os.path.join(SRC, "main.c")])


def sdl_build():
    tables()
    cflags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
    libs = subprocess.check_output(["sdl2-config", "--libs"], text=True).split()
    run(["cc", "-std=c99", "-O1", "-DSDL", *cflags,
         "-o", SDL_BIN, os.path.join(SRC, "main.c"), *libs])


def sdl():
    sdl_build()
    run([SDL_BIN])


def open_rom():
    gb()
    run(["open", "-a", "SameBoy", ROM])


def src_mtime():
    return max(os.path.getmtime(os.path.join(SRC, f))
               for f in os.listdir(SRC)
               if f.endswith((".c", ".h")) and f != "tables.h")


def watch(target):
    proc = None
    last = 0
    while True:
        m = src_mtime()
        if m != last:
            last = m
            if proc:
                proc.terminate()
                proc.wait()
                proc = None
            try:
                if target == "sdl":
                    sdl_build()
                    proc = subprocess.Popen([SDL_BIN])
                else:
                    gb()
            except subprocess.CalledProcessError:
                print("build failed")
        time.sleep(0.25)


def main():
    args = sys.argv[1:] or ["gb"]
    cmd = args[0]
    try:
        if cmd == "gb":
            gb()
        elif cmd == "sdl":
            sdl()
        elif cmd == "tables":
            tables()
        elif cmd == "run":
            open_rom()
        elif cmd == "watch":
            watch(args[1] if len(args) > 1 else "gb")
        else:
            sys.exit(__doc__)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
