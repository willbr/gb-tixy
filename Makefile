BGB := bgb64.exe
MAKE := mingw32-make

tixy.gb: main.c shared.h tables.h
	lcc -o tixy.gb main.c

watch-c: 
	watchexec -cr -f "*.c" "$(MAKE)"

watch-gb:
	$(BGB) -watch ./tixy.gb

sdl: main.c shared.h
	tcc -DSDL `sdl2-config --cflags --libs` -o ./tixy_sdl ./main.c
	./tixy_sdl

watch-sdl:
	watchexec -cr -f "*.c"  -f "*.h" "make sdl"

tables.h: build_tables.c
	tcc -run build_tables.c > tables.h

.PHONY: watch-gb watch-c watch-sdl

