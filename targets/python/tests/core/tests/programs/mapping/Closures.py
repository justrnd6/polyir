from __future__ import annotations
import sys
from collections.abc import Callable
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
# inside the string, and a search starts inside it.
def text_length(text: str) -> int:
    return len(text.encode())

def text_byte(text: str, at: int) -> int:
    data = text.encode()
    if at < 0 or at >= len(data):
        return 0
    return data[at] - 256 if data[at] > 127 else data[at]

def text_slice(text: str, start: int, end: int) -> str:
    data = text.encode()
    start = max(0, min(start, len(data)))
    end = max(start, min(end, len(data)))
    return data[start:end].decode("utf-8", "replace")

def text_find(text: str, part: str, start: int) -> int:
    data = text.encode()
    return data.find(part.encode(), max(0, min(start, len(data))))

# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object

# A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
# finished; `give` is a yield that hears whether the coroutine was closed.
class Coroutine:
    def __init__(self, body, *args):
        self.finished = False
        self.started = False
        self.waiting = False
        self.source = body(self, *args)

    def resume(self) -> Variant:
        if self.finished:
            return Variant(1, None)
        self.waiting = False
        try:
            value = self.source.send(False) if self.started else next(self.source)
        except StopIteration:
            self.finished = True
            return Variant(1, None)
        self.started = True
        return Variant(0, value)

    def done(self) -> bool:
        return self.finished

    def give(self, value):
        self.waiting = True
        return (yield value) is True

    def close(self):
        if not self.finished and self.waiting:
            try:
                self.source.send(True)
            except StopIteration:
                pass
        self.finished = True

# Stops the program with a message, as polyir's `fail` does.
def fail(message: str):
    print("trap: " + message, file=sys.stderr)
    sys.exit(1)

# The value an optional holds; an empty one stops the program.
def present(value):
    if value is None:
        fail("unwrap of an empty optional")
    return value

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

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

flag: bool = False
ratio: float = 1.5
hits: int = 0

@dataclass
class Node:
    value: int
    next_: Node | None

@dataclass
class Holder:
    op: Callable[[int], int]
    name: str

    def run_(self, x: int) -> int:
        f: Callable[[int], int] = self.op
        return f(x)

    def via_later(self) -> Callable[[int], int]:
        def closure1(y: int):
            f: Callable[[int], int] = self.op
            return i64(f(y) + text_length(self.name))
        return closure1

def pairs(n: int) -> Coroutine:
    def run(coroutine: Coroutine, n: int):
        i: int = 0
        while i < n:
            yield Node(i, None)
            i = i64(i + 1)

    return Coroutine(run, n)

def sum_nodes(n: int) -> int:
    g: Coroutine = pairs(n)
    s: int = 0
    done: bool = False
    k: int = 0
    while k < 20 and done == False:
        r: Variant = g.resume()
        if r.tag == 1:
            done = True
        else:
            s = i64(s + r.value.value)
        k = i64(k + 1)
    return s

def chain() -> int:
    a: Node = Node(1, None)
    b: Node = Node(2, a)
    c: Node = Node(3, b)
    sum_: int = 0
    cur: Node | None = c
    i: int = 0
    while i < 5 and cur is not None:
        n: Node = present(cur)
        sum_ = i64(sum_ + n.value)
        cur = n.next_
        i = i64(i + 1)
    a.value = 10
    return i64(i64(sum_ * 100) + present(b.next_).value)

def nested_closures(a: int) -> int:
    total: int = 0
    def closure3(x: int):
        def closure2(y: int):
            nonlocal total
            total = i64(total + y)
            return i64(i64(x * y) + a)
        inner: Callable[[int], int] = closure2
        return i64(inner(i64(x + 1)) + inner(2))
    outer: Callable[[int], int] = closure3
    r: int = outer(3)
    return i64(i64(r * 1000) + total)

def str_closure(prefix: str) -> int:
    tail: str = "!"
    f: Callable[[str], str] = lambda s: prefix + s + tail
    return text_length(f("abc"))

def holders() -> int:
    h: Holder = Holder(lambda x: i64(x * x), "hh")
    later: Callable[[int], int] = h.via_later()
    return i64(h.run_(5) + later(2))

def globals() -> int:
    global hits, flag, ratio
    hits = i64(hits + 5)
    flag = True
    ratio = ratio * 2.0
    r: int = hits
    if flag:
        r = i64(r + 100)
    if ratio > 2.5:
        r = i64(r + 1000)
    return r

def find_first(limit: int) -> int:
    i: int = 0
    while i < limit:
        j: int = 0
        while j < limit:
            if i64(i * j) == 12:
                return i64(i64(i * 100) + j)
            j = i64(j + 1)
        i = i64(i + 1)
    return i64(0 - 1)

def try_in_loop(n: int) -> int:
    s: int = 0
    i: int = 0
    while i < n:
        try:
            if rem(i, 2) == 1:
                raise Thrown(i)
            s = i64(s + 10)
        except Thrown as thrown:
            code = thrown.value
            s = i64(s + code)
        i = i64(i + 1)
    return s

def lines(a: str, b: str) -> str:
    label: int = 0
    acc: str = ""
    n: int = 0
    while True:
        if label == 0:
            acc = a
            n = 0
            label = 1
            continue
        if label == 1:
            if n < 3:
                label = 2
                continue
            else:
                label = 3
                continue
        if label == 2:
            acc = acc + b
            n = i64(n + 1)
            label = 1
            continue
        if label == 3:
            return acc

def line_len() -> int:
    return text_length(lines("x", "yz"))

def machine_strings(n: int) -> int:
    label: int = 0
    p: int = 0
    s: str = ""
    q: int = 0
    r: int = 0
    while True:
        if label == 0:
            if n == 0:
                p = n
                s = "p"
                label = 1
                continue
            else:
                q = n
                s = "q"
                label = 2
                continue
        if label == 1:
            if p > 4:
                r = p
                label = 3
                continue
            else:
                q = i64(p + 1)
                s = s + "a"
                label = 2
                continue
        if label == 2:
            if q > 4:
                r = q
                label = 3
                continue
            else:
                p = i64(q + 1)
                s = s + "b"
                label = 1
                continue
        if label == 3:
            return i64(i64(r * 100) + text_length(s))

def precedence(a: int, b: int) -> int:
    x: int = div(i64(i64(a + b) * i64(a - b)), 3)
    y: int = a & b | a ^ b
    z: int = i64(a << i64(2 + b)) >> 1
    s: int = i64(i64(x + y) + z)
    if a & 1 == 1 and b > 2 or a == 0:
        s = i64(s + 1)
    w: int = i32(a)
    if w < 100 and i32(i64(a + b)) < 1000:
        s = i64(s + 2)
    return s

def main() -> int:
    score: int = i64(i64(i64(sum_nodes(5) + chain()) + nested_closures(2)) + str_closure("ab"))
    score = i64(i64(i64(i64(score + holders()) + globals()) + find_first(10)) + try_in_loop(6))
    score = i64(i64(i64(i64(score + line_len()) + machine_strings(0)) + machine_strings(1)) + precedence(13, 7))
    return rem(score, 251)

if __name__ == "__main__":
    raise SystemExit(main())
