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

pub fn weightedSum3D() -> i64 {
    let t: Ref<Grid<i64>> = Grid::new(vec![2, 3, 4], 0);
    {
        let mut i: i64 = 0;
        while i < 2 {
            {
                let mut j: i64 = 0;
                while j < 3 {
                    {
                        let mut k: i64 = 0;
                        while k < 4 {
                            let stored = i * 100 + j * 10 + k;
                            t.borrow_mut().set(&[i, j, k], stored);
                            k = k + 1;
                        }
                    }
                    j = j + 1;
                }
            }
            i = i + 1;
        }
    }
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < 2 {
            {
                let mut j: i64 = 0;
                while j < 3 {
                    {
                        let mut k: i64 = 0;
                        while k < 4 {
                            total = total + t.borrow().get(&[i, j, k]) * (i + 1) * (j + 1);
                            k = k + 1;
                        }
                    }
                    j = j + 1;
                }
            }
            i = i + 1;
        }
    }
    return total;
}

pub fn cornerRoundTrip() -> i64 {
    let t: Ref<Grid<i64>> = Grid::new(vec![2, 2, 2], 0);
    let stored = 1;
    t.borrow_mut().set(&[0, 0, 0], stored);
    let stored = 2;
    t.borrow_mut().set(&[0, 0, 1], stored);
    let stored = 3;
    t.borrow_mut().set(&[0, 1, 0], stored);
    let stored = 4;
    t.borrow_mut().set(&[1, 0, 0], stored);
    let stored = 5;
    t.borrow_mut().set(&[1, 1, 1], stored);
    return t.borrow().get(&[1, 1, 1]) * 100 + t.borrow().get(&[1, 0, 0]) * 10 + t.borrow().get(&[0, 1, 0]);
}

pub fn dimsCheck() -> i64 {
    let t: Ref<Grid<i64>> = Grid::new(vec![2, 3, 4], 0);
    return t.borrow().dims[0 as usize] * 100 + t.borrow().dims[1 as usize] * 10 + t.borrow().dims[2 as usize];
}

pub fn oneDimensional() -> i64 {
    let t: Ref<Grid<i64>> = Grid::new(vec![4], 0);
    let stored = 10;
    t.borrow_mut().set(&[0], stored);
    let stored = 20;
    t.borrow_mut().set(&[1], stored);
    let stored = 30;
    t.borrow_mut().set(&[2], stored);
    let stored = 40;
    t.borrow_mut().set(&[3], stored);
    return t.borrow().get(&[1]) + t.borrow().get(&[2]);
}

pub fn fourDimensional() -> i64 {
    let t: Ref<Grid<i64>> = Grid::new(vec![2, 2, 2, 2], 0);
    let stored = 1;
    t.borrow_mut().set(&[0, 0, 0, 0], stored);
    let stored = 99;
    t.borrow_mut().set(&[1, 1, 1, 1], stored);
    let stored = 5;
    t.borrow_mut().set(&[0, 1, 0, 1], stored);
    return t.borrow().get(&[1, 1, 1, 1]) * 100 + t.borrow().get(&[0, 1, 0, 1]);
}
