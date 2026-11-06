import sys
from dataclasses import dataclass

# A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
# inside the string, and a search starts inside it.
def text_length(text: str) -> int:
    return len(text.encode())

def text_byte(text: str, at: int) -> int:
    data = text.encode()
    if at < 0 or at >= len(data):
        return 0
    return data[at] - 256 if data[at] > 127 else data[at]

def text_slice(text: str, start: int, end: int) -> str:
    data = text.encode()
    start = max(0, min(start, len(data)))
    end = max(start, min(end, len(data)))
    return data[start:end].decode("utf-8", "replace")

def text_find(text: str, part: str, start: int) -> int:
    data = text.encode()
    return data.find(part.encode(), max(0, min(start, len(data))))

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Stops the program with a message, as polyir's `fail` does.
def fail(message: str):
    print("trap: " + message, file=sys.stderr)
    sys.exit(1)

# The value an optional holds; an empty one stops the program.
def present(value):
    if value is None:
        fail("unwrap of an empty optional")
    return value

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object

def contains(s: str, part: str) -> bool:
    return text_find(s, part, 0) >= 0

def starts_with(s: str, prefix: str) -> bool:
    if text_length(prefix) > text_length(s):
        return False
    return text_slice(s, 0, text_length(prefix)) == prefix

def ends_with(s: str, suffix: str) -> bool:
    if text_length(suffix) > text_length(s):
        return False
    return text_slice(s, i64(text_length(s) - text_length(suffix)), text_length(s)) == suffix

def join(parts: list[str], sep: str) -> str:
    out: str = ""
    i: int = 0
    while i < len(parts):
        if i > 0:
            out = out + sep
        out = out + parts[i]
        i = i64(i + 1)
    return out

def off_text(got: str, want: str) -> float:
    if got == want:
        return 0.0
    return 1.0

def size_of(n: int) -> str:
    match_value_16_12: str = ""
    match n:
        case 0:
            match_value_16_12 = "none"
        case 1 | 2 | 3:
            match_value_16_12 = "a few"
        case -1:
            match_value_16_12 = "minus one"
        case _:
            match_value_16_12 = "many"
    return match_value_16_12

def check_numbers() -> float:
    return off_text(size_of(0) + size_of(2) + size_of(9) + size_of(-1), "nonea fewmanyminus one")

def sign_of(n: int) -> str:
    match_value_30_16: str = ""
    if n < 0:
        match_value_30_16 = "negative"
    elif n == 0:
        match_value_30_16 = "zero"
    elif n >= 100:
        match_value_30_16 = "large"
    else:
        match_value_30_16 = "positive"
    sign: str = match_value_30_16
    return sign

def check_comparisons() -> float:
    return off_text(sign_of(-3) + " " + sign_of(0) + " " + sign_of(150) + " " + sign_of(5), "negative zero large positive")

def grade_of(score: int) -> str:
    match_value_45_12: str = ""
    if score == 100:
        match_value_45_12 = "perfect"
    elif score >= 90:
        s: int = score
        match_value_45_12 = f"A {s}"
    elif score >= 80:
        match_value_45_12 = "B"
    else:
        match_value_45_12 = "C"
    return match_value_45_12

def check_guards_on_the_subject() -> float:
    return off_text(grade_of(100) + grade_of(93) + grade_of(85) + grade_of(10), "perfectA 93BC")

def yes(reply: str) -> bool:
    match_value_60_12: bool = False
    match reply:
        case "y" | "yes":
            match_value_60_12 = True
        case "n" | "no":
            match_value_60_12 = False
        case _:
            match_value_60_12 = False
    return match_value_60_12

def check_strings() -> float:
    return off_text(f"{'true' if yes("y") else 'false'} {'true' if yes("yes") else 'false'} {'true' if yes("no") else 'false'} {'true' if yes("maybe") else 'false'}", "true true false false")

def kind_of(path: str) -> str:
    match_value_73_12: str = ""
    if path == "Ninafile" or path == "Polyfile":
        match_value_73_12 = "build"
    elif ends_with(path, ".poly"):
        match_value_73_12 = "polyir"
    elif starts_with(path, "README"):
        match_value_73_12 = "docs"
    elif contains(path, "test") and not ends_with(path, ".md"):
        match_value_73_12 = "a test"
    elif ends_with(path, ".md"):
        p: str = path
        match_value_73_12 = f"markdown {p}"
    else:
        match_value_73_12 = "other"
    return match_value_73_12

def check_conditions() -> float:
    kinds: list[str] = [kind_of("Polyfile"), kind_of("a.poly"), kind_of("README.md"), kind_of("my_test.c"), kind_of("notes.md"), kind_of("x.c")]
    return off_text(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other")

def word(ready: bool) -> str:
    match_value_89_12: str = ""
    match ready:
        case True:
            match_value_89_12 = "go"
        case False:
            match_value_89_12 = "wait"
    return match_value_89_12

def check_booleans() -> float:
    return off_text(word(True) + word(False), "gowait")

def shown(found: int | None) -> str:
    match_value_102_12: str = ""
    if found is not None and present(found) > 100:
        n: int = present(found)
        match_value_102_12 = f"big {n}"
    elif found is not None:
        n: int = present(found)
        match_value_102_12 = f"found {n}"
    else:
        match_value_102_12 = "nothing"
    return match_value_102_12

def check_optionals() -> float:
    return off_text(shown(500) + ", " + shown(5) + ", " + shown(None), "big 500, found 5, nothing")

def safe_divide(a: int, b: int) -> Ok | Err:
    if b == 0:
        return Err("divide by zero")
    return Ok(div(a, b))

def message(a: int, b: int) -> str:
    match_122_12: Ok | Err = safe_divide(a, b)
    match_value_122_12: str = ""
    if isinstance(match_122_12, Ok):
        q: int = match_122_12.value
        match_value_122_12 = f"quotient {q}"
    elif isinstance(match_122_12, Err):
        why: str = match_122_12.value
        match_value_122_12 = f"error {why}"
    return match_value_122_12

def check_eithers() -> float:
    return off_text(message(7, 2) + "; " + message(7, 0), "quotient 3; error divide by zero")

def text_of(shape: Variant) -> str:
    match_value_133_12: str = ""
    if shape.tag == 0:
        r: int = shape.value
        match_value_133_12 = f"circle {r}"
    elif shape.tag == 1:
        s: int = shape.value
        match_value_133_12 = f"square {s}"
    elif shape.tag == 2:
        name: str = shape.value
        match_value_133_12 = name
    return match_value_133_12

def check_variants() -> float:
    circle: str = text_of(Variant(0, 2))
    return off_text(circle + ", " + text_of(Variant(1, 4)) + ", " + text_of(Variant(2, "blob")), "circle 2, square 4, blob")

def zero_or(found: int | None) -> str:
    match_value_147_12: str = ""
    if found is not None and present(found) == 0:
        match_value_147_12 = "zero"
    elif found is not None:
        match_value_147_12 = "other"
    else:
        match_value_147_12 = "none"
    return match_value_147_12

def check_payload_patterns() -> float:
    return off_text(zero_or(0) + zero_or(3) + zero_or(None), "zeroothernone")

def said(xs: list[int]) -> str:
    match_value_161_12: str = ""
    if len(xs) == 0:
        match_value_161_12 = "empty"
    elif len(xs) == 1:
        x: int = xs[0]
        match_value_161_12 = f"just {x}"
    elif xs[0] == 0:
        match_value_161_12 = "starts at zero"
    else:
        first: int = xs[0]
        rest: list[int] = drop(xs, 1)
        match_value_161_12 = f"{first}, then {len(rest)} more"
    return match_value_161_12

def check_lists() -> float:
    none: list[int] = []
    return off_text(said(none) + "; " + said([7]) + "; " + said([0, 1, 2]) + "; " + said([3, 4, 5]), "empty; just 7; starts at zero; 3, then 2 more")

def pair_of(xs: list[int]) -> str:
    match_value_176_12: str = ""
    if len(xs) == 2 and xs[0] == xs[1]:
        a: int = xs[0]
        match_value_176_12 = f"a pair of {a}"
    elif len(xs) == 2:
        a: int = xs[0]
        b: int = xs[1]
        match_value_176_12 = f"{a} and {b}"
    elif len(xs) >= 1 and xs[0] < 0:
        match_value_176_12 = "starts below zero"
    else:
        match_value_176_12 = "something else"
    return match_value_176_12

def check_list_guards() -> float:
    same: list[int] = [4, 4]
    two: list[int] = [1, 2]
    low: list[int] = [-1, 2, 3]
    return off_text(pair_of(same) + "; " + pair_of(two) + "; " + pair_of(low) + "; " + pair_of([5]), "a pair of 4; 1 and 2; starts below zero; something else")

def command(c: str) -> str:
    said: str = ""
    if c == "quit" or c == "exit":
        return "bye"
    elif starts_with(c, "say "):
        said = text_slice(c, 4, text_length(c))
    elif c == "noop":
        pass
    return said

def check_statements() -> float:
    return off_text(command("say hi") + "|" + command("quit") + "|" + command("noop") + "|" + command("huh"), "hi|bye||")

def check_nested() -> float:
    a: int = 2
    b: int = 5
    match_value_212_21: str = ""
    match a:
        case 1:
            match_value_212_21 = "one"
        case 2:
            match_value_212_49: str = ""
            match b:
                case 5:
                    match_value_212_49 = "two-five"
                case _:
                    match_value_212_49 = "two"
            match_value_212_21 = match_value_212_49
        case _:
            match_value_212_21 = "many"
    text: str = match_value_212_21
    return off_text(text, "two-five")

def drop[T](xs: list[T], n: int) -> list[T]:
    out: list[T] = []
    i: int = n
    while i < len(xs):
        out.append(xs[i])
        i = i64(i + 1)
    return out
