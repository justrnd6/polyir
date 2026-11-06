from __future__ import annotations
from dataclasses import dataclass

@dataclass
class Point:
    x: float
    y: float

def distance_squared(a: Point, b: Point) -> float:
    dx: float = a.x - b.x
    dy: float = a.y - b.y
    return dx * dx + dy * dy

def dist_from_origin(bx: float, by: float) -> float:
    return distance_squared(Point(0.0, 0.0), Point(bx, by))
