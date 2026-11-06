from __future__ import annotations

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

def literal_equals_itself() -> int:
    c: int = 65
    if c == 65:
        return 1
    return 0

def literals_distinguish_correctly() -> int:
    a: int = 97
    b: int = 98
    if a == b:
        return 0
    return 1

def grid_of_chars_round_trip() -> int:
    g: Grid = Grid([1, 3], 0)
    g.set(0, 0, 120)
    g.set(0, 1, 121)
    g.set(0, 2, 122)
    if g.get(0, 0) == 120 and g.get(0, 1) == 121 and g.get(0, 2) == 122:
        return 1
    return 0

def darray_of_chars_round_trip() -> int:
    cs: list[int] = []
    cs.append(104)
    cs.append(105)
    if cs[0] == 104 and cs[1] == 105:
        return 1
    return 0
