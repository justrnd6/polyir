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

pub fn relu(x: f64) -> f64 {
    if x < 0.0 {
        return 0.0;
    }
    return x;
}

pub fn xorForward(x1: f64, x2: f64) -> f64 {
    let w1: Ref<Grid<f64>> = Grid::new(vec![2, 2], 0.0);
    let stored = 1.0;
    w1.borrow_mut().set(&[0, 0], stored);
    let stored = 1.0;
    w1.borrow_mut().set(&[0, 1], stored);
    let stored = 1.0;
    w1.borrow_mut().set(&[1, 0], stored);
    let stored = 1.0;
    w1.borrow_mut().set(&[1, 1], stored);
    let b1: Ref<Grid<f64>> = Grid::new(vec![2], 0.0);
    let stored = 0.0;
    b1.borrow_mut().set(&[0], stored);
    let stored = 0.0 - 1.0;
    b1.borrow_mut().set(&[1], stored);
    let hidden: Ref<Grid<f64>> = Grid::new(vec![2], 0.0);
    {
        let mut i: i64 = 0;
        while i < 2 {
            let mut preActivation: f64 = b1.borrow().get(&[i]);
            preActivation = preActivation + w1.borrow().get(&[i, 0]) * x1;
            preActivation = preActivation + w1.borrow().get(&[i, 1]) * x2;
            let stored = relu(preActivation);
            hidden.borrow_mut().set(&[i], stored);
            i = i + 1;
        }
    }
    let w2: Ref<Grid<f64>> = Grid::new(vec![2], 0.0);
    let stored = 1.0;
    w2.borrow_mut().set(&[0], stored);
    let stored = 0.0 - 2.0;
    w2.borrow_mut().set(&[1], stored);
    let b2: f64 = 0.0;
    let mut output: f64 = b2;
    {
        let mut i: i64 = 0;
        while i < 2 {
            output = output + w2.borrow().get(&[i]) * hidden.borrow().get(&[i]);
            i = i + 1;
        }
    }
    return output;
}

pub fn xorPredict(x1: f64, x2: f64) -> i64 {
    if xorForward(x1, x2) >= 0.5 {
        return 1;
    }
    return 0;
}
