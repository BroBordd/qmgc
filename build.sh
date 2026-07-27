#!/bin/sh
# Quick build without CMake. For a proper out-of-tree build, use CMake instead:
#   cmake -B build && cmake --build build
set -e
ROOT=$(cd "$(dirname "$0")" && pwd)
CXX=${CXX:-g++}
JPEG_FLAGS=""
JPEG_LIBS=""
if pkg-config --exists libjpeg 2>/dev/null; then
    JPEG_FLAGS="-DQMGC_HAVE_JPEG $(pkg-config --cflags libjpeg)"
    JPEG_LIBS="$(pkg-config --libs libjpeg)"
fi
$CXX -O2 -Wall -std=c++17 -I"$ROOT/include" -I"$ROOT/src" $JPEG_FLAGS \
    -o qmgc \
    "$ROOT/src/residual.cpp" \
    "$ROOT/src/image.cpp" \
    "$ROOT/src/encode_w2.cpp" \
    "$ROOT/src/encode_a9ll.cpp" \
    "$ROOT/src/encode.cpp" \
    "$ROOT/tools/qmgc.cpp" \
    -lpng $JPEG_LIBS -pthread
echo "built ./qmgc"
