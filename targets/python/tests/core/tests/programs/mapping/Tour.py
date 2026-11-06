from __future__ import annotations
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# What `throw` raises: the value thrown.
class Thrown(Exception):
    def __init__(self, value):
        super().__init__(value)
        self.value = value

@dataclass
class Pt:
    x: int
    y: int

@dataclass
class Counter:
    n: int
    name: str

class Shape(Protocol):
    def area(self) -> int: ...

@dataclass
class Sq:
    side: int

    def area(self) -> int:
        return i64(self.side * self.side)

@dataclass
class Rect:
    w: int
    h: int

    def area(self) -> int:
        return i64(self.w * self.h)

def total_area(shapes: list[Shape]) -> int:
    sum_: int = 0
    i: int = 0
    while i < len(shapes):
        sum_ = i64(sum_ + shapes[i].area())
        i = i64(i + 1)
    return sum_

def risky(x: int) -> int:
    if x < 0:
        raise Thrown(7)
    return x

def guarded(x: int) -> int:
    out: int = 0
    try:
        out = risky(x)
    except Thrown as thrown:
        code = thrown.value
        out = i64(code * 10)
    return out

def main() -> int:
    score: int = 0
    p: Pt = Pt(3, 4)
    q: Pt = Pt(3, 4)
    if p == q:
        score = i64(score + 1)
    c: Counter = Counter(5, "hits")
    d: Counter = c
    d.n = i64(d.n + 1)
    if c is d:
        score = i64(score + c.n)
    shapes: list[Shape] = []
    shapes.append(Sq(3))
    shapes.append(Rect(2, 5))
    score = i64(score + total_area(shapes))
    calls: int = 0
    def closure1():
        nonlocal calls
        calls = i64(calls + 1)
        return calls
    tick: Callable[[], int] = closure1
    first: int = tick()
    second: int = tick()
    score = i64(i64(i64(score + first) + second) + calls)
    stock: dict[str, int] = {}
    stock["apple"] = 3
    stock["pear"] = 4
    score = i64(score + i64(stock.get("apple", 0) * stock.get("pear", 0)))
    b: Box[int] = Box(7)
    score = i64(score + b.value)
    score = i64(i64(score + guarded(5)) + guarded(i64(0 - 1)))
    return score

@dataclass
class Box[T]:
    value: T

if __name__ == "__main__":
    raise SystemExit(main())
