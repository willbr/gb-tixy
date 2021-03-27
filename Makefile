TARGET := tixy.gb
TARGET_SDL := tixy_sdl

BGB := bgb64.exe
MAKE := mingw32-make

BUILD_DIR := ./build
SRC_DIR := ./src

FILTER_PULSE := grep --invert-match "tcc.*pa_.*" | grep --invert-match ".*libpulse.*"
FILTER_DBUS := grep --invert-match "tcc.*dbus_.*"
FILTER_LIBGL := grep --invert-match "LIBGL:.*"
FILTER := $(FILTER_PULSE) | $(FILTER_DBUS)

gb: $(SRC_DIR)/main.c $(SRC_DIR)/shared.h tables.h
	mkdir -p $(BUILD_DIR)
	lcc -o $(BUILD_DIR)/$(TARGET) $(SRC_DIR)/main.c

watch-c: 
	watchexec -cr -f "*.c" "$(MAKE)"

watch-gb:
	$(BGB) -watch $(BUILD_DIR)/$(TARGET)

sdl: $(SRC_DIR)/main.c $(SRC_DIR)/shared.h tables.h
	mkdir -p $(BUILD_DIR)
	tcc -DSDL `sdl2-config --cflags --libs` -o $(BUILD_DIR)/$(TARGET_SDL) $(SRC_DIR)/main.c 2>&1 | $(FILTER)
	$(BUILD_DIR)/$(TARGET_SDL) 2>&1 | $(FILTER_LIBGL)

watch-sdl:
	watchexec -cr -f "$(SRC_DIR)/*.c"  -f "$(SRC_DIR)/*.h" "make sdl"

tables.h: $(SRC_DIR)/build_tables.c
	tcc -run $(SRC_DIR)/build_tables.c > $(SRC_DIR)/tables.h

.PHONY: watch-gb watch-c watch-sdl

