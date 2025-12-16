#!/bin/bash

OS_NAME=`uname -o 2>/dev/null || uname -s`

if [ $OS_NAME == "Msys" ]; then
    GLFLAG="-lopengl32"
elif [ $OS_NAME == "Darwin" ]; then
    GLFLAG="-framework OpenGL"
else
    GLFLAG="-lGL"
fi

SDL_CFLAGS=`sdl2-config --cflags 2>/dev/null | sed -E 's/-I([^[:space:]]+)\/SDL2/-I\\1/g'`
SDL_LDFLAGS=`sdl2-config --libs 2>/dev/null`

CFLAGS="-I../src -Wall -std=c11 -pedantic -DGL_GLEXT_PROTOTYPES $SDL_CFLAGS $GLFLAG -lm -O3 -g"
LDFLAGS="$SDL_LDFLAGS"

gcc main.c renderer.c ../src/microui.c $CFLAGS $LDFLAGS

