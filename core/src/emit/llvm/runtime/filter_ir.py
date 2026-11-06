"""Reads clang's LLVM IR of Collector.c on stdin and prints it ready to go into every module a program is built from (see gen_collector_ir.sh):
only the type, global and function definitions, in plain form (no target lines, attribute groups, metadata, parameter attributes, lifetime
markers, alias-scope declarations or flags an older LLVM does not read). The functions and globals the compiled program shares with the collector become `linkonce_odr`,
so modules built separately and linked together share one collector; the collector's own helpers stay `internal`.

The collector's IR lands in the same module as the program's own functions and globals, which keep the names the program gives them. So
every name of the collector's own (each `internal` function, clang's `.cold` pieces included, and each `internal` or `private` global) is
renamed `@.gc.<name>` (`@.gc.markWord`): a dot cannot appear in a polyir name, so no program function or global can meet one. Its string
constants are renamed `@.gc_str...` so they cannot meet the program's, and so are the C library's struct types (`%struct.gc_timespec`).
The shared names (gc_poll, gc_allocate, gc_top, gc_state, ...) keep their C spelling: the compiled code calls them by it."""
import re
import sys

SKIP = ("source_filename", "target ", "!", "attributes", "; ModuleID", "; Function Attrs", "declare")
MACHO_NAME = re.compile(r'@"\\01_(\w+)(\$\w+)?"')
NOISE = re.compile(r" (noundef|signext|zeroext|nonnull|noalias|local_unnamed_addr|unnamed_addr|nuw|nsw|inbounds|readonly|readnone|writeonly"
                   r"|nocapture|samesign|nneg|disjoint|exact|dso_local|noinline|nounwind|ssp|uwtable)\b")
SIZES = re.compile(r" (captures\([^)]*\)|range\([^)]*\)|dereferenceable(_or_null)?\([0-9]+\)|initializes\(\([^)]*\)(, \([^)]*\))*\)|memory\([^)]*\))")

# The collector's own names: an `internal` function's definition, an `internal` or `private` global's.
OWN_FUNCTION = re.compile(r"^define internal [^@]*@([-\w$.]+)\(")
OWN_GLOBAL = re.compile(r"^@([-\w$.]+) = (internal|private) ")
# Any global name in the IR (`@name`; LLVM's unquoted spelling: letters, digits and `-$._`).
GLOBAL_NAME = re.compile(r"@([-A-Za-z$._][-\w$.]*)")
RESERVED_PREFIX = ".gc."


def clean(line):
    line = re.sub(r" #[0-9]+", "", line)
    line = MACHO_NAME.sub(r"@\1", line)
    line = re.sub(r", ![A-Za-z_][\w.]* ![0-9]+", "", line)
    line = SIZES.sub("", NOISE.sub("", line))
    line = re.sub(r"\balign [0-9]+ ", "", re.sub(r", align [0-9]+", "", line))
    line = line.replace("@.str", "@.gc_str")
    line = re.sub(r"%struct\.(?!Gc)(\w+)", r"%struct.gc_\1", line)  # a C library type (timespec): renamed, so it cannot meet one the module has
    line = re.sub(r"^define (?!internal|private)", "define linkonce_odr ", line)
    return re.sub(r"^(@gc_\w+) = global ", r"\1 = linkonce_odr global ", line)


def ownNames(lines):
    """Every name the collector keeps to itself. A name that starts with a dot (`.gc_str.3`) is one no program can spell already."""
    names = set()
    for line in lines:
        match = OWN_FUNCTION.match(line) or OWN_GLOBAL.match(line)
        if match is not None and not match.group(1).startswith("."):
            names.add(match.group(1))
    return names


def withReservedNames(line, names):
    """The line with each of the collector's own names, wherever it is used, spelled `@.gc.<name>`."""
    def reserved(match):
        if match.group(1) in names:
            return "@" + RESERVED_PREFIX + match.group(1)
        return match.group(0)
    return GLOBAL_NAME.sub(reserved, line)


def plainDeclaration(line):
    line = re.sub(r" #[0-9]+", "", MACHO_NAME.sub(r"@\1", line))
    line = re.sub(r" (immarg|allocptr)\b", "", SIZES.sub("", NOISE.sub("", line)))
    return line.strip()


def isMarker(line):
    """A lifetime marker or an alias-scope declaration: hints an older LLVM does not read, and the collector does not need."""
    return "@llvm.lifetime." in line or "@llvm.experimental.noalias.scope.decl" in line


lines = sys.stdin.read().split("\n")
if "--declarations" in sys.argv:
    # Only the library functions and intrinsics the collector calls, one plain `declare` per line.
    for line in lines:
        if line.startswith("declare") and not isMarker(line):
            print(plainDeclaration(line))
else:
    kept = [clean(line) for line in lines if not line.startswith(SKIP) and not isMarker(line) and line.strip() != ""]
    names = ownNames(kept)
    for line in kept:
        print(withReservedNames(line, names))
