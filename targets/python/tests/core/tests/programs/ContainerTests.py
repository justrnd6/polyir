from __future__ import annotations
import copy
import sys
from collections.abc import Callable
from dataclasses import dataclass

from Check import off_int

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A push used as a value gives the list, as in polyir (Python's append gives nothing).
def pushed(items: list, item) -> list:
    items.append(item)
    return items

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

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

# A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(i, j, value)`
# writes one.
class Grid:
    def __init__(self, dims: list, zero):
        self.dims = dims
        count = 1
        for size in dims:
            count *= size
        self.cells = [zero] * count

    def place(self, indices) -> int:
        at = 0
        for size, index in zip(self.dims, indices):
            at = at * size + index
        return at

    def get(self, *indices):
        return self.cells[self.place(indices)]

    def set(self, *indices_and_value):
        self.cells[self.place(indices_and_value[:-1])] = indices_and_value[-1]

    def copy(self) -> Grid:
        out = Grid(self.dims, None)
        out.cells = list(self.cells)
        return out

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# What `throw` raises: the value thrown.
class Thrown(Exception):
    def __init__(self, value):
        super().__init__(value)
        self.value = value

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
class Bag:
    items: list[int]
    total: int

@dataclass
class Pt:
    x: int
    y: int

def push_three(xs: list[int]) -> int:
    xs.append(3)
    return len(xs)

def push_many(xs: list[int], count: int) -> int:
    i: int = 0
    while i < count:
        xs.append(i64(i * 2))
        i = i64(i + 1)
    return len(xs)

def check_a_push_inside_a_function_is_seen_by_the_caller() -> float:
    xs: list[int] = []
    xs.append(1)
    push_three(xs)
    return off_int(len(xs), 2) + off_int(xs[1], 3)

def check_many_pushes_inside_a_function_leave_the_caller_valid() -> float:
    xs: list[int] = []
    xs.append(7)
    push_many(xs, 100)
    return off_int(len(xs), 101) + off_int(xs[0], 7) + off_int(xs[1], 0) + off_int(xs[100], 198)

def check_two_names_for_one_darray_share_it() -> float:
    a: list[int] = []
    b: list[int] = a
    a.append(5)
    b.append(6)
    return off_int(len(a), 2) + off_int(len(b), 2) + off_int(a[1], 6) + off_int(b[0], 5)

def check_an_element_written_in_a_function_is_seen_by_the_caller() -> float:
    xs: list[int] = []
    xs.append(1)
    xs.append(2)
    overwrite_first(xs, 40)
    return off_int(xs[0], 40) + off_int(xs[1], 2)

def overwrite_first(xs: list[int], value: int) -> int:
    xs[0] = value
    return 0

def make_filled(count: int) -> list[int]:
    xs: list[int] = []
    i: int = 0
    while i < count:
        xs.append(i64(i + 1))
        i = i64(i + 1)
    return xs

def check_a_returned_darray_is_the_one_the_function_built() -> float:
    xs: list[int] = make_filled(4)
    xs.append(50)
    return off_int(len(xs), 5) + off_int(xs[3], 4) + off_int(xs[4], 50)

def check_a_struct_field_is_the_same_darray() -> float:
    xs: list[int] = []
    bag: Bag = Bag(xs, 0)
    xs.append(8)
    bag.items.append(9)
    return off_int(len(bag.items), 2) + off_int(len(xs), 2) + off_int(bag.items[1], 9)

def check_a_method_style_push_on_a_field_works() -> float:
    bag: Bag = Bag([], 0)
    bag.items.append(4)
    bag.items.append(5)
    return off_int(len(bag.items), 2) + off_int(bag.items[1], 5)

def check_concatenation_makes_a_new_darray() -> float:
    a: list[int] = []
    b: list[int] = []
    a.append(1)
    b.append(2)
    c: list[int] = [*a, *b]
    a.append(99)
    return off_int(len(c), 2) + off_int(c[0], 1) + off_int(c[1], 2) + off_int(len(a), 2)

def check_a_closure_shares_the_darray_it_captures() -> float:
    xs: list[int] = []
    def closure1(v: int):
        xs.append(v)
        return len(xs)
    add: Callable[[int], int] = closure1
    add(10)
    add(20)
    return off_int(len(xs), 2) + off_int(xs[1], 20)

def check_push_gives_back_the_same_darray() -> float:
    xs: list[int] = []
    xs = pushed(xs, 5)
    xs = pushed(xs, 6)
    return off_int(len(xs), 2) + off_int(xs[1], 6)

def check_reassigning_a_var_switches_darrays() -> float:
    xs: list[int] = []
    other: list[int] = []
    other.append(1)
    other.append(2)
    other.append(3)
    xs = other
    xs.append(4)
    return off_int(len(other), 4) + off_int(len(xs), 4)

def check_a_list_of_lists_shares_its_inner_lists() -> float:
    outer: list[list[int]] = []
    a: list[int] = []
    a.append(1)
    a.append(2)
    outer.append(a)
    b: list[int] = []
    b.append(10)
    outer.append(b)
    a.append(3)
    first: list[int] = outer[0]
    second: list[int] = outer[1]
    return off_int(len(outer), 2) + off_int(len(first), 3) + off_int(first[2], 3) + off_int(second[0], 10)

def check_a_darray_of_structs_keeps_the_structs() -> float:
    ps: list[Pt] = []
    ps.append(Pt(1, 2))
    ps.append(Pt(3, 4))
    p: Pt = ps[1]
    return off_int(len(ps), 2) + off_int(p.x, 3) + off_int(p.y, 4)

def check_a_thousand_pushes_keep_every_element() -> float:
    xs: list[int] = []
    i: int = 0
    while i < 1000:
        xs.append(i)
        i = i64(i + 1)
    total: int = 0
    i: int = 0
    while i < len(xs):
        total = i64(total + xs[i])
        i = i64(i + 1)
    return off_int(len(xs), 1000) + off_int(total, 499500)

def check_a_list_inside_an_either_is_the_same_list() -> float:
    xs: list[int] = []
    xs.append(4)
    r: Ok | Err = Ok(xs)
    xs.append(5)
    back: list[int] = r.value
    return off_int(len(back), 2) + off_int(back[1], 5)

def fill_sparse(sp: Sparse) -> int:
    sp.set(1, 1, 5)
    sp.set(2, 3, 7)
    return 0

def check_a_sparse_written_in_a_function_is_seen_by_the_caller() -> float:
    sp: Sparse = Sparse(0)
    fill_sparse(sp)
    return off_int(sp.get(1, 1), 5) + off_int(sp.get(2, 3), 7) + off_int(sp.get(9, 9), 0) + off_int(len(sp), 2)

def check_last_write_wins_for_a_sparse() -> float:
    sp: Sparse = Sparse(0)
    sp.set(3, 3, 1)
    sp.set(3, 3, 2)
    sp.set(3, 3, 3)
    return off_int(sp.get(3, 3), 3)

def check_one_dimensional_sparse_shares_too() -> float:
    sp: Sparse = Sparse(0)
    alias: Sparse = sp
    alias.set(0, 0, 100)
    sp.set(7, 0, 200)
    return off_int(sp.get(0, 0), 100) + off_int(alias.get(7, 0), 200)

def fill_map(hm: dict[int, int]) -> int:
    hm[1] = 10
    hm[2] = 20
    hm[1] = 11
    return 0

def check_a_hashmap_written_in_a_function_is_seen_by_the_caller() -> float:
    hm: dict[int, int] = {}
    fill_map(hm)
    return off_int(hm.get(1, 0), 11) + off_int(hm.get(2, 0), 20) + off_int(hm.get(3, 0), 0) + off_int(len(hm), 2)

def check_two_names_for_one_hashmap_share_it() -> float:
    a: dict[int, int] = {}
    b: dict[int, int] = a
    a[5] = 50
    b[6] = 60
    return off_int(len(a), 2) + off_int(b.get(5, 0), 50) + off_int(a.get(6, 0), 60)

def check_a_string_keyed_hashmap_is_shared() -> float:
    hm: dict[str, int] = {}
    add_word(hm, "one", 1)
    add_word(hm, "two", 2)
    return off_int(hm.get("one", 0), 1) + off_int(hm.get("two", 0), 2) + off_int(len(hm), 2)

def add_word(hm: dict[str, int], word: str, value: int) -> int:
    hm[word] = value
    return 0

def check_a_hashmap_of_lists_keeps_the_lists() -> float:
    hm: dict[int, list[int]] = {}
    l: list[int] = []
    hm[7] = l
    l.append(5)
    l.append(6)
    got: list[int] = hm.get(7, [])
    return off_int(len(got), 2) + off_int(got[1], 6)

def fill_grid(g: Grid) -> int:
    r: int = 0
    while r < 2:
        c: int = 0
        while c < 3:
            g.set(r, c, i64(i64(r * 10) + c))
            c = i64(c + 1)
        r = i64(r + 1)
    return 0

def check_a_grid_filled_in_a_function_is_seen_by_the_caller() -> float:
    g: Grid = Grid([2, 3], 0)
    fill_grid(g)
    return off_int(g.get(1, 2), 12) + off_int(g.get(0, 1), 1)

def check_two_names_for_one_grid_share_it() -> float:
    a: Grid = Grid([2, 3], 0)
    b: Grid = a
    a.set(1, 1, 9)
    return off_int(b.get(1, 1), 9)

def check_a_grid_starts_zeroed() -> float:
    t: Grid = Grid([2, 2, 2], 0)
    t.set(1, 1, 1, 5)
    return off_int(t.get(0, 1, 0), 0) + off_int(t.get(1, 1, 1), 5)

def check_darray_copy_is_independent() -> float:
    a: list[int] = []
    a.append(1)
    a.append(2)
    b: list[int] = list(a)
    b.append(3)
    a.append(9)
    return off_int(len(a), 3) + off_int(len(b), 3) + off_int(b[2], 3) + off_int(a[2], 9) + off_int(b[0], 1)

def check_grid_and_tensor_copy_are_independent() -> float:
    g: Grid = Grid([2, 2], 0)
    g.set(1, 1, 5)
    h: Grid = g.copy()
    g.set(1, 1, 6)
    t: Grid = Grid([2, 2, 2], 0)
    t.set(1, 0, 1, 7)
    u: Grid = t.copy()
    t.set(1, 0, 1, 8)
    return off_int(h.get(1, 1), 5) + off_int(g.get(1, 1), 6) + off_int(u.get(1, 0, 1), 7)

def check_sparse_copy_is_independent() -> float:
    s: Sparse = Sparse(0)
    s.set(1, 2, 7)
    c: Sparse = s.copy()
    s.set(3, 4, 8)
    c.set(5, 6, 9)
    return off_int(len(s), 2) + off_int(len(c), 2) + off_int(c.get(1, 2), 7)

def check_hashmap_copy_is_independent() -> float:
    m: dict[int, int] = {}
    m[1] = 10
    c: dict[int, int] = copy.copy(m)
    m[2] = 20
    c[3] = 30
    return off_int(len(m), 2) + off_int(len(c), 2) + off_int(c.get(1, 0), 10) + off_int(c.get(3, 0), 30)

def check_copy_of_a_string_keyed_hashmap() -> float:
    m: dict[str, int] = {}
    m["a"] = 1
    c: dict[str, int] = copy.copy(m)
    m["b"] = 2
    return off_int(c.get("a", 0), 1) + off_int(len(c), 1)

def make_bag(n: int) -> list[int]:
    d: list[int] = []
    d.append(n)
    d.append(i64(n + 1))
    return d

def sum_grid(g: Grid) -> int:
    return i64(g.get(0, 0) + g.get(1, 1))

def holds_onto(store: list[list[int]], d: list[int]) -> int:
    store.append(d)
    return 0

def check_a_returned_container_survives() -> float:
    a: list[int] = make_bag(10)
    b: list[int] = make_bag(20)
    return off_int(i64(i64(i64(a[0] + a[1]) + b[0]) + b[1]), 62)

def check_a_container_lent_to_a_callee_is_still_usable_afterwards() -> float:
    total: int = 0
    i: int = 0
    while i < 50:
        g: Grid = Grid([2, 2], 0)
        g.set(0, 0, i)
        g.set(1, 1, 1)
        total = i64(i64(total + sum_grid(g)) + g.get(0, 0))
        i = i64(i + 1)
    return off_int(total, i64(i64(i64(2 * 1225) + 50) + 0))

def check_a_container_given_to_a_callee_that_keeps_it_survives() -> float:
    store: list[list[int]] = []
    i: int = 0
    while i < 3:
        d: list[int] = []
        d.append(i64(i * 10))
        holds_onto(store, d)
        i = i64(i + 1)
    return off_int(i64(i64(store[0][0] + store[1][0]) + store[2][0]), 30)

def check_a_container_returned_from_inside_an_if_survives() -> float:
    keep: list[int] = []
    keep.append(4)
    other: list[int] = []
    other.append(5)
    total: int = 0
    i: int = 0
    while i < 20:
        scratch: list[int] = []
        scratch.append(i)
        if i == 7:
            total = i64(total + scratch[0])
        i = i64(i + 1)
    return off_int(i64(i64(total + keep[0]) + other[0]), 16)

def early_exit(n: int) -> int:
    scratch: list[int] = []
    scratch.append(n)
    if n > 5:
        return i64(scratch[0] * 2)
    scratch.append(1)
    return i64(len(scratch) + scratch[0])

def check_a_return_inside_an_if_still_releases_and_returns_the_right_value() -> float:
    return off_int(i64(i64(early_exit(9) * 100) + early_exit(2)), i64(i64(18 * 100) + 4))

def greeting(name: str) -> str:
    hello: str = "hello, "
    full: str = hello + name + "!"
    return full

def shout(s: str) -> int:
    if s == "hi!":
        return 1
    return 0

def counter_from(start: int) -> Callable[[], int]:
    n: int = start
    def closure2():
        nonlocal n
        n = i64(n + 1)
        return n
    next_: Callable[[], int] = closure2
    return next_

def apply_twice(f: Callable[[], int]) -> int:
    f()
    return f()

def check_a_returned_string_survives() -> float:
    a: str = greeting("ann")
    b: str = greeting("bo")
    if a != "hello, ann!":
        return 1.0
    if b != "hello, bo!":
        return 1.0
    return 0.0

def check_a_string_lent_to_a_callee_is_usable_afterwards() -> float:
    hits: int = 0
    i: int = 0
    while i < 20:
        s: str = "h" + "i" + "!"
        hits = i64(hits + shout(s))
        if s == "hi!":
            hits = i64(hits + 1)
        i = i64(i + 1)
    return off_int(hits, 40)

def check_a_string_used_as_a_hashmap_key_is_kept() -> float:
    m: dict[str, int] = {}
    i: int = 0
    while i < 3:
        key: str = "k" + "ey"
        m[key] = i
        i = i64(i + 1)
    return off_int(m.get("key", 0), 2) + off_int(len(m), 1)

def check_a_closure_that_is_returned_keeps_its_box() -> float:
    c: Callable[[], int] = counter_from(10)
    c()
    return off_int(c(), 12)

def check_a_closure_given_to_a_callee_keeps_working() -> float:
    n: int = 0
    def closure3():
        nonlocal n
        n = i64(n + 5)
        return n
    tick: Callable[[], int] = closure3
    return off_int(apply_twice(tick), 10) + off_int(n, 10)

def check_a_closure_only_called_locally_in_a_loop() -> float:
    total: int = 0
    i: int = 0
    while i < 50:
        count: int = i
        def closure5():
            nonlocal count
            count = i64(count + 1)
            return count
        bump: Callable[[], int] = closure5
        bump()
        total = i64(i64(total + bump()) - i)
        i = i64(i + 1)
    return off_int(total, 100)

def check_two_closures_share_one_captured_name() -> float:
    n: int = 0
    def closure6():
        nonlocal n
        n = i64(n + 1)
        return n
    inc: Callable[[], int] = closure6
    read: Callable[[], int] = lambda: i64(n * 10)
    inc()
    inc()
    return off_int(read(), 20)

@dataclass
class Pocket:
    items: list[int]

def made_here() -> list[int]:
    d: list[int] = []
    d.append(1)
    return d

def made_through_another() -> list[int]:
    d: list[int] = made_here()
    d.append(2)
    return d

def made_in_place() -> list[int]:
    return []

def same_as_argument(d: list[int]) -> list[int]:
    return d

def element_of(xs: list[list[int]]) -> list[int]:
    return xs[0]

def field_of(p: Pocket) -> list[int]:
    return p.items

def kept_and_returned(store: list[list[int]]) -> list[int]:
    d: list[int] = []
    store.append(d)
    return d

def sometimes_the_argument(flag: int, d: list[int]) -> list[int]:
    if flag == 0:
        return d
    return []

def check_a_fresh_result_is_usable_and_released() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        a: list[int] = made_here()
        b: list[int] = made_through_another()
        c: list[int] = made_in_place()
        c.append(i)
        total = i64(i64(i64(total + len(a)) + len(b)) + len(c))
        i = i64(i + 1)
    return off_int(total, i64(100 * 4))

def check_an_alias_returned_by_a_call_is_not_released_under_its_owner() -> float:
    src: list[int] = []
    src.append(7)
    same: list[int] = same_as_argument(src)
    same.append(8)
    outer: list[list[int]] = []
    outer.append(src)
    inner: list[int] = element_of(outer)
    pocket: Pocket = Pocket(src)
    field: list[int] = field_of(pocket)
    maybe: list[int] = sometimes_the_argument(0, src)
    return off_int(len(src), 2) + off_int(len(inner), 2) + off_int(len(field), 2) + off_int(len(maybe), 2)

def check_an_object_kept_by_the_callee_survives_its_caller() -> float:
    store: list[list[int]] = []
    total: int = 0
    i: int = 0
    while i < 5:
        d: list[int] = kept_and_returned(store)
        d.append(i)
        i = i64(i + 1)
    j: int = 0
    while j < 5:
        total = i64(total + store[j][0])
        j = i64(j + 1)
    return off_int(total, 10)

def handed_on() -> list[int]:
    a: list[int] = made_here()
    return a

def check_a_result_handed_on_again_is_still_fresh() -> float:
    a: list[int] = handed_on()
    return off_int(a[0], 1)

def check_a_discarded_fresh_result_is_harmless() -> float:
    made_here()
    made_through_another()
    return 0.0

def made_struct(n: int) -> Pocket:
    p: Pocket = Pocket([])
    p.items.append(n)
    return p

def make_label(a: str) -> str:
    s: str = a + "-" + a
    return s

def check_a_struct_and_a_string_handed_over_by_a_call() -> float:
    total: int = 0
    i: int = 0
    while i < 50:
        p: Pocket = made_struct(i)
        s: str = make_label("x")
        if s == "x-x":
            total = i64(i64(total + p.items[0]) + 1)
        i = i64(i + 1)
    return off_int(total, i64(1225 + 50))

@dataclass
class Shelf:
    books: list[int]
    tags: str

@dataclass
class Cabinet:
    drawer: list[int]

@dataclass
class Crate:
    items: list[int]

def stocked_shelf(n: int) -> Shelf:
    s: Shelf = Shelf([], "a" + "b")
    s.books.append(n)
    s.books.append(i64(n + 1))
    return s

def open_drawer(c: Cabinet) -> list[int]:
    return c.drawer

def check_an_owned_field_is_usable_then_released_with_its_struct() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        s: Shelf = stocked_shelf(i)
        total = i64(i64(i64(total + len(s.books)) + s.books[0]) - i)
        if s.tags == "ab":
            total = i64(total + 1)
        i = i64(i + 1)
    return off_int(total, i64(100 * 3))

def check_a_replaced_field_value_is_released_and_the_new_one_works() -> float:
    s: Shelf = stocked_shelf(5)
    s.books = []
    s.books.append(40)
    s.tags = "x" + "y"
    if s.tags != "xy":
        return 1.0
    return off_int(len(s.books), 1) + off_int(s.books[0], 40)

def check_a_field_that_is_read_out_is_never_released_with_its_struct() -> float:
    c: Cabinet = Cabinet([])
    c.drawer.append(9)
    drawer: list[int] = open_drawer(c)
    drawer.append(10)
    return off_int(len(c.drawer), 2) + off_int(drawer[1], 10)

def check_a_copied_struct_shares_nothing_it_must_not_free() -> float:
    a: Crate = Crate([])
    a.items.append(3)
    b: Crate = copy.copy(a)
    b.items.append(4)
    return off_int(len(a.items), 2) + off_int(len(b.items), 2)

def check_a_list_of_lists_releases_its_elements() -> float:
    total: int = 0
    i: int = 0
    while i < 100:
        outer: list[list[int]] = []
        j: int = 0
        while j < 3:
            outer.append([])
            outer[j].append(i64(i + j))
            j = i64(j + 1)
        total = i64(i64(i64(i64(total + len(outer)) + len(outer[2])) + outer[1][0]) - i)
        i = i64(i + 1)
    return off_int(total, i64(100 * i64(i64(3 + 1) + 1)))

def first_of(xs: list[list[int]]) -> list[int]:
    first: list[int] = xs[0]
    return first

def check_an_element_read_out_of_a_list_is_not_released_with_it() -> float:
    store: list[list[int]] = []
    store.append([])
    store[0].append(6)
    got: list[int] = first_of(store)
    got.append(7)
    return off_int(len(store[0]), 2) + off_int(got[1], 7)

def check_a_map_past_its_starting_capacity_keeps_every_entry() -> float:
    m: dict[int, int] = {}
    i: int = 0
    while i < 1000:
        m[i] = i64(i * 3)
        i = i64(i + 1)
    wrong: int = 0
    i: int = 0
    while i < 1000:
        if m.get(i, 0) != i64(i * 3):
            wrong = i64(wrong + 1)
        i = i64(i + 1)
    return off_int(wrong, 0) + off_int(len(m), 1000)

def check_a_grown_map_is_still_the_same_map_for_its_aliases() -> float:
    m: dict[int, int] = {}
    alias: dict[int, int] = m
    i: int = 0
    while i < 20:
        alias[i] = i
        i = i64(i + 1)
    return off_int(len(m), 20) + off_int(m.get(19, 0), 19)

def check_string_keys_survive_growing() -> float:
    m: dict[str, int] = {}
    m["a"] = 1
    m["b"] = 2
    m["c"] = 3
    m["d"] = 4
    m["a"] = 10
    return off_int(m.get("a", 0), 10) + off_int(m.get("b", 0), 2) + off_int(m.get("c", 0), 3) + off_int(m.get("d", 0), 4) + off_int(len(m), 4)

@dataclass(unsafe_hash=True)
class GrowKey:
    a: int
    b: int

def check_struct_keys_survive_growing() -> float:
    m: dict[GrowKey, int] = {}
    i: int = 0
    while i < 50:
        m[GrowKey(i, i64(i + 1))] = i
        i = i64(i + 1)
    wrong: int = 0
    i: int = 0
    while i < 50:
        if m.get(GrowKey(i, i64(i + 1)), 0) != i:
            wrong = i64(wrong + 1)
        i = i64(i + 1)
    return off_int(wrong, 0) + off_int(len(m), 50)

def check_a_set_past_its_starting_capacity_keeps_every_member() -> float:
    seen: set[int] = set()
    i: int = 0
    while i < 100:
        seen.add(rem(i, 40))
        i = i64(i + 1)
    absent: int = 0
    if (39 in seen) == False:
        absent = i64(absent + 1)
    if 40 in seen:
        absent = i64(absent + 1)
    return off_int(len(seen), 40) + off_int(absent, 0)

def read_at(xs: list[int], i: int) -> int:
    return xs[i]

def caught_code(xs: list[int], i: int) -> int:
    code: int = 0
    try:
        code = read_at(xs, i)
    except Thrown as thrown:
        c = thrown.value
        code = c
    return code

def check_an_index_past_the_end_of_a_list_throws_minus_one() -> float:
    xs: list[int] = []
    xs.append(5)
    return off_int(caught_code(xs, 1), i64(0 - 1)) + off_int(caught_code(xs, 100000), i64(0 - 1))

def check_a_negative_index_throws_minus_one() -> float:
    xs: list[int] = []
    xs.append(5)
    return off_int(caught_code(xs, i64(0 - 1)), i64(0 - 1))

def check_an_index_inside_the_list_still_reads() -> float:
    xs: list[int] = []
    xs.append(5)
    xs.append(6)
    return off_int(caught_code(xs, 0), 5) + off_int(caught_code(xs, 1), 6)

def check_an_empty_list_cannot_be_read() -> float:
    xs: list[int] = []
    return off_int(caught_code(xs, 0), i64(0 - 1))

def divide_by(a: int, b: int) -> int:
    out: int = 0
    try:
        out = div(a, b)
    except Thrown as thrown:
        c = thrown.value
        out = c
    return out

def check_dividing_by_zero_throws_minus_four() -> float:
    return off_int(divide_by(10, 0), i64(0 - 4)) + off_int(divide_by(10, 2), 5)

def check_a_fixed_array_and_a_grid_are_checked_per_dimension() -> float:
    code: int = 0
    g: Grid = Grid([2, 2], 0)
    try:
        g.set(0, 5, 1)
    except Thrown as thrown:
        c = thrown.value
        code = c
    return off_int(code, i64(0 - 1))

@dataclass
class OptPoint:
    x: int
    y: int

def optional_find(xs: list[int], want: int) -> int | None:
    i: int = 0
    while i < len(xs):
        if xs[i] == want:
            return i
        i = i64(i + 1)
    return None

def optional_point(ok: int) -> OptPoint | None:
    if ok == 1:
        return OptPoint(7, 8)
    return None

def optional_name(ok: int) -> str | None:
    if ok == 1:
        return "ann"
    return None

def unwrap_code(o: int | None) -> int:
    code: int = 0
    try:
        code = present(o)
    except Thrown as thrown:
        c = thrown.value
        code = c
    return code

def check_an_optional_holds_a_value_or_none() -> float:
    xs: list[int] = []
    xs.append(10)
    xs.append(20)
    seen: int = 0
    if optional_find(xs, 20) is not None:
        seen = i64(seen + 1)
    if optional_find(xs, 99) is None:
        seen = i64(seen + 10)
    return off_int(seen, 11) + off_int(present(optional_find(xs, 20)), 1)

def check_an_optional_of_a_struct_and_of_a_string_work() -> float:
    p: OptPoint | None = optional_point(1)
    seen: int = 0
    if p is not None:
        seen = i64(i64(seen + present(p).x) + present(p).y)
    if optional_point(0) is None:
        seen = i64(seen + 100)
    if optional_name(1) is not None:
        seen = i64(seen + 1000)
    return off_int(seen, 1115)

def check_unwrapping_none_throws_minus_three() -> float:
    return off_int(unwrap_code(None), i64(0 - 3)) + off_int(unwrap_code(42), 42)
