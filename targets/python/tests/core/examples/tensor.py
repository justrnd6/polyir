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

def weighted_sum3_d() -> int:
    t: Grid = Grid([2, 3, 4], 0)
    i: int = 0
    while i < 2:
        j: int = 0
        while j < 3:
            k: int = 0
            while k < 4:
                t.set(i, j, k, i64(i64(i64(i * 100) + i64(j * 10)) + k))
                k = i64(k + 1)
            j = i64(j + 1)
        i = i64(i + 1)
    total: int = 0
    i: int = 0
    while i < 2:
        j: int = 0
        while j < 3:
            k: int = 0
            while k < 4:
                total = i64(total + i64(i64(t.get(i, j, k) * i64(i + 1)) * i64(j + 1)))
                k = i64(k + 1)
            j = i64(j + 1)
        i = i64(i + 1)
    return total

def corner_round_trip() -> int:
    t: Grid = Grid([2, 2, 2], 0)
    t.set(0, 0, 0, 1)
    t.set(0, 0, 1, 2)
    t.set(0, 1, 0, 3)
    t.set(1, 0, 0, 4)
    t.set(1, 1, 1, 5)
    return i64(i64(i64(t.get(1, 1, 1) * 100) + i64(t.get(1, 0, 0) * 10)) + t.get(0, 1, 0))

def dims_check() -> int:
    t: Grid = Grid([2, 3, 4], 0)
    return i64(i64(i64(t.dims[0] * 100) + i64(t.dims[1] * 10)) + t.dims[2])

def one_dimensional() -> int:
    t: Grid = Grid([4], 0)
    t[0] = 10
    t[1] = 20
    t[2] = 30
    t[3] = 40
    return i64(t[1] + t[2])

def four_dimensional() -> int:
    t: Grid = Grid([2, 2, 2, 2], 0)
    t.set(0, 0, 0, 0, 1)
    t.set(1, 1, 1, 1, 99)
    t.set(0, 1, 0, 1, 5)
    return i64(i64(t.get(1, 1, 1, 1) * 100) + t.get(0, 1, 0, 1))
