use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn square(x: i64) -> i64 {
    return x * x;
}

pub fn sub(a: i64, b: i64) -> i64 {
    return a - b;
}

pub fn equivalenceCheck(x: i64) -> i64 {
    return square(x) - square(x);
}

pub fn orderingCheck(a: i64, b: i64) -> i64 {
    return sub(a, b) - (a - b);
}

pub fn pushViaMethodPersists() -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 10; arr.borrow_mut().push(pushed) };
    { let pushed = 20; arr.borrow_mut().push(pushed) };
    return (arr.borrow().len() as i64);
}

pub fn pushViaMethodSum() -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 1; arr.borrow_mut().push(pushed) };
    { let pushed = 2; arr.borrow_mut().push(pushed) };
    { let pushed = 3; arr.borrow_mut().push(pushed) };
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone();
}
