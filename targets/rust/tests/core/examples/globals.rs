use std::cell::RefCell;

thread_local! { static counter: RefCell<i64> = RefCell::new(0); }

pub fn bump() -> i64 {
    counter.with(|cell| *cell.borrow_mut() = counter.with(|cell| cell.borrow().clone()) + 1);
    return counter.with(|cell| cell.borrow().clone());
}
