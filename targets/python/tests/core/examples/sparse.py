from __future__ import annotations

# A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
# written reads as the element's zero.
class Sparse:
    def __init__(self, zero):
        self.rows = []
        self.cols = []
        self.values = []
        self.zero = zero

    def __len__(self) -> int:
        return len(self.values)

    def get(self, row: int, col: int):
        found = self.zero
        for i in range(len(self.values)):
            if self.rows[i] == row and self.cols[i] == col:
                found = self.values[i]
        return found

    def set(self, row: int, col: int, value):
        self.rows.append(row)
        self.cols.append(col)
        self.values.append(value)

    def copy(self) -> Sparse:
        out = Sparse(self.zero)
        out.rows = list(self.rows)
        out.cols = list(self.cols)
        out.values = list(self.values)
        return out

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def write_and_read_back() -> int:
    sp: Sparse = Sparse(0)
    sp.set(0, 0, 10)
    sp.set(2, 3, 20)
    sp.set(5, 1, 30)
    return i64(i64(i64(sp.get(2, 3) * 100) + i64(sp.get(5, 1) * 10)) + sp.get(0, 0))

def unset_cell_is_zero() -> int:
    sp: Sparse = Sparse(0)
    sp.set(1, 1, 99)
    return sp.get(9, 9)

def last_write_wins() -> int:
    sp: Sparse = Sparse(0)
    sp.set(3, 3, 1)
    sp.set(3, 3, 2)
    sp.set(3, 3, 3)
    return sp.get(3, 3)

def one_dimensional_round_trip() -> int:
    sp: Sparse = Sparse(0)
    sp.set(0, 0, 100)
    sp.set(7, 0, 200)
    sp.set(3, 0, 300)
    return i64(i64(i64(sp.get(7, 0) * 100) + i64(sp.get(3, 0) * 10)) + sp.get(0, 0))

def len_after_writes() -> int:
    sp: Sparse = Sparse(0)
    sp.set(0, 0, 1)
    sp.set(1, 1, 2)
    sp.set(1, 1, 3)
    return len(sp)
