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

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def fill_and_weight(scale: int) -> int:
    g: Grid = Grid([3, 4], 0)
    r: int = 0
    while r < 3:
        c: int = 0
        while c < 4:
            g.set(r, c, i64(i64(i64(r * 4) + c) * scale))
            c = i64(c + 1)
        r = i64(r + 1)
    total: int = 0
    r: int = 0
    while r < 3:
        c: int = 0
        while c < 4:
            total = i64(total + i64(g.get(r, c) * i64(r + 1)))
            c = i64(c + 1)
        r = i64(r + 1)
    return total

def single_cell_round_trip() -> int:
    g: Grid = Grid([2, 2], 0)
    g.set(0, 0, 10)
    g.set(0, 1, 20)
    g.set(1, 0, 30)
    g.set(1, 1, 40)
    return i64(g.get(1, 0) - g.get(0, 1))

def dims_check() -> int:
    g: Grid = Grid([3, 4], 0)
    return i64(i64(g.dims[0] * 100) + g.dims[1])
