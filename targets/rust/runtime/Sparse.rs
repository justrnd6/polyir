// A sparse grid keeps the cells written; a read finds the last write, else the zero.
#[derive(Clone, PartialEq)]
pub struct Sparse<T> {
    pub cells: Vec<(i64, i64, T)>,
    pub zero: T,
}

impl<T: Clone> Sparse<T> {
    pub fn new(zero: T) -> Ref<Sparse<T>> {
        share(Sparse { cells: Vec::new(), zero })
    }

    pub fn get(&self, row: i64, col: i64) -> T {
        self.cells.iter().rev().find(|cell| cell.0 == row && cell.1 == col).map(|cell| cell.2.clone()).unwrap_or(self.zero.clone())
    }

    pub fn set(&mut self, row: i64, col: i64, value: T) {
        self.cells.push((row, col, value));
    }
}
