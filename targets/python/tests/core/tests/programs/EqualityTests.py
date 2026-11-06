from __future__ import annotations
import copy
from dataclasses import dataclass

from Check import off_int

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object

@dataclass
class Point:
    x: int
    y: int

@dataclass
class Line:
    a: Point
    b: Point

@dataclass
class Named:
    name: str
    tags: list[int]

@dataclass
class Counter:
    n: int
    id: int

@dataclass
class Digit:
    v: int

    def equals(self, other: Digit) -> bool:
        return rem(self.v, 10) == rem(other.v, 10)

    __eq__ = equals

@dataclass
class Wrapped:
    inner: Ok | Err

def check_structs_are_equal_field_by_field() -> float:
    p: Point = Point(1, 2)
    q: Point = Point(1, 2)
    r: Point = Point(1, 3)
    score: int = 0
    if p == q:
        score = i64(score + 1)
    if p != r:
        score = i64(score + 2)
    if p == r:
        score = i64(score + 100)
    return off_int(score, 3)

def check_nested_structs_are_equal_recursively() -> float:
    p: Point = Point(1, 2)
    z: Point = Point(3, 4)
    a: Line = Line(p, z)
    b: Line = Line(Point(1, 2), Point(3, 4))
    c: Line = Line(z, p)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    return off_int(score, 3)

def check_a_string_and_a_list_inside_a_struct_are_compared_by_content() -> float:
    t1: list[int] = []
    t1.append(1)
    t2: list[int] = []
    t2.append(1)
    a: Named = Named("a" + "b", t1)
    b: Named = Named("ab", t2)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    t2.append(2)
    if a != b:
        score = i64(score + 2)
    return off_int(score, 3)

def check_strings_compare_by_characters() -> float:
    x: str = "match"
    y: str = "ma" + "tch"
    score: int = 0
    if x == y:
        score = i64(score + 1)
    if x != "other":
        score = i64(score + 2)
    if x is y:
        score = i64(score + 100)
    return off_int(score, 3)

def check_a_struct_with_var_fields_is_compared_by_what_it_holds_now() -> float:
    a: Counter = Counter(1, 2)
    b: Counter = Counter(1, 2)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    b.n = 5
    if a != b:
        score = i64(score + 2)
    return off_int(score, 3)

def check_identity_is_the_same_object() -> float:
    a: Counter = Counter(1, 2)
    b: Counter = Counter(1, 2)
    alias: Counter = a
    score: int = 0
    if a is alias:
        score = i64(score + 1)
    if (a is not b):
        score = i64(score + 2)
    if a is b:
        score = i64(score + 100)
    return off_int(score, 3)

def check_an_immutable_struct_compared_by_identity_is_an_object() -> float:
    p: Point = Point(1, 2)
    q: Point = Point(1, 2)
    same: Point = p
    score: int = 0
    if p is same:
        score = i64(score + 1)
    if (p is not q):
        score = i64(score + 2)
    if p == q:
        score = i64(score + 4)
    return off_int(score, 7)

def check_a_val_struct_is_a_different_object_that_is_equal() -> float:
    a: Counter = Counter(1, 2)
    b: Counter = copy.copy(a)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if (a is not b):
        score = i64(score + 2)
    return off_int(score, 3)

def check_an_equals_method_decides_what_equal_means() -> float:
    a: Digit = Digit(13)
    b: Digit = Digit(23)
    c: Digit = Digit(24)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    return off_int(score, 3)

def check_lists_are_equal_element_by_element() -> float:
    a: list[int] = []
    b: list[int] = []
    a.append(1)
    a.append(2)
    b.append(1)
    b.append(2)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    b.append(3)
    if a != b:
        score = i64(score + 2)
    if a is b:
        score = i64(score + 100)
    return off_int(score, 3)

def check_lists_of_structs_are_equal_deeply() -> float:
    a: list[Point] = []
    b: list[Point] = []
    a.append(Point(1, 2))
    b.append(Point(1, 2))
    score: int = 0
    if a == b:
        score = i64(score + 1)
    a.append(Point(5, 6))
    b.append(Point(5, 7))
    if a != b:
        score = i64(score + 2)
    return off_int(score, 3)

def check_eithers_are_equal_when_the_same_case_with_an_equal_payload() -> float:
    a: Ok | Err = Ok(5)
    b: Ok | Err = Ok(5)
    c: Ok | Err = Err(5)
    d: Ok | Err = Ok(6)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    if a != d:
        score = i64(score + 4)
    return off_int(score, 7)

def check_variants_are_equal_when_the_same_case_with_an_equal_payload() -> float:
    a: Variant = Variant(1, 2.5)
    b: Variant = Variant(1, 2.5)
    c: Variant = Variant(0, 2)
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    return off_int(score, 3)

def check_a_struct_holding_an_either_is_compared_through_it() -> float:
    a: Wrapped = Wrapped(Ok(1))
    b: Wrapped = Wrapped(Ok(1))
    c: Wrapped = Wrapped(Err("no"))
    score: int = 0
    if a == b:
        score = i64(score + 1)
    if a != c:
        score = i64(score + 2)
    return off_int(score, 3)

def check_nan_is_not_equal_to_itself() -> float:
    nan: float = 0.0 / 0.0
    score: int = 0
    if nan == nan:
        score = i64(score + 100)
    if nan != nan:
        score = i64(score + 1)
    return off_int(score, 1)
