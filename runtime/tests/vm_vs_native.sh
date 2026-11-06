#!/bin/bash
# The C++ bytecode VM against the LLVM build, on the real programs: every .poly of core/examples and core/tests/programs (and of
# ideas/audio/examples and ideas/*/tests, the libraries under review; the other ideas/ examples need a GPU or an audio device) runs
# through `polyir-check FILE --interp` and as a native executable (`polyir-check FILE --out EXE`, what `--run` builds, then run), and the
# standard output and the exit status must agree.
#   tests/vm_vs_native.sh <build dir> [files...]      (default: every .poly in those folders)
#
# What runs: a program's `main` as it is (the VM prints the entry's value on a last line of its own, which is dropped before comparing).
# A native build can only start at `main`, so every other entry (the `checkSomething()` functions of core/tests/programs, the argument-less
# functions of a library, the invocations a header documents as `//   polyir run FILE --entry NAME --args "WORDS"`) runs through a small
# wrapper program that imports the file and has a `main` calling the entry with the words as literal arguments and printing its value;
# the VM and the native build both run that wrapper. A float value is printed as a whole number of millionths (polyir has no float text).
# A row is OK, DIFF (the first differing line, or the two exit statuses), DECL (the VM declined a program the native build runs), or SKIP
# (no native build: the LLVM route refuses the program, or the wrapper does not compile).
# Each program runs in its own folder, where it finds its fixtures (ideas/audio/tests/WavTests.poly reads fixtures/*.wav).
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$HERE/../.." && pwd)"         # the repository
CORE_DIR="$ROOT_DIR/core"                     # the programs: core/examples and core/tests/programs, with core/stdlib
BUILD="${1:?usage: vm_vs_native.sh <build dir> [files...]}"; shift
CHECK="$(cd "$BUILD" && pwd)/polyir-check"
export LC_ALL=C  # the programs print raw bytes (examples/strings.poly): diff and grep must not reject them as bad UTF-8
[ -x "$CHECK" ] || { echo "FAIL $CHECK is not built"; exit 1; }
FILES=()
for f in "$@"; do FILES+=("$(cd "$(dirname "$f")" && pwd)/$(basename "$f")"); done  # absolute, so the mirror finds its copy
[ ${#FILES[@]} -eq 0 ] && FILES=($(find "$CORE_DIR/examples" "$CORE_DIR/tests/programs" "$ROOT_DIR/ideas/audio/examples" "$ROOT_DIR"/ideas/*/tests -maxdepth 1 -name '*.poly' | sort))
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
ok=0; differ=0; declined=0; skip=0
LIMIT=120  # seconds a run may take before it counts as hung

# A command cut off after LIMIT seconds (perl's alarm: no coreutils `timeout` needed).
limited() { perl -e 'alarm shift; exec @ARGV' "$LIMIT" "$@"; }

# The entries of a program: its `main`, else every `checkSomething()` test function, else (a library of functions) every
# function that takes no arguments, so the examples without a `main` are still exercised.
entries() {
  if grep -qE '^func main\(' "$1"; then echo main
  elif grep -qE '^func check[A-Za-z0-9_]*\(\)' "$1"; then grep -oE '^func check[A-Za-z0-9_]*\(\)' "$1" | sed -E 's/^func //; s/\(\)$//'
  else grep -oE '^func [A-Za-z0-9_]+\(\)' "$1" | sed -E 's/^func //; s/\(\)$//'; fi
}

# The invocations a program's header documents, one per line as ENTRY|WORDS (the words empty when the line passes none).
documented() {
  grep -oE '^// +polyir run [^ ]+ --entry [A-Za-z0-9_]+( +--args "[^"]*")?' "$1" |
    sed -E 's/^.*--entry ([A-Za-z0-9_]+)( +--args "([^"]*)")?$/\1|\3/'
}

# A documented invocation without words of an entry the discovery already runs would be the same row twice.
alreadyRun() { [ -z "$2" ] && printf '%s\n' $found | grep -qx "$1"; }

# ── the wrapper for an entry other than main ──

# The wrapper has the only `main`, so it imports a copy of the program in which every `main` (the program's own and those of the files
# it imports, e.g. core/tests/programs/InterfaceTests.poly imports examples/interfaces.poly) is renamed; the copies keep their relative layout.
MIRROR="$TMP/mirror"
mirrorWithoutMains() {
  (cd "$ROOT_DIR" && find core/examples core/tests/programs core/stdlib ideas -name '*.poly') | while read -r rel; do
    mkdir -p "$MIRROR/$(dirname "$rel")"
    sed -E 's/^func main\(/func importedMain(/' "$ROOT_DIR/$rel" > "$MIRROR/$rel"
  done
}
mirrorWithoutMains
mirrored() { echo "$MIRROR/${1#$ROOT_DIR/}"; }

# The entry's declaration line: `func NAME(PARAMS) -> TYPE {…`.
signature() { grep -m1 -E "^func $2\(" "$1"; }

# The result type ("" for a function that returns nothing).
resultType() { signature "$1" "$2" | sed -nE 's/^func [A-Za-z0-9_]+\([^)]*\) *-> *([A-Za-z0-9_<>]+).*/\1/p'; }

# The parameter types, one per line.
parameterTypes() { signature "$1" "$2" | sed -E 's/^func [A-Za-z0-9_]+\(([^)]*)\).*/\1/' | tr ',' '\n' | sed -nE 's/.*: *([A-Za-z0-9_<>]+).*/\1/p'; }

# A word as a literal of its parameter's type: a float literal needs its point.
literal() {
  case "$2" in
    double|float) [[ "$1" == *.* ]] && echo "$1" || echo "$1.0" ;;
    *) echo "$1" ;;
  esac
}

# `E(w1, w2, …)` with each word written as its parameter's type.
callText() {
  local f="$1" entry="$2"; shift 2
  local types=($(parameterTypes "$f" "$entry")) args="" i=0
  for word in "$@"; do args+="${args:+, }$(literal "$word" "${types[$i]}")"; i=$((i+1)); done
  [ "$entry" = main ] && entry=importedMain  # the mirror's name for it
  echo "$entry($args)"
}

# How main shows the value: whole numbers and bools as text, a float as millionths, anything else (a list, nothing) not at all.
printedValue() {
  case "$1" in
    i1|i8|i16|i32|i64|u8|u16|u32|u64|bool) echo "    print(str(value))" ;;
    double|float) echo "    print(str((value.toDouble() * 1000000.0).toI64()))" ;;
    *) echo "" ;;
  esac
}

writeWrapper() {
  local f="$1" entry="$2"; shift 2
  local type; type="$(resultType "$f" "$entry")"
  {
    echo "import \"$(mirrored "$f")\""
    echo "func main() -> i64 {"
    if [ -n "$type" ]; then echo "    let value = $(callText "$f" "$entry" "$@")"; printedValue "$type"; else echo "    $(callText "$f" "$entry" "$@")"; fi
    echo "    return 0"
    echo "}"
  } > "$TMP/wrapper.poly"
}

# ── running both sides ──

# The VM's run of PROGRAM: what it printed without the value line, its status, and its error stream. The value (a number, a bool or `void`)
# and its newline come last; when the program's own output did not end with a newline the value follows it on the same line, so only the
# value is taken off, not the line (examples/io.poly ends with the byte it read).
runVm() {
  limited "$CHECK" "$1" --interp < /dev/null > "$TMP/vm.raw" 2> "$TMP/vm.err"; vm=$?
  if [ -s "$TMP/vm.err" ]; then cp "$TMP/vm.raw" "$TMP/vm.out"
  else perl -0pe 's/(-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?|true|false|void|-?nan|-?inf)\n\z//' "$TMP/vm.raw" > "$TMP/vm.out"; fi
}

# The native build of PROGRAM, then its run; `native` stays empty when it does not build.
runNative() {
  native=""
  "$CHECK" "$1" --out "$TMP/native.exe" > "$TMP/build.out" 2>&1 || return 1
  limited "$TMP/native.exe" < /dev/null > "$TMP/native.out" 2> "$TMP/native.err"; native=$?
}

# A program with a main runs as it is; any other entry through the wrapper.
programFor() {
  local f="$1" entry="$2"; shift 2
  if [ "$entry" = main ] && [ $# -eq 0 ]; then echo "$f"; return; fi
  writeWrapper "$f" "$entry" "$@"
  echo "$TMP/wrapper.poly"
}

row() { printf '%-4s %-56s %s\n' "$1" "$2" "$3"; }

firstDifference() {
  if [ "$vm" != "$native" ]; then echo "VM exit $vm ($(head -1 "$TMP/vm.err" | cut -c1-70)), native exit $native"
  else diff "$TMP/vm.out" "$TMP/native.out" | grep -m1 '^[<>]' | cut -c1-110; fi
}

# A trap on the VM (status 1, `trap:` on its error stream) matches a native run that also failed after the same output.
sameFailure() { [ "$vm" = 1 ] && grep -q '^trap: ' "$TMP/vm.err" && [ "$native" != 0 ] && cmp -s "$TMP/vm.out" "$TMP/native.out"; }

compare() {
  if [ -z "$native" ]; then skip=$((skip+1)); row SKIP "$1" "no native build: $(grep -m1 -v ':$' "$TMP/build.out" | sed 's/^ *//' | cut -c1-80)"
  elif [ "$vm" = 125 ] && grep -q '^declined: ' "$TMP/vm.err"; then declined=$((declined+1)); row DECL "$1" "$(head -1 "$TMP/vm.err" | cut -c1-90)"
  elif [ "$vm" = "$native" ] && cmp -s "$TMP/vm.out" "$TMP/native.out"; then ok=$((ok+1)); row OK "$1" "exit $vm"
  elif sameFailure; then ok=$((ok+1)); row OK "$1" "both failed: $(head -1 "$TMP/vm.err" | cut -c1-80)"
  else differ=$((differ+1)); row DIFF "$1" "$(firstDifference)"; fi
}

runRow() {
  local f="$1" label="$2"; shift 2
  local program; program="$(programFor "$f" "$@")"
  runNative "$program"
  [ -n "$native" ] && runVm "$program"
  compare "$label"
}

for f in "${FILES[@]}"; do
  cd "$(dirname "$f")" || exit 1
  rel="${f#$CORE_DIR/}"; rel="${rel#$ROOT_DIR/}"
  found="$(entries "$f")"
  shown="$(documented "$f")"
  [ -n "$found$shown" ] || { skip=$((skip+1)); row SKIP "$rel" "no main, no check function, no argument-less function and no documented run: nothing to run"; }
  for entry in $found; do runRow "$f" "$rel:$entry" "$entry"; done
  while IFS='|' read -r entry words; do
    [ -n "$entry" ] || continue
    alreadyRun "$entry" "$words" && continue
    runRow "$f" "$rel:$entry($words)" "$entry" $words
  done <<< "$shown"
done
echo "$ok ok, $differ differ, $declined declined, $skip skipped"
[ "$differ" -eq 0 ] && [ "$declined" -eq 0 ]
