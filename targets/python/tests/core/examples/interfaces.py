from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

class Shape(Protocol):
    def area(self) -> int: ...
    def scaled(self, p0: int) -> int: ...

class Named(Protocol):
    def id(self) -> int: ...

class Combiner(Protocol):
    def combine(self, p0: Shape) -> int: ...

@dataclass
class Square:
    side: int

    def area(self) -> int:
        return i64(self.side * self.side)

    def scaled(self, k: int) -> int:
        return i64(self.side * k)

    def id(self) -> int:
        return 1

@dataclass
class Rect:
    w: int
    h: int

    def area(self) -> int:
        return i64(self.w * self.h)

    def scaled(self, k: int) -> int:
        return i64(self.w * k)

@dataclass
class Summer:
    base: int

    def combine(self, other: Shape) -> int:
        return i64(self.base + other.area())

@dataclass
class Holder:
    shape: Shape
    tag: int

def total_area(s: Shape) -> int:
    return s.area()

def direct_call() -> int:
    return i64(i64(total_area(Square(3)) * 100) + total_area(Rect(4, 5)))

def pick(which: int) -> Shape:
    if which == 0:
        return Square(6)
    return Rect(2, 7)

def chosen() -> int:
    a: Shape = pick(0)
    b: Shape = pick(1)
    return i64(i64(i64(a.area() * 1000) + b.area()) + b.scaled(10))

def sum_all() -> int:
    shapes: list[Shape] = []
    shapes.append(Square(2))
    shapes.append(Rect(3, 4))
    shapes.append(Square(5))
    total: int = 0
    i: int = 0
    while i < len(shapes):
        total = i64(total + shapes[i].area())
        i = i64(i + 1)
    return total

def via_field() -> int:
    h: Holder = Holder(Rect(5, 6), 7)
    return i64(i64(h.shape.area() * 10) + h.tag)

def two_interfaces() -> int:
    sq: Square = Square(4)
    n: Named = sq
    s: Shape = sq
    return i64(i64(n.id() * 1000) + s.area())

def interface_argument() -> int:
    c: Combiner = Summer(100)
    return c.combine(Rect(3, 3))

def main() -> int:
    return i64(i64(i64(i64(i64(direct_call() + chosen()) + sum_all()) + via_field()) + two_interfaces()) + interface_argument())

if __name__ == "__main__":
    raise SystemExit(main())
