#!/bin/bash

mkdir -p bin

flags=(
  -std=c99 -g -O0 -Werror -D_DEBUG -DGGF_ENABLE_ASSERTIONS
  -DGGF_OSX
)

# Include directories
inc=(
  -I./deps/glad/
  -I./deps/stb/
  -I/opt/homebrew/include/
)

# Library directories
lib=(
-L/opt/homebrew/lib	
-lglfw
)

# Source files
src=(
  ./src/blackjack.c
)

fworks=(
  -framework OpenGL
)

# Build
gcc ${flags[*]} ${fworks[*]} ${inc[*]} ${lib[*]}  ${src[*]} -o ./bin/ggf.out

cd ..
