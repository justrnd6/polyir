from __future__ import annotations
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

@dataclass
class Pt:
    x: int
    y: int

@dataclass
class Seg:
    a: Pt
    b: Pt
    tag: int

@dataclass
class Counter:
    hits: int
    name: str

def measure(s: Seg, c: Counter, word: str) -> int:
    dx: int = i64(s.b.x - s.a.x)
    xs: list[int] = []
    xs.append(dx)
    xs.append(7)
    cells: list[int] = [0 for _ in range(3)]
    c.hits = i64(c.hits + dx)
    return i64(len(xs) + c.hits)

def main() -> int:
    seg: Seg = Seg(Pt(1, 2), Pt(4, 6), 9)
    c: Counter = Counter(0, "ctr")
    return measure(seg, c, "hello")

if __name__ == "__main__":
    raise SystemExit(main())
