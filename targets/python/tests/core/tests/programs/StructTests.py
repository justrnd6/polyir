from __future__ import annotations
import copy
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from Check import off_int

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
# written reads as the element's zero.
class Sparse:
    def __init__(self, zero):
        self.rows = []
        self.cols = []
        self.values = []
        self.zero = zero

    def __len__(self) -> int:
        return len(self.values)

    def get(self, row: int, col: int):
        found = self.zero
        for i in range(len(self.values)):
            if self.rows[i] == row and self.cols[i] == col:
                found = self.values[i]
        return found

    def set(self, row: int, col: int, value):
        self.rows.append(row)
        self.cols.append(col)
        self.values.append(value)

    def copy(self) -> Sparse:
        out = Sparse(self.zero)
        out.rows = list(self.rows)
        out.cols = list(self.cols)
        out.values = list(self.values)
        return out

@dataclass
class Counter:
    n: int
    id: int

    def bump(self) -> int:
        self.n = i64(self.n + 1)
        return self.n

@dataclass
class Pt:
    x: int
    y: int

@dataclass
class Holder:
    items: list[int]
    inner: Counter

class Reading(Protocol):
    def read(self) -> int: ...

@dataclass
class Gauge:
    level: int

    def read(self) -> int:
        return self.level

def add_ten(c: Counter) -> int:
    c.n = i64(c.n + 10)
    return 0

def add_ten_on_a_copy(c: Counter) -> int:
    c: Counter = copy.copy(c)
    c.n = i64(c.n + 10)
    return c.n

def check_two_names_for_one_object_share_it() -> float:
    a: Counter = Counter(1, 7)
    b: Counter = a
    b.n = 5
    return off_int(a.n, 5) + off_int(a.id, 7)

def check_a_callee_changes_the_callers_object() -> float:
    a: Counter = Counter(1, 0)
    add_ten(a)
    return off_int(a.n, 11)

def check_a_val_parameter_is_the_callees_own_copy() -> float:
    a: Counter = Counter(1, 0)
    seen: int = add_ten_on_a_copy(a)
    return off_int(seen, 11) + off_int(a.n, 1)

def check_val_in_front_of_an_expression_copies() -> float:
    a: Counter = Counter(1, 3)
    b: Counter = copy.copy(a)
    b.n = 50
    return off_int(a.n, 1) + off_int(b.n, 50) + off_int(b.id, 3)

def check_copy_method_and_val_agree() -> float:
    a: Counter = Counter(4, 0)
    b: Counter = copy.copy(a)
    a.n = 9
    return off_int(b.n, 4)

def check_a_method_changes_this() -> float:
    a: Counter = Counter(0, 0)
    a.bump()
    a.bump()
    return off_int(a.n, 2)

def check_an_immutable_struct_is_a_value() -> float:
    p: Pt = Pt(1, 2)
    q: Pt = copy.copy(p)
    r: Pt = copy.copy(p)
    return off_int(i64(q.x + r.y), 3)

def check_a_field_holding_another_reference_struct() -> float:
    h: Holder = Holder([], Counter(1, 0))
    alias: Holder = h
    alias.inner.n = 8
    alias.items.append(5)
    return off_int(h.inner.n, 8) + off_int(len(h.items), 1)

def check_a_field_can_be_replaced_with_another_object() -> float:
    h: Holder = Holder([], Counter(1, 0))
    keep: Counter = h.inner
    h.inner = Counter(2, 0)
    return off_int(keep.n, 1) + off_int(h.inner.n, 2)

def check_a_generic_struct_with_a_var_field() -> float:
    c: Cell[int] = Cell(1)
    d: Cell[int] = c
    d.v = 9
    return off_int(c.v, 9)

def check_an_interface_value_sees_later_changes() -> float:
    g: Gauge = Gauge(1)
    r: Reading = g
    g.level = 42
    return off_int(r.read(), 42)

def check_a_struct_in_a_darray_is_the_same_object() -> float:
    all: list[Counter] = []
    a: Counter = Counter(1, 0)
    all.append(a)
    a.n = 6
    return off_int(all[0].n, 6)

def check_a_struct_in_a_closure_is_shared() -> float:
    a: Counter = Counter(1, 0)
    def closure1():
        a.n = i64(a.n + 1)
        return a.n
    f: Callable[[], int] = closure1
    f()
    return off_int(a.n, 2)

def make_counter(start: int) -> Counter:
    c: Counter = Counter(start, 1)
    c.n = i64(c.n + 1)
    return c

def lend(c: Counter) -> int:
    return i64(c.n * 2)

def keep_in(store: list[Counter], c: Counter) -> int:
    store.append(c)
    return 0

def check_a_returned_object_survives() -> float:
    a: Counter = make_counter(5)
    b: Counter = make_counter(10)
    return off_int(i64(i64(a.n * 100) + b.n), 611)

def check_an_object_lent_to_a_callee_is_usable_afterwards() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        c: Counter = Counter(i, 0)
        total = i64(i64(total + lend(c)) + c.n)
        c.n = 0
        i = i64(i + 1)
    return off_int(total, i64(3 * 4950))

def check_an_object_another_callee_keeps_survives() -> float:
    store: list[Counter] = []
    i: int = 0
    while i < 4:
        c: Counter = Counter(i64(i * 10), 0)
        keep_in(store, c)
        i = i64(i + 1)
    return off_int(i64(i64(i64(store[0].n + store[1].n) + store[2].n) + store[3].n), 60)

def check_a_val_parameter_copy_is_released_and_the_original_is_not() -> float:
    a: Counter = Counter(1, 0)
    total: int = 0
    i: int = 0
    while i < 50:
        total = i64(total + add_ten_on_a_copy(a))
        i = i64(i + 1)
    return off_int(total, i64(50 * 11)) + off_int(a.n, 1)

def early_return_with_objects(n: int) -> int:
    c: Counter = Counter(n, 0)
    if n > 5:
        return i64(c.n * 2)
    c.bump()
    return c.n

def check_a_return_inside_an_if_releases_the_object_and_keeps_the_value() -> float:
    return off_int(i64(i64(early_return_with_objects(9) * 100) + early_return_with_objects(2)), i64(1800 + 3))

def check_let_var_and_val_on_reference_structs() -> float:
    a: Counter = Counter(1, 0)
    alias: Counter = a
    copy_: Counter = copy.copy(a)
    moving: Counter = a
    a.n = 10
    moving = Counter(100, 0)
    moving.n = i64(moving.n + 1)
    return off_int(alias.n, 10) + off_int(copy_.n, 1) + off_int(moving.n, 101) + off_int(a.n, 10)

def check_val_on_a_container_is_an_independent_copy() -> float:
    d: list[int] = []
    d.append(4)
    e: list[int] = list(d)
    d.append(5)
    return off_int(len(e), 1) + off_int(len(d), 2)

@dataclass
class Dot:
    x: int
    y: int

@dataclass
class Span:
    lo: int
    hi: int

    def width(self) -> int:
        return i64(self.hi - self.lo)

@dataclass
class Meter:
    reading: int
    id: int

    def bump(self) -> int:
        self.reading = i64(self.reading + 1)
        return self.reading

@dataclass
class Plate:
    code: int

    def read(self) -> int:
        return self.code

def check_shorthand_structs_work() -> float:
    d: Dot = Dot(3, 4)
    s: Span = Span(2, 9)
    w: Wrapper[int] = Wrapper(5)
    r: Reading = Plate(7)
    return off_int(i64(d.x + d.y), 7) + off_int(s.width(), 7) + off_int(w.value, 5) + off_int(r.read(), 7)

def check_a_shorthand_struct_with_a_var_field_is_a_reference() -> float:
    m: Meter = Meter(1, 2)
    alias: Meter = m
    alias.bump()
    return off_int(m.reading, 2) + off_int(m.id, 2)

def check_a_hashmap_of_objects_is_usable_then_released() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        m: dict[int, Counter] = {}
        m[1] = Counter(i, 1)
        m[2] = Counter(5, 2)
        m[2] = Counter(6, 2)
        m.get(1, None).bump()
        total = i64(i64(i64(i64(total + m.get(1, None).n) - i) + m.get(2, None).n) + len(m))
        i = i64(i + 1)
    return off_int(total, i64(100 * i64(i64(1 + 6) + 2)))

def check_a_sparse_of_strings_is_usable_then_released() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        s: Sparse = Sparse("")
        s.set(0, 0, "a" + "b")
        s.set(2, 3, "c" + "d")
        if s.get(2, 3) == "cd":
            total = i64(total + len(s))
        i = i64(i + 1)
    return off_int(total, 200)

@dataclass
class Cell[T]:
    v: T

@dataclass
class Wrapper[T]:
    value: T
