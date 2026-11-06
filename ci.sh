#!/bin/bash
# The checks for polyir (core, runtime, targets: one CMake project), in the order a change should pass them. One line per stage; the exit
# status is non-zero if any stage failed.
#   ci.sh [build dir]     (default: a fresh temporary directory, removed afterwards)
# Stages: configure, build, ctest, negative (the recorded answers to tests/data/negative), style (the C++ style checker over the C++
# prints nothing). NINA_DIR (default: ../nina beside this checkout) is where that checker, ninastyle, is built.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE"                                          # the polyir checkout
NINA_DIR="${NINA_DIR:-$ROOT/../nina}"                  # the nina checkout, a sibling of polyir
TEMP_BUILD=""
if [ -n "$1" ]; then BUILD="$1"; mkdir -p "$BUILD"; else BUILD="$(mktemp -d)"; TEMP_BUILD="$BUILD"; fi
BUILD="$(cd "$BUILD" && pwd)"
LOG="$(mktemp -d)"; trap 'rm -rf "$LOG" "$TEMP_BUILD"' EXIT
failed=0

# stage <name> <command...>: runs the command, prints "ok" or "FAIL" with the name and the last lines of its output.
stage() {
  local name="$1"; shift
  if "$@" > "$LOG/$name.log" 2>&1; then echo "ok    $name"; else echo "FAIL  $name"; tail -n 12 "$LOG/$name.log" | sed 's/^/        /'; failed=1; fi
}

# The C++ of the repository.
CPP_SOURCES="translate core/include core/src core/tools core/examples/Gcd.cpp core/tests/*.cpp core/tests/*.hpp core/tests/consumer runtime targets/gpu targets/wasm targets/include targets/tests/*.cpp"
# The style checker prints findings but exits 0, so the stage is "prints nothing".
style_clean() { local out; out="$(cd "$ROOT" && "$NINA_DIR/ninastyle/build/ninastyle" $CPP_SOURCES 2>&1 | grep -E 'warning|error|findings')"; [ -z "$out" ] || { echo "$out"; return 1; }; }

stage configure cmake -S "$HERE" -B "$BUILD" -G Ninja
stage build ninja -C "$BUILD"
stage ctest ctest --test-dir "$BUILD" --output-on-failure
stage negative python3 "$HERE/core/tests/negative_cases.py" "$BUILD"
stage style style_clean
[ "$failed" -eq 0 ] && echo "ci: all stages passed" || echo "ci: FAILED"
exit "$failed"
