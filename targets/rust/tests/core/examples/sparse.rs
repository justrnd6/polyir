use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

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

pub fn writeAndReadBack() -> i64 {
    let sp: Ref<Sparse<i64>> = Sparse::new(0);
    let stored = 10;
    sp.borrow_mut().set(0, 0, stored);
    let stored = 20;
    sp.borrow_mut().set(2, 3, stored);
    let stored = 30;
    sp.borrow_mut().set(5, 1, stored);
    return sp.borrow().get(2, 3) * 100 + sp.borrow().get(5, 1) * 10 + sp.borrow().get(0, 0);
}

pub fn unsetCellIsZero() -> i64 {
    let sp: Ref<Sparse<i64>> = Sparse::new(0);
    let stored = 99;
    sp.borrow_mut().set(1, 1, stored);
    return sp.borrow().get(9, 9);
}

pub fn lastWriteWins() -> i64 {
    let sp: Ref<Sparse<i64>> = Sparse::new(0);
    let stored = 1;
    sp.borrow_mut().set(3, 3, stored);
    let stored = 2;
    sp.borrow_mut().set(3, 3, stored);
    let stored = 3;
    sp.borrow_mut().set(3, 3, stored);
    return sp.borrow().get(3, 3);
}

pub fn oneDimensionalRoundTrip() -> i64 {
    let sp: Ref<Sparse<i64>> = Sparse::new(0);
    let stored = 100;
    sp.borrow_mut().set(0, 0, stored);
    let stored = 200;
    sp.borrow_mut().set(7, 0, stored);
    let stored = 300;
    sp.borrow_mut().set(3, 0, stored);
    return sp.borrow().get(7, 0) * 100 + sp.borrow().get(3, 0) * 10 + sp.borrow().get(0, 0);
}

pub fn lenAfterWrites() -> i64 {
    let sp: Ref<Sparse<i64>> = Sparse::new(0);
    let stored = 1;
    sp.borrow_mut().set(0, 0, stored);
    let stored = 2;
    sp.borrow_mut().set(1, 1, stored);
    let stored = 3;
    sp.borrow_mut().set(1, 1, stored);
    return (sp.borrow().cells.len() as i64);
}
