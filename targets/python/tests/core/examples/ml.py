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

def relu(x: float) -> float:
    if x < 0.0:
        return 0.0
    return x

def xor_forward(x1: float, x2: float) -> float:
    w1: Grid = Grid([2, 2], 0.0)
    w1.set(0, 0, 1.0)
    w1.set(0, 1, 1.0)
    w1.set(1, 0, 1.0)
    w1.set(1, 1, 1.0)
    b1: Grid = Grid([2], 0.0)
    b1[0] = 0.0
    b1[1] = 0.0 - 1.0
    hidden: Grid = Grid([2], 0.0)
    i: int = 0
    while i < 2:
        pre_activation: float = b1[i]
        pre_activation = pre_activation + w1.get(i, 0) * x1
        pre_activation = pre_activation + w1.get(i, 1) * x2
        hidden[i] = relu(pre_activation)
        i = i64(i + 1)
    w2: Grid = Grid([2], 0.0)
    w2[0] = 1.0
    w2[1] = 0.0 - 2.0
    b2: float = 0.0
    output: float = b2
    i: int = 0
    while i < 2:
        output = output + w2[i] * hidden[i]
        i = i64(i + 1)
    return output

def xor_predict(x1: float, x2: float) -> int:
    if xor_forward(x1, x2) >= 0.5:
        return 1
    return 0
