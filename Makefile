BGB := bgb64.exe

tixy.gb: main.c shared.h
	lcc -o tixy.gb main.c

watch-c: tixy.gb
	watchexec -cr -f "*.c" mingw32-make

watch-gb: tixy.gb
	$(BGB) -watch ./tixy.gb

sdl: main.c shared.h
	tcc -DSDL `sdl2-config --cflags --libs` -o ./tixy_sdl ./main.c
	./tixy_sdl

watch-sdl:
	watchexe -cr -f "*.(c|h)" "make sdl"

.PHONY: watch-gb watch-c watch-sdl

