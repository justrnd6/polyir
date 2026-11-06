#!/bin/bash
# Installs libpolyir into a temporary prefix and builds a separate project against it with find_package(polyir): the library has to work from outside the build
# tree (headers, CMake package, dylib install name), and what it compiles has to run.
#   tests/install_check.sh <build dir> <source dir>
set -e
BUILD="${1:?build dir}"; SRC="${2:?source dir}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cmake --install "$BUILD" --prefix "$TMP/prefix" >/dev/null
cmake -S "$SRC/tests/consumer" -B "$TMP/consumer" -G Ninja -DCMAKE_PREFIX_PATH="$TMP/prefix" >/dev/null
ninja -C "$TMP/consumer" >/dev/null
"$TMP/consumer/consumer" > "$TMP/program.ll"
clang -x ir -O1 -Wno-override-module -o "$TMP/program" "$TMP/program.ll"
set +e; "$TMP/program"; STATUS=$?; set -e
[ "$STATUS" = 42 ] || { echo "FAIL: the installed library compiled a program that exits $STATUS, not 42"; exit 1; }
echo "ok   the installed library compiles and runs a program (42)"
