from __future__ import annotations
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

@dataclass
class Point:
    x: float
    y: float

    def dist(self) -> float:
        return self.x * self.x + self.y * self.y

@dataclass
class Circle:
    r: float

    def dist(self) -> float:
        return self.r * self.r

def dist_sum(px: float, py: float, r: float) -> float:
    p: Point = Point(px, py)
    c: Circle = Circle(r)
    return p.dist() + c.dist()

@dataclass
class Pair:
    a: int
    b: int

    def sum_(self) -> int:
        return i64(self.a + self.b)

    def doubled_sum(self) -> int:
        return i64(self.sum_() * 2)

def doubled_sum_check(a: int, b: int) -> int:
    p: Pair = Pair(a, b)
    return p.doubled_sum()
