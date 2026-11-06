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

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

@dataclass
class Acc:
    total: int
    tag: str
    items: list[int]

    def add(self, x: int) -> int:
        self.total = i64(self.total + x)
        self.items.append(x)
        return self.total

    def later(self) -> Callable[[], int]:
        return lambda: i64(self.total * 2)

    def describe(self) -> str:
        return self.tag + ":" + self.tag

    def self_(self) -> Acc:
        return self

class Animal(Protocol):
    def legs(self, p0: int) -> int: ...
    def name(self) -> str: ...

@dataclass
class Dog:
    age: int

    def legs(self, n: int) -> int:
        return i64(i64(4 * n) + self.age)

    def name(self) -> str:
        return "dog"

@dataclass
class Bird:
    age: int

    def legs(self, n: int) -> int:
        return i64(2 * n)

    def name(self) -> str:
        return "bird"

def risky(x: int) -> int:
    if x < 0:
        raise Thrown(7)
    return x

def try_return(x: int) -> int:
    try:
        return i64(risky(x) + 1)
    except Thrown as thrown:
        code = thrown.value
        return i64(code * 100)
    return 0

def nested(x: int) -> int:
    r: int = 0
    try:
        try:
            r = risky(x)
        except Thrown as thrown:
            a = thrown.value
            r = i64(a + 1)
            raise Thrown(r)
    except Thrown as thrown:
        b = thrown.value
        r = i64(b * 10)
    return r

def make_adder(n: int) -> Callable[[int], int]:
    return lambda x: i64(x + n)

def twice(f: Callable[[int], int], x: int) -> int:
    return f(f(x))

def counter_pair() -> int:
    c: int = 0
    def closure3():
        nonlocal c
        c = i64(c + 1)
        return c
    inc: Callable[[], int] = closure3
    get: Callable[[], int] = lambda: c
    inc()
    inc()
    return i64(i64(get() * 10) + c)

def main() -> int:
    score: int = 0
    a: Acc = Acc(1, "ab", [])
    score = i64(i64(score + a.add(5)) + a.add(6))
    f: Callable[[], int] = a.later()
    score = i64(score + f())
    score = i64(score + len(a.items))
    if a.describe() == "ab:ab":
        score = i64(score + 1000)
    s: Acc = a.self_()
    if s is a:
        score = i64(score + 2000)
    a.tag = "zz"
    if s.tag == "zz":
        score = i64(score + 3000)
    zoo: list[Animal] = []
    zoo.append(Dog(3))
    zoo.append(Bird(1))
    i: int = 0
    while i < len(zoo):
        score = i64(score + zoo[i].legs(2))
        if zoo[i].name() == "bird":
            score = i64(score + 7)
        i = i64(i + 1)
    score = i64(i64(score + try_return(3)) + try_return(i64(0 - 1)))
    score = i64(i64(score + nested(4)) + nested(i64(0 - 2)))
    add3: Callable[[int], int] = make_adder(3)
    score = i64(score + twice(add3, 10))
    score = i64(score + counter_pair())
    return rem(score, 251)

if __name__ == "__main__":
    raise SystemExit(main())
