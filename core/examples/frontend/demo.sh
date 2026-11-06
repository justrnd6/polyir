#!/bin/bash
# End to end: a program in "tiny" (tiny.tl) -> polyir text + source map (tinyc.py) -> native program with DWARF (polyir --debug),
# then a debugger stops in the tiny source. Needs python3, lldb (macOS/Linux), and the built polyir CLI.
#   examples/frontend/demo.sh            run it and check every step
#   examples/frontend/demo.sh --show     the same, printing the generated files and the debugger session
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
POLYIR="${POLYIR:-$HERE/../../../lang/cli/build/out/polyir-lang}"
OUT="$HERE/build"; mkdir -p "$OUT"; cd "$HERE"
show() { [ "${1:-}" = "--show" ]; }
fail() { echo "FAIL: $*"; exit 1; }

echo "1. front end: tiny.tl -> build/tiny.poly + build/tiny.map"
python3 tinyc.py tiny.tl --poly "$OUT/tiny.poly" --map "$OUT/tiny.map"
if show "$@"; then echo "--- tiny.poly"; cat "$OUT/tiny.poly"; echo "--- tiny.map (first lines)"; head -12 "$OUT/tiny.map"; fi

echo "2. polyir: native program with debug information, positions mapped to tiny.tl"
"$POLYIR" "$OUT/tiny.poly" --debug --source-map "$OUT/tiny.map" --out "$OUT/tiny" >/dev/null
GOT="$("$OUT/tiny" | tr '\n' ' ')"
[ "$GOT" = "30 385 " ] || fail "program printed '$GOT', expected '30 385 '"
echo "   runs: $GOT"

echo "3. the LLVM IR names tiny.tl, not tiny.poly"
"$POLYIR" "$OUT/tiny.poly" --debug --source-map "$OUT/tiny.map" --emit-llvm > "$OUT/tiny.ll"
grep -q 'DISubprogram(name: "sumsq".*line: 6' "$OUT/tiny.ll" || fail "sumsq is not at tiny.tl line 6"
grep -q 'DIFile(filename: "tiny.tl"' "$OUT/tiny.ll" || fail "no DIFile for tiny.tl"
echo "   sumsq is at tiny.tl:6"

echo "4. the description as JSON (any target)"
"$POLYIR" "$OUT/tiny.poly" --debug-json "$OUT/tiny.json" --source-map "$OUT/tiny.map" --check >/dev/null
python3 - "$OUT/tiny.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
rows = {f["name"]: (f["source"], f["line"]) for f in d["functions"]}
assert rows["square"] == ("tiny.tl", 2) and rows["sumsq"] == ("tiny.tl", 6), rows
print("   functions:", {k: "%s:%d" % v for k, v in rows.items()})
PY

echo "5. a debugger stops in the tiny source and prints its variables"
if command -v lldb >/dev/null; then
  # The debuggee is built in a temp directory: on this macOS, lldb hangs launching a binary that sits under ~/Documents. stdin from
  # /dev/null and a timeout keep a stuck session from hanging the demo.
  TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
  "$POLYIR" "$OUT/tiny.poly" --debug --source-map "$OUT/tiny.map" --out "$TMP/tiny" >/dev/null
  SESSION="$(cd "$TMP" && timeout 90 lldb -b -o "breakpoint set -f tiny.tl -l 3" -o "run" -o "bt" -o "frame variable" -o "frame select 1" -o "frame variable" -o "quit" -- ./tiny </dev/null 2>&1)" || true
  if show "$@"; then echo "$SESSION"; fi
  echo "$SESSION" | grep -q "square.* at tiny.tl:3" || fail "breakpoint did not stop at tiny.tl:3"
  echo "$SESSION" | grep -q "sumsq.* at tiny.tl:10" || fail "caller sumsq is not at tiny.tl:10"
  echo "$SESSION" | grep -q "main.* at tiny.tl:16" || fail "main is not at tiny.tl:16"
  echo "$SESSION" | grep -q "(long) x = " || fail "parameter x of square is not printed"
  echo "$SESSION" | grep -q "(long) total = " || fail "local total of sumsq is not printed"
  echo "   stopped in square at tiny.tl:3 (x printed), called from sumsq at tiny.tl:10 (n, total, i printed), from main at tiny.tl:16"
else
  echo "   (lldb not found: skipped)"
fi

echo "5b. structs, strings and arrays print too"
if command -v lldb >/dev/null; then
  "$POLYIR" shapes.poly --debug --out "$TMP/shapes" >/dev/null
  SESSION="$(cd "$TMP" && cp "$HERE/shapes.poly" . && timeout 90 lldb -b -o "breakpoint set -f shapes.poly -l 20" -o "run" -o "frame variable" -o "p *xs" -o "p xs->data[0]" -o "p c->name" -o "quit" -- ./shapes </dev/null 2>&1)" || true
  if show "$@"; then echo "$SESSION"; fi
  echo "$SESSION" | grep -q "b = (x = 4, y = 6)" || fail "struct s (nested Pt) is not printed"
  echo "$SESSION" | grep -q '(char \*) word = .* "hello"' || fail "string word is not printed"
  echo "$SESSION" | grep -q "(Counter \*) c = " || fail "reference struct c is not printed"
  echo "$SESSION" | grep -q "length = 2" || fail "array xs is not printed"
  echo "$SESSION" | grep -q "(long\[3\]) cells" || fail "fixed array cells is not printed"
  echo "   printed: a nested value struct, a string, a reference struct, an array header (and its elements), a fixed array"
fi

echo "5c. the same program as Java, debugged in the tiny source (needs a JDK)"
if command -v javac >/dev/null && command -v java >/dev/null; then
  JAVA_DIR="$(mktemp -d)"
  "$POLYIR" "$OUT/tiny.poly" --emit-java --source-map "$OUT/tiny.map" --smap "$JAVA_DIR/Tiny.smap" > "$JAVA_DIR/Tiny.java"
  javac -d "$JAVA_DIR/out" "$JAVA_DIR/Tiny.java"
  java "$HERE/../../../lang/tools/AttachSmap.java" "$JAVA_DIR/Tiny.smap" "$JAVA_DIR/out" >/dev/null
  GOT="$(java -cp "$JAVA_DIR/out" Tiny | tr '\n' ' ')"
  [ "$GOT" = "30 385 " ] || fail "the Java program printed '$GOT', expected '30 385 '"
  SESSION="$(timeout 90 java --add-modules jdk.jdi "$HERE/../java-mapping/DebugCheck.java" "$JAVA_DIR/out" Tiny tl tiny.tl 3 2>&1)" || true
  if show "$@"; then echo "$SESSION"; fi
  echo "$SESSION" | grep -q "stopped at tiny.tl:3" || fail "the Java program did not stop at tiny.tl:3"
  echo "$SESSION" | grep -q "sumsq (tiny.tl:10)" || fail "the caller sumsq is not at tiny.tl:10 in the Java stack"
  echo "   Java stopped at tiny.tl:3, called from sumsq at tiny.tl:10"
  rm -rf "$JAVA_DIR"
else
  echo "   (no JDK found: skipped)"
fi

echo "6. the same program as wasm, with the same line table"
BIN="${POLYIR_LLVM_BIN:-}"
if [ -n "$BIN" ] && [ -x "$BIN/llvm-dwarfdump" ] && command -v wasmtime >/dev/null; then
  POLYIR_LLVM_BIN="$BIN" "$POLYIR" "$OUT/tiny.poly" --debug --source-map "$OUT/tiny.map" --emit-wasm --out "$OUT/tiny.wasm" >/dev/null
  GOT="$(wasmtime run --preload env=host-stub.wat --invoke sumsq "$OUT/tiny.wasm" 10 2>&1 | tail -1)"
  [ "$GOT" = "385" ] || fail "wasm sumsq(10) gave '$GOT', expected 385"
  "$BIN/llvm-dwarfdump" --debug-line "$OUT/tiny.wasm" | grep -q "tiny.tl" || fail "no tiny.tl in the wasm line table"
  python3 - "$OUT/tiny.wasm" <<'PY'
import json, sys
d = open(sys.argv[1], "rb").read()
def leb(b, i):
    r = s = 0
    while True:
        x = b[i]; i += 1; r |= (x & 0x7f) << s; s += 7
        if not x & 0x80: return r, i
i = 8
while i < len(d):
    sid = d[i]; i += 1; size, i = leb(d, i); end = i + size
    if sid == 0:
        n, j = leb(d, i)
        if d[j:j+n] == b"polyir.meta":
            doc = json.loads(d[j+n:end]); print("   wasm custom section polyir.meta: functions", [f["name"] for f in doc["functions"]])
    i = end
PY
  echo "   wasm line table names tiny.tl; sumsq(10) = $GOT"
else
  echo "   (set POLYIR_LLVM_BIN to an LLVM with wasm32 support, and install wasmtime: skipped)"
fi
echo "OK"
