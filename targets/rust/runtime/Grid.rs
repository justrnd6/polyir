// A grid, matrix or tensor: its sizes, and its cells in one row-major list.
#[derive(Clone, PartialEq)]
pub struct Grid<T> {
    pub dims: Vec<i64>,
    pub cells: Vec<T>,
}

impl<T: Clone> Grid<T> {
    pub fn new(dims: Vec<i64>, zero: T) -> Ref<Grid<T>> {
        let count = dims.iter().product::<i64>() as usize;
        share(Grid { dims, cells: vec![zero; count] })
    }

    fn at(&self, indices: &[i64]) -> usize {
        indices.iter().zip(&self.dims).fold(0, |at, (index, dim)| at * dim + index) as usize
    }

    pub fn get(&self, indices: &[i64]) -> T {
        self.cells[self.at(indices)].clone()
    }

    pub fn set(&mut self, indices: &[i64], value: T) {
        let at = self.at(indices);
        self.cells[at] = value;
    }
}
