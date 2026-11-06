from __future__ import annotations
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A new dict with one more entry, or a new set with one more member.
def with_entry(table, key, value):
    out = type(table)(table)
    out[key] = value
    return out

def with_item(items, item):
    out = type(items)(items)
    out.add(item)
    return out

@dataclass(unsafe_hash=True)
class Point:
    x: int
    y: int

def grow() -> int:
    none: list[int] = []
    one: list[int] = [*none, 10]
    two: list[int] = [*one, 20]
    return i64(i64(i64(i64(len(none) * 1000) + i64(len(one) * 100)) + i64(len(two) * 10)) + div(two[1], 10))

def build() -> list[int]:
    xs: list[int] = []
    i: int = 0
    while i < 5:
        xs.append(i64(i * i))
        i = i64(i + 1)
    return list(xs)

def lookup() -> int:
    ages: dict[str, int] = with_entry(with_entry({}, "ann", 31), "bo", 27)
    seen: set[int] = with_item(with_item(with_item(set(), 3), 5), 3)
    score: int = i64(i64(ages.get("ann", 0) + ages.get("bo", 0)) + len(seen))
    if ("cy" in ages) == False:
        score = i64(score + 100)
    if 5 in seen:
        score = i64(score + 1000)
    return score

def visited() -> int:
    seen: set[Point] = set()
    seen.add(Point(1, 2))
    seen.add(Point(3, 4))
    seen.add(Point(1, 2))
    score: int = len(seen)
    if Point(3, 4) in seen:
        score = i64(score + 10)
    return score

def same_things() -> int:
    a: dict[int, str] = with_entry(with_entry({}, 1, "a"), 2, "b")
    b: dict[int, str] = with_entry(with_entry({}, 2, "b"), 1, "a")
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a is b:
        score = i64(score + 100)
    return score

def main() -> int:
    return i64(i64(i64(i64(grow() + i64(build()[4] * 10000)) + lookup()) + visited()) + same_things())

if __name__ == "__main__":
    raise SystemExit(main())
