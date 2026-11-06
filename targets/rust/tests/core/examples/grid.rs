use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

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

pub fn fillAndWeight(scale: i64) -> i64 {
    let g: Ref<Grid<i64>> = Grid::new(vec![3, 4], 0);
    {
        let mut r: i64 = 0;
        while r < 3 {
            {
                let mut c: i64 = 0;
                while c < 4 {
                    let stored = (r * 4 + c) * scale;
                    g.borrow_mut().set(&[r, c], stored);
                    c = c + 1;
                }
            }
            r = r + 1;
        }
    }
    let mut total: i64 = 0;
    {
        let mut r: i64 = 0;
        while r < 3 {
            {
                let mut c: i64 = 0;
                while c < 4 {
                    total = total + g.borrow().get(&[r, c]) * (r + 1);
                    c = c + 1;
                }
            }
            r = r + 1;
        }
    }
    return total;
}

pub fn singleCellRoundTrip() -> i64 {
    let g: Ref<Grid<i64>> = Grid::new(vec![2, 2], 0);
    let stored = 10;
    g.borrow_mut().set(&[0, 0], stored);
    let stored = 20;
    g.borrow_mut().set(&[0, 1], stored);
    let stored = 30;
    g.borrow_mut().set(&[1, 0], stored);
    let stored = 40;
    g.borrow_mut().set(&[1, 1], stored);
    return g.borrow().get(&[1, 0]) - g.borrow().get(&[0, 1]);
}

pub fn dimsCheck() -> i64 {
    let g: Ref<Grid<i64>> = Grid::new(vec![3, 4], 0);
    return g.borrow().dims[0] * 100 + g.borrow().dims[1];
}
