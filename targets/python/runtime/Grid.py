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
