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
class Pt:
    x: int
    y: int

@dataclass
class Contact:
    name: str
    nickname: str | None

def find(xs: list[int], want: int) -> int | None:
    i: int = 0
    while i < len(xs):
        if xs[i] == want:
            return i
        i = i64(i + 1)
    return None

def first_point(ok: int) -> Pt | None:
    if ok == 1:
        return Pt(7, 8)
    return None

def shown(c: Contact) -> str:
    if c.nickname is not None:
        return present(c.nickname)
    return c.name

def main() -> int:
    xs: list[int] = []
    xs.append(10)
    xs.append(20)
    score: int = 0
    hit: int | None = find(xs, 20)
    if hit is not None:
        score = i64(score + present(hit))
    if find(xs, 99) is None:
        score = i64(score + 10)
    p: Pt | None = first_point(1)
    if p is not None:
        score = i64(score + present(p).x)
    if first_point(0) is None:
        score = i64(score + 100)
    named: Contact = Contact("ann", "annie")
    plain: Contact = Contact("bo", None)
    if shown(named) == "annie":
        score = i64(score + 1000)
    if shown(plain) == "bo":
        score = i64(score + 2000)
    return score

if __name__ == "__main__":
    raise SystemExit(main())
