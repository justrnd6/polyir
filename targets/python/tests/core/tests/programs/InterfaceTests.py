from __future__ import annotations
import struct
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from interfaces import Rect, Shape, Square, chosen, direct_call, interface_argument, sum_all, two_interfaces, via_field
from Check import off, off_int

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

def check_conversion_at_a_call() -> float:
    return off_int(direct_call(), 920)

def check_conversion_at_a_return() -> float:
    return off_int(chosen(), 36034)

def check_darray_of_interface_values() -> float:
    return off_int(sum_all(), 41)

def check_interface_as_a_struct_field() -> float:
    return off_int(via_field(), 307)

def check_one_struct_two_interfaces() -> float:
    return off_int(two_interfaces(), 1016)

def check_interface_method_taking_an_interface() -> float:
    return off_int(interface_argument(), 109)

def check_dispatch_follows_the_value() -> float:
    a: Shape = Square(5)
    b: Shape = Rect(5, 2)
    return off_int(a.area(), 25) + off_int(b.area(), 10)

def check_same_method_name_on_two_structs() -> float:
    s: Square = Square(3)
    r: Rect = Rect(3, 4)
    return off_int(i64(s.area() + r.area()), 21)

def check_converted_value_is_a_copy() -> float:
    s: Square = Square(4)
    a: Shape = s
    b: Shape = s
    return off_int(i64(a.area() + b.area()), 32)

class Scaler(Protocol):
    def half(self, p0: float) -> float: ...
    def offset(self, p0: int) -> int: ...

@dataclass
class Halver:
    base: int

    def half(self, x: float) -> float:
        return f32(x / 2.0)

    def offset(self, by: int) -> int:
        return i64(self.base + by)

def check_argument_widths_follow_the_interface() -> float:
    s: Scaler = Halver(40)
    return off(s.half(9.0), 4.5) + off_int(s.offset(2), 42)

def check_interface_in_an_either() -> float:
    r: Ok | Err = Ok(Rect(4, 4))
    if isinstance(r, Err):
        return 1.0
    s: Shape = r.value
    return off_int(s.area(), 16)

def check_interface_captured_by_a_closure() -> float:
    s: Shape = Square(7)
    f: Callable[[], int] = lambda: s.area()
    return off_int(f(), 49)

def check_interface_through_a_var() -> float:
    s: Shape = Square(2)
    first: int = s.area()
    s = Rect(3, 5)
    return off_int(i64(i64(first * 100) + s.area()), 415)

class Weighted(Protocol):
    def weight(self) -> int: ...

def total_weight(a: Weighted, b: Weighted) -> int:
    return i64(a.weight() + b.weight())

def check_generic_struct_converts_to_an_interface() -> float:
    a: Cell[int] = Cell(5)
    b: Cell[float] = Cell(2.5)
    return off_int(total_weight(a, b), 14) + off_int(a.get(), 5) + off(f32(b.get()), 2.5)

def check_generic_interface_values_in_a_darray() -> float:
    all: list[Weighted] = []
    a: Cell[int] = Cell(1)
    all.append(a)
    all.append(a)
    return off_int(i64(all[0].weight() + all[1].weight()), 14)

@dataclass
class Cell[T]:
    value: T

    def weight(self) -> int:
        return 7

    def get(self) -> T:
        return self.value
