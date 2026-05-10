@echo off
setlocal

set TARGET=tixy.gb
set TARGET_SDL=tixy_sdl.exe
set BGB=bgb64.exe
set BUILD_DIR=build
set SRC_DIR=src

if "%1"=="" goto gb
if "%1"=="gb" goto gb
if "%1"=="sdl" goto sdl
if "%1"=="tables" goto tables
if "%1"=="watch-c" goto watch-c
if "%1"=="watch-gb" goto watch-gb
if "%1"=="watch-sdl" goto watch-sdl

echo Unknown target: %1
exit /b 1

:gb
call :tables
mkdir %BUILD_DIR% 2>nul
lcc -o %BUILD_DIR%\%TARGET% %SRC_DIR%\main.c
goto :eof

:sdl
call :tables
mkdir %BUILD_DIR% 2>nul
tcc -DSDL -lSDL2 -o %BUILD_DIR%\%TARGET_SDL% %SRC_DIR%\main.c
%BUILD_DIR%\%TARGET_SDL%
goto :eof

:tables
if not exist %SRC_DIR%\tables.h (
    tcc -run %SRC_DIR%\build_tables.c > %SRC_DIR%\tables.h
)
goto :eof

:watch-c
watchexec -cr -f "*.c" "build.cmd"
goto :eof

:watch-gb
%BGB% -watch %BUILD_DIR%\%TARGET%
goto :eof

:watch-sdl
watchexec -cr -f "%SRC_DIR%\*.c" -f "%SRC_DIR%\*.h" "build.cmd sdl"
goto :eof
