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
