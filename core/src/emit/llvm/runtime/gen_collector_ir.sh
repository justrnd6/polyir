#!/usr/bin/env bash
# Regenerates the collector's IR inside ../LlvmCollector.cpp (between its "; BEGIN GENERATED" and "; END GENERATED" lines, and the declarations
# between "// BEGIN DECLARATIONS" and "// END DECLARATIONS") from Collector.c: clang -O1, then filter_ir.py keeps the plain definitions and
# lists the library functions they call. Run it after changing Collector.c, then rebuild libpolyir.
set -eu
cd "$(dirname "$0")"
FLAGS="-O1 -D_FORTIFY_SOURCE=0 -fno-common -fno-strict-aliasing -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector"
raw=$(clang $FLAGS -S -emit-llvm Collector.c -o -)
ir=$(printf '%s\n' "$raw" | python3 filter_ir.py)
declarations=$(printf '%s\n' "$raw" | python3 filter_ir.py --declarations)
python3 - "$ir" "$declarations" <<'PY'
import re, sys
path = "../LlvmCollector.cpp"
text = open(path).read()
ir = "; BEGIN GENERATED\n" + sys.argv[1].strip("\n") + "\n; END GENERATED"
listed = "".join('      "%s",\n' % line for line in sys.argv[2].strip("\n").split("\n"))
text = re.sub(r"; BEGIN GENERATED\n.*?; END GENERATED", lambda _: ir, text, flags=re.S)
text = re.sub(r"(// BEGIN DECLARATIONS\n).*?// END DECLARATIONS", lambda m: m.group(1) + listed + "      // END DECLARATIONS", text, flags=re.S)
open(path, "w").write(text)
PY
