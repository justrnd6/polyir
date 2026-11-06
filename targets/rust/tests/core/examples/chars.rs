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

pub fn literalEqualsItself() -> i64 {
    let c: i8 = 65;
    if c == 65 {
        return 1;
    }
    return 0;
}

pub fn literalsDistinguishCorrectly() -> i64 {
    let a: i8 = 97;
    let b: i8 = 98;
    if a == b {
        return 0;
    }
    return 1;
}

pub fn gridOfCharsRoundTrip() -> i64 {
    let g: Ref<Grid<i8>> = Grid::new(vec![1, 3], 0);
    let stored = 120;
    g.borrow_mut().set(&[0, 0], stored);
    let stored = 121;
    g.borrow_mut().set(&[0, 1], stored);
    let stored = 122;
    g.borrow_mut().set(&[0, 2], stored);
    if g.borrow().get(&[0, 0]) == 120 && g.borrow().get(&[0, 1]) == 121 && g.borrow().get(&[0, 2]) == 122 {
        return 1;
    }
    return 0;
}

pub fn darrayOfCharsRoundTrip() -> i64 {
    let cs: Ref<Vec<i8>> = share(Vec::new());
    { let pushed = 104; cs.borrow_mut().push(pushed) };
    { let pushed = 105; cs.borrow_mut().push(pushed) };
    if cs.borrow()[0 as usize].clone() == 104 && cs.borrow()[1 as usize].clone() == 105 {
        return 1;
    }
    return 0;
}
