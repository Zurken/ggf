@echo off

mkdir bin

set flags=-std=c99 -g -O0 -Werror -D_DEBUG -DGGF_ENABLE_ASSERTIONS -DGGF_WINDOWS
set inc=-I./deps/glad/ -I./deps/stb/ -I./deps/cglm/include -I./deps/glfw/include
set lib=-L./deps/glfw/lib-mingw-w64 -lopengl32 -lglfw3 -luser32 -lwinmm -lgdi32
set src=./src/main.c

gcc %src% %flags% %inc% %lib% -o bin/ggf
