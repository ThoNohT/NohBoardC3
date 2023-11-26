#!/usr/bin/env sh

set -xe
if [ $1 = "build" ]; then
  # Build the project

  mkdir -p ./build
  touch ./build/NohBoard
  rm -r ./build/NohBoard

  CFLAGS="-Wall -Wextra -ggdb -I./raylib/src/"
  LIBS="-lm -ldl -lpthread -L./build/raylib/ -l:libraylib.a"

  clang $CFLAGS -o ./build/NohBoard src/main.c $LIBS 

elif [ $1 = "noh" ]; then
  # Build noh.c

  CFLAGS="-Wall -Wextra -ggdb"
  LIBS="-lm -ldl -lpthread"

  clang $CFLAGS -o ./noh ./noh.c $LIBS

elif [ $1 = "bld" ]; then
  # Build bld.c

  CFLAGS="-Wall -Wextra -ggdb"
  LIBS="-lm -ldl -lpthread"

  clang $CFLAGS -o ./bld ./bld.c $LIBS

elif [ $1 = "raylib" ]; then
  # Build Raylib

  mkdir -p ./build/raylib/
  rm -r ./build/raylib/
  mkdir -p ./build/raylib/

  for file in raudio rcore rglfw rmodels rshapes rtext rtextures utils
  do
    CFLAGS="-ggdb -DPLATFORM_DESKTOP -I./raylib/src/external/glfw/include"
    clang $CFLAGS -c ./raylib/src/$file.c -o build/raylib/$file.o
  done

  ar -crs build/raylib/libraylib.a build/raylib/raudio.o build/raylib/rcore.o build/raylib/rglfw.o build/raylib/rmodels.o build/raylib/rshapes.o build/raylib/rtext.o build/raylib/rtextures.o build/raylib/utils.o

elif [ $1 = "run" ]; then
  ./build/NohBoard

elif [ $1 = "clean" ]; then
  rm ./build/NohBoard

else
    echo "Invalid command."

fi

