from __future__ import annotations
from collections.abc import Callable
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

visits: int = 0

@dataclass
class Account:
    balance: int
    owner: str

    def deposit(self, amount: int) -> int:
        self.balance = i64(self.balance + amount)
        return self.balance

    def deposit_twice(self, amount: int) -> int:
        self.deposit(amount)
        return self.deposit(amount)

@dataclass
class Vec2:
    x: float
    y: float

    def length2(self) -> float:
        return self.x * self.x + self.y * self.y

    def scaled(self, k: float) -> Vec2:
        return Vec2(self.x * k, self.y * k)

def fib(n: int) -> int:
    return n if n < 2 else i64(fib(i64(n - 1)) + fib(i64(n - 2)))

def classify(n: int) -> int:
    return 1 if n < 0 else (2 if n == 0 else (3 if n < 10 else 4))

def bump() -> int:
    global visits
    visits = i64(visits + 1)
    return visits

def greeting(name: str) -> str:
    return "hello, " + name

def narrow(a: int, b: int) -> int:
    return i32(i32(a * b) + 7)

def apply(f: Callable[[int], int], x: int) -> int:
    return i64(f(x) + f(i64(x + 1)))

def main() -> int:
    score: int = 0
    score = i64(score + fib(10))
    acct: Account = Account(100, "ann")
    score = i64(score + acct.deposit_twice(5))
    v: Vec2 = Vec2(3.0, 4.0)
    w: Vec2 = v.scaled(2.0)
    score = i64(score + i64(int(w.length2())))
    if greeting("bo") == "hello, bo":
        score = i64(score + 1)
    score = i64(score + narrow(6, 7))
    seen: set[int] = set()
    i: int = 0
    while i < 20:
        seen.add(rem(i, 7))
        i = i64(i + 1)
    score = i64(score + len(seen))
    nums: list[int] = []
    i: int = 0
    j: int = 10
    while i < 5:
        nums.append(i64(i * j))
        i = i64(i + 1)
        j = i64(j - 1)
    nums[0] = 100
    score = i64(i64(i64(score + nums[0]) + nums[4]) + len(nums))
    ages: dict[str, int] = {}
    ages["ann"] = 31
    if "ann" in ages:
        score = i64(score + ages.get("ann", 0))
    if ("zed" in ages) == False:
        score = i64(score + 1)
    factor: int = 3
    score = i64(score + apply(lambda x: i64(x * factor), 4))
    b: Box[float] = Box(2.5)
    score = i64(score + i64(int(b.value * 4.0)))
    score = i64(i64(i64(i64(score + classify(i64(0 - 5))) + classify(0)) + classify(7)) + classify(70))
    score = i64(i64(score + bump()) + bump())
    return score

@dataclass
class Box[T]:
    value: T

if __name__ == "__main__":
    raise SystemExit(main())
