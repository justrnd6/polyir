from __future__ import annotations
from dataclasses import dataclass

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

@dataclass
class Rect:
    width: int
    height: int

def area(r: Rect) -> int:
    return i64(r.width * r.height)
