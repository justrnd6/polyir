from __future__ import annotations
from dataclasses import dataclass

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

def boxed_i64() -> int:
    b: Box[int] = Box(42)
    return b.get_value()

def boxed_both() -> int:
    bi: Box[int] = Box(100)
    bd: Box[float] = Box(2.0)
    return i64(bi.get_value() + i64(int(bd.get_value())))

def pair_of_int_and_double() -> int:
    p: Pair_i64_double = Pair_i64_double(7, 8.5)
    return p.sum_as_int()

def doubled_i64() -> int:
    d: Doubler_i64 = Doubler_i64(21)
    return d.doubled()

def container_ok() -> int:
    c: Container[int] = Container(Ok(42))
    return c.unwrap_or_default(0)

def make_pair_result(ok: int) -> Ok | Err:
    if ok == 0:
        return Err(-1)
    return Ok(Pair_i64_i64(11, 22))

def pair_result_or_err(ok: int) -> int:
    r: Ok | Err = make_pair_result(ok)
    if isinstance(r, Err):
        return r.value
    p: Pair_i64_i64 = r.value
    return i64(i64(p.a * 100) + p.b)

@dataclass
class Box[T]:
    value: T

    def get_value(self) -> T:
        return self.value

@dataclass
class Pair_i64_double:
    a: int
    b: float

    def sum_as_int(self) -> int:
        return i64(i64(self.a) + i64(int(self.b)))

@dataclass
class Doubler_i64:
    value: int

    def doubled(self) -> int:
        return i64(self.value + self.value)

@dataclass
class Container[T]:
    value: Ok | Err

    def unwrap_or_default(self, def_: T) -> T:
        if isinstance(self.value, Err):
            return def_
        return self.value.value

@dataclass
class Pair_i64_i64:
    a: int
    b: int

    def sum_as_int(self) -> int:
        return i64(i64(self.a) + i64(self.b))
