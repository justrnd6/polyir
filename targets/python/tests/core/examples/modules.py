from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

class Geometry:
    @dataclass
    class Point:
        x: int
        y: int

        def manhattan(self) -> int:
            return i64(self.absolute(self.x) + self.absolute(self.y))

        def absolute(self, n: int) -> int:
            return i64(0 - n) if n < 0 else n

    @staticmethod
    def clamp(n: int, low: int, high: int) -> int:
        return low if n < low else (high if n > high else n)

    @staticmethod
    def distance(a: Geometry.Point, b: Geometry.Point) -> int:
        step: Geometry.Point = Geometry.Point(i64(a.x - b.x), i64(a.y - b.y))
        return Geometry.clamp(step.manhattan(), 0, 100)

    class Shapes:
        class Shape(Protocol):
            def area(self) -> int: ...

        @dataclass
        class Square:
            side: int

            def area(self) -> int:
                return i64(self.side * self.side)

        @dataclass
        class Rect:
            w: int
            h: int

            def area(self) -> int:
                return i64(self.w * self.h)

        @staticmethod
        def total(shapes: list[Geometry.Shapes.Shape]) -> int:
            sum_: int = 0
            for s in shapes:
                sum_ = i64(sum_ + s.area())
            return sum_

def main() -> None:
    a: Geometry.Point = Geometry.Point(1, 2)
    b: Geometry.Point = Geometry.Point(4, -2)
    shapes: list[Geometry.Shapes.Shape] = [Geometry.Shapes.Square(3), Geometry.Shapes.Rect(2, 5)]
    size: str = "big" if Geometry.Shapes.total(shapes) > 10 else "small"
    print(f"{Geometry.distance(a, b)} {size} {larger(3, 7, False)} {larger("x", "y", True)} {first_of(["first", "second"])}")

def larger[T](a: T, b: T, a_first: bool) -> T:
    return a if a_first else b

def first_of[T](xs: list[T]) -> T:
    return xs[0]

if __name__ == "__main__":
    main()
