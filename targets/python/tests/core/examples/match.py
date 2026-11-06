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

def starts_with(s: str, prefix: str) -> bool:
    if text_length(prefix) > text_length(s):
        return False
    return text_slice(s, 0, text_length(prefix)) == prefix

def ends_with(s: str, suffix: str) -> bool:
    if text_length(suffix) > text_length(s):
        return False
    return text_slice(s, i64(text_length(s) - text_length(suffix)), text_length(s)) == suffix

def size_of(n: int) -> str:
    match_value_10_10: str = ""
    match n:
        case 0:
            match_value_10_10 = "none"
        case 1 | 2 | 3:
            match_value_10_10 = "a few"
        case _:
            match_value_10_10 = "many"
    return match_value_10_10

def grade_of(score: int) -> str:
    match_value_19_10: str = ""
    if score >= 90:
        match_value_19_10 = "A"
    elif score >= 80:
        match_value_19_10 = "B"
    else:
        match_value_19_10 = "C"
    return match_value_19_10

def kind_of(path: str) -> str:
    match_value_28_10: str = ""
    if path == "Ninafile" or path == "Polyfile":
        match_value_28_10 = "build"
    elif ends_with(path, ".poly"):
        match_value_28_10 = "polyir"
    elif starts_with(path, "README"):
        match_value_28_10 = "docs"
    else:
        match_value_28_10 = "other"
    return match_value_28_10

def shown(found: int | None) -> str:
    match_value_38_10: str = ""
    if found is not None and present(found) > 100:
        n: int = present(found)
        match_value_38_10 = f"big {n}"
    elif found is not None:
        n: int = present(found)
        match_value_38_10 = f"found {n}"
    else:
        match_value_38_10 = "nothing"
    return match_value_38_10

def safe_divide(a: int, b: int) -> Ok | Err:
    if b == 0:
        return Err("divide by zero")
    return Ok(div(a, b))

def quotient(a: int, b: int) -> str:
    match_54_10: Ok | Err = safe_divide(a, b)
    match_value_54_10: str = ""
    if isinstance(match_54_10, Ok):
        q: int = match_54_10.value
        match_value_54_10 = f"quotient {q}"
    elif isinstance(match_54_10, Err):
        why: str = match_54_10.value
        match_value_54_10 = why
    return match_value_54_10

def said(xs: list[int]) -> str:
    match_value_62_10: str = ""
    if len(xs) == 0:
        match_value_62_10 = "empty"
    elif len(xs) == 1:
        x: int = xs[0]
        match_value_62_10 = f"just {x}"
    else:
        first: int = xs[0]
        rest: list[int] = drop(xs, 1)
        match_value_62_10 = f"{first}, then {len(rest)} more"
    return match_value_62_10

def report(code: int) -> None:
    if code == 200 or code == 204:
        print("done")
    elif code >= 500:
        print(f"retry {code}")
    elif code >= 400:
        s: int = code
        print(f"failed with {s}")
    else:
        print(f"? {code}")

def main() -> int:
    print(size_of(2))
    print(grade_of(85))
    print(kind_of("main.poly"))
    print(shown(500))
    print(quotient(7, 2))
    print(said([3, 4, 5]))
    report(404)
    return 0

def drop[T](xs: list[T], n: int) -> list[T]:
    out: list[T] = []
    i: int = n
    while i < len(xs):
        out.append(xs[i])
        i = i64(i + 1)
    return out

if __name__ == "__main__":
    raise SystemExit(main())
