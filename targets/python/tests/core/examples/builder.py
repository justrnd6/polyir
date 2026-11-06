from __future__ import annotations
import sys
from dataclasses import dataclass

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

@dataclass
class Shape:
    side: int
    name: str
    tags: list[str]

    def size(self) -> Shape.Size:
        return Shape.Size(i64(self.side * self.side))

    @staticmethod
    def square(side: int) -> Shape:
        return Shape.builder().side(side).name("square").build()

    @staticmethod
    def builder() -> Shape.Builder:
        return Shape.Builder(None, "shape", [])

    @dataclass
    class Size:
        area: int

    @dataclass
    class Builder:
        __side: int | None
        __name: str
        __tags: list[str]

        def side(self, value: int) -> Shape.Builder:
            self.__side = value
            return self

        def name(self, value: str) -> Shape.Builder:
            self.__name = value
            return self

        def tags(self, value: list[str]) -> Shape.Builder:
            self.__tags = value
            return self

        def build(self) -> Shape:
            if self.__side is None:
                fail("Shape.build: side is not set")
            return Shape(present(self.__side), self.__name, self.__tags)

def tagged(b: Shape.Builder) -> Shape.Builder:
    tags: list[str] = ["red", "big"]
    return b.tags(tags)

def main() -> None:
    first: Shape = tagged(Shape.builder()).side(2).build()
    second: Shape = Shape.square(3)
    size: Shape.Size = second.size()
    print(f"{first.name} {first.side} {first.size().area} [{first.tags[0]}, {first.tags[1]}] {second.name} {second.side} {size.area}")

if __name__ == "__main__":
    main()
