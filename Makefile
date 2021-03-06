BGB := bgb64.exe

tixy.gb: main.c
	lcc -o tixy.gb main.c

watch-c: tixy.gb
	watchexec -cr -f "*.c" mingw32-make

watch-gb: tixy.gb
	$(BGB) -watch ./tixy.gb

.PHONY: watch

