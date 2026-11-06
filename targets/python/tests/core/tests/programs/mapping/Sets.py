from __future__ import annotations
import sys
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

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

# Stops the program with a message, as polyir's `fail` does.
def fail(message: str):
    print("trap: " + message, file=sys.stderr)
    sys.exit(1)

# The value an optional holds; an empty one stops the program.
def present(value):
    if value is None:
        fail("unwrap of an empty optional")
    return value

# A whole number brought back into the unsigned 8-bit range, as u8 arithmetic wraps.
def u8(value: int) -> int:
    return value % 2**8

# A whole number brought back into the unsigned 32-bit range, as u32 arithmetic wraps.
def u32(value: int) -> int:
    return value % 2**32

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

@dataclass(unsafe_hash=True)
class Point:
    x: int
    y: int

@dataclass
class Named:
    name: str
    tag: str | None

@dataclass
class Cell2:
    v: int
    note: str

    def bump(self) -> int:
        self.v = i64(self.v + 1)
        return self.v

def double(x: int) -> int:
    return i64(x * 2)

def inc(x: int) -> int:
    return i64(x + 1)

def sets() -> int:
    seen: set[Point] = set()
    seen.add(Point(1, 2))
    seen.add(Point(3, 4))
    seen.add(Point(1, 2))
    score: int = len(seen)
    if Point(3, 4) in seen:
        score = i64(score + 10)
    words: set[str] = set()
    words.add("a")
    words.add("b")
    words.add("a")
    score = i64(score + i64(len(words) * 100))
    return score

def maps() -> int:
    m: dict[str, int] = {}
    m["x"] = 5
    m["y"] = 6
    m["x"] = i64(m.get("x", 0) + 1)
    by_point: dict[Point, str] = {}
    by_point[Point(1, 1)] = "p"
    score: int = i64(i64(i64(m.get("x", 0) * 10) + m.get("y", 0)) + len(m))
    if Point(1, 1) in by_point:
        score = i64(score + 100)
    if m.get("none", 0) == 0:
        score = i64(score + 1000)
    return score

def strs() -> int:
    a: str = "ab"
    b: str = a + "cd" + a
    score: int = text_length(b)
    if b == "abcdab":
        score = i64(score + 10)
    if b != a:
        score = i64(score + 100)
    q: str = "say hi {x}"
    score = i64(score + text_length(q))
    return score

def opts() -> int:
    n: Named = Named("a", "t")
    m: Named = Named("a", "t")
    o: Named = Named("a", None)
    score: int = 0
    if n == m:
        score = i64(score + 1)
    if n == o:
        score = i64(score + 10)
    if o.tag is None:
        score = i64(score + 100)
    return i64(score + text_length(present(n.tag)))

def bufs() -> int:
    b: list[int] = [0 for _ in range(4)]
    b[0] = 7
    b[3] = 9
    s: int = 0
    i: int = 0
    while i < 4:
        s = i64(s + b[i])
        i = i64(i + 1)
    return s

def swaps() -> int:
    c: Cell2 = Cell2(1, "n")
    d: Cell2 = c
    d.bump()
    c.note = c.note + "!"
    if d.note == "n!":
        return i64(i64(c.v * 10) + text_length(d.note))
    return 0

def choose(k: int) -> int:
    label: int = 0
    while True:
        if label == 0:
            if k == 0:
                label = 1
                continue
            elif k == 1:
                label = 2
                continue
            else:
                label = 3
                continue
        if label == 1:
            return 100
        if label == 2:
            return 200
        if label == 3:
            return 300

def loops(n: int) -> int:
    s: int = 0
    i: int = 0
    j: int = n
    while i < j:
        s = i64(s + i64(i * j))
        i = i64(i + 1)
        j = i64(j - 1)
    k: int = 0
    t: int = 0
    while t < 4:
        k = i64(k + 1)
        t = i64(t + 1)
    return i64(s + k)

def unsigned() -> int:
    a: int = 250
    b: int = u8(a + 10)
    c: int = 4000000000
    d: int = u32(c + c)
    e: int = 2147483647
    f: int = i32(e + 1)
    return i64(i64(b + d) + rem(f, 1000))

def floats() -> int:
    x: float = 7.5
    y: float = 2.25
    z: float = x / 2.0 + y * 2.0
    if z > 8.0 and x != y:
        return i64(int(z))
    return 0

def main() -> int:
    score: int = 0
    score = i64(i64(i64(i64(i64(i64(score + sets()) + maps()) + strs()) + opts()) + bufs()) + swaps())
    score = i64(i64(i64(i64(i64(i64(score + choose(0)) + choose(1)) + choose(5)) + loops(6)) + unsigned()) + floats())
    return rem(score, 251)

if __name__ == "__main__":
    raise SystemExit(main())
