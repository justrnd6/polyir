from __future__ import annotations
from dataclasses import dataclass

from Check import off_int

# A new dict with one more entry, or a new set with one more member.
def with_entry(table, key, value):
    out = type(table)(table)
    out[key] = value
    return out

def with_item(items, item):
    out = type(items)(items)
    out.add(item)
    return out

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A dict whose keys, or a set whose members, may be lists, sets or dicts, compared by what they hold. Each is kept under a
# frozen stand-in that is equal exactly when the values are; the first one added stands for all values equal to it.
def frozen(value):
    if isinstance(value, list):
        return ("list", tuple(frozen(item) for item in value))
    if isinstance(value, (set, frozenset)):
        return ("set", frozenset(frozen(item) for item in value))
    if isinstance(value, dict):
        return ("dict", frozenset((frozen(key), frozen(value[key])) for key in value))
    return value

class ValueDict(dict):
    def __init__(self, entries=()):
        super().__init__()
        self.held = {}
        for key, value in ([(key, entries[key]) for key in entries] if isinstance(entries, dict) else entries):
            self[key] = value

    def __setitem__(self, key, value):
        self.held.setdefault(frozen(key), key)
        super().__setitem__(frozen(key), value)

    def __getitem__(self, key):
        return super().__getitem__(frozen(key))

    def get(self, key, default=None):
        return super().get(frozen(key), default)

    def __contains__(self, key):
        return super().__contains__(frozen(key))

    def __delitem__(self, key):
        super().__delitem__(frozen(key))
        del self.held[frozen(key)]

    def __iter__(self):
        return iter(list(self.held.values()))

class ValueSet(set):
    def __init__(self, items=()):
        super().__init__()
        self.held = {}
        for item in items:
            self.add(item)

    def add(self, item):
        self.held.setdefault(frozen(item), item)
        super().add(frozen(item))

    def __contains__(self, item):
        return super().__contains__(frozen(item))

    def discard(self, item):
        super().discard(frozen(item))
        self.held.pop(frozen(item), None)

    def __iter__(self):
        return iter(list(self.held.values()))

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

@dataclass(unsafe_hash=True)
class Point:
    x: int
    y: int

@dataclass(unsafe_hash=True)
class Named:
    name: str
    at: Point

def check_an_array_grows_by_making_a_new_one() -> float:
    a0: list[int] = []
    a1: list[int] = [*a0, 10]
    a2: list[int] = [*a1, 20]
    return off_int(len(a0), 0) + off_int(len(a1), 1) + off_int(len(a2), 2) + off_int(a2[0], 10) + off_int(a2[1], 20)

def check_thaw_and_freeze_copy() -> float:
    a: list[int] = [1, 2]
    m: list[int] = list(a)
    m.append(3)
    f: list[int] = list(m)
    m.append(4)
    return off_int(len(a), 2) + off_int(len(m), 4) + off_int(len(f), 3) + off_int(f[2], 3)

def check_a_map_grows_by_making_a_new_one() -> float:
    m0: dict[int, int] = {}
    m1: dict[int, int] = with_entry(m0, 1, 100)
    m: dict[int, int] = m1
    i: int = 2
    while i < 30:
        m = with_entry(m, i, i64(i * 100))
        i = i64(i + 1)
    return off_int(len(m0), 0) + off_int(len(m1), 1) + off_int(len(m), 29) + off_int(m.get(1, 0), 100) + off_int(m.get(29, 0), 2900) + off_int(m1.get(2, 0), 0)

def check_with_replaces_an_existing_key() -> float:
    m: dict[int, int] = with_entry(with_entry({}, 1, 10), 1, 11)
    return off_int(len(m), 1) + off_int(m.get(1, 0), 11)

def check_has_tells_present_from_absent() -> float:
    m: dict[str, int] = with_entry({}, "a", 0)
    score: int = 0
    if "a" in m:
        score = i64(score + 1)
    if ("b" in m) == False:
        score = i64(score + 2)
    return off_int(score, 3)

def check_a_set_keeps_each_member_once() -> float:
    s: set[int] = with_item(with_item(with_item(set(), 7), 8), 7)
    score: int = 0
    if 7 in s:
        score = i64(score + 1)
    if (9 in s) == False:
        score = i64(score + 2)
    return off_int(len(s), 2) + off_int(score, 3)

def check_a_mutset_changes_in_place() -> float:
    s: set[str] = set()
    s.add("x")
    s.add("y")
    s.add("x")
    score: int = 0
    if "y" in s:
        score = i64(score + 1)
    if ("z" in s) == False:
        score = i64(score + 2)
    return off_int(len(s), 2) + off_int(score, 3)

def check_a_mutmap_changes_in_place() -> float:
    m: dict[int, int] = {}
    m[1] = 10
    m[2] = 20
    m[1] = 11
    return off_int(len(m), 2) + off_int(m.get(1, 0), 11) + off_int(m.get(2, 0), 20)

def check_a_struct_can_be_a_key() -> float:
    seen: dict[Point, int] = {}
    seen[Point(1, 2)] = 10
    seen[Point(3, 4)] = 20
    seen[Point(1, 2)] = 11
    score: int = 0
    if Point(3, 4) in seen:
        score = i64(score + 1)
    if (Point(9, 9) in seen) == False:
        score = i64(score + 2)
    return off_int(len(seen), 2) + off_int(seen.get(Point(1, 2), 0), 11) + off_int(score, 3)

def check_a_struct_with_a_string_is_a_key_by_content() -> float:
    names: set[Named] = with_item(with_item(with_item(set(), Named("a" + "b", Point(1, 1))), Named("ab", Point(1, 1))), Named("c", Point(2, 2)))
    score: int = 0
    if Named("c", Point(2, 2)) in names:
        score = i64(score + 1)
    return off_int(len(names), 2) + off_int(score, 1)

def check_an_immutable_array_is_a_key() -> float:
    lists: dict[list[int], str] = with_entry(with_entry(ValueDict(), [1, 2], "one-two"), [3], "three")
    score: int = 0
    if lists.get([1, 2], "") == "one-two":
        score = i64(score + 1)
    if lists.get([3], "") == "three":
        score = i64(score + 2)
    return off_int(score, 3)

def check_maps_are_equal_whatever_the_order_they_were_built_in() -> float:
    a: dict[int, str] = with_entry(with_entry({}, 1, "a"), 2, "b")
    b: dict[int, str] = with_entry(with_entry({}, 2, "b"), 1, "a")
    c: dict[int, str] = with_entry(with_entry({}, 2, "b"), 1, "z")
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    return off_int(score, 3)

def check_sets_are_equal_whatever_the_order_they_were_built_in() -> float:
    a: set[str] = with_item(with_item(set(), "x"), "y")
    b: set[str] = with_item(with_item(set(), "y"), "x")
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != with_item(set(), "x"):
        score = i64(score + 2)
    return off_int(score, 3)

def check_mutable_and_immutable_lists_are_equal_by_content() -> float:
    a: list[int] = [1, 2]
    b: list[int] = [1, 2]
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a is b:
        score = i64(score + 100)
    return off_int(score, 1)

def check_repeated_with_in_a_loop_is_not_quadratic_in_memory() -> float:
    total: int = 0
    r: int = 0
    while r < 200:
        a: list[int] = []
        m: dict[int, int] = {}
        s: set[int] = set()
        i: int = 0
        while i < 20:
            a = [*a, i]
            m = with_entry(m, i, i64(i * 2))
            s = with_item(s, rem(i, 7))
            i = i64(i + 1)
        total = i64(i64(i64(total + len(a)) + m.get(19, 0)) + len(s))
        r = i64(r + 1)
    return off_int(total, i64(200 * 65))
