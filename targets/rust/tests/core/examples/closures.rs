use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn counterTwice() -> i64 {
    let count: Ref<i64> = share(0);
    let count_moved = count.clone();
    let inc: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = count_moved.borrow().clone() + 1;
        *count_moved.borrow_mut() = assigned;
        return count_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    let a: i64 = (inc)();
    let b: i64 = (inc)();
    return a * 100 + b;
}

pub fn makeMultiplier(factor: i64) -> Rc<dyn Fn(i64) -> i64> {
    let factor_moved = factor.clone();
    let mul: Rc<dyn Fn(i64) -> i64> = Rc::new(move |x: i64| -> i64 {
        return x * factor_moved;
    }) as Rc<dyn Fn(i64) -> i64>;
    return mul.clone();
}

pub fn tripleOf(x: i64) -> i64 {
    let triple: Rc<dyn Fn(i64) -> i64> = makeMultiplier(3);
    return (triple)(x);
}

pub fn sharedCounter() -> i64 {
    let shared: Ref<i64> = share(0);
    let shared_moved = shared.clone();
    let bumpBy1: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = shared_moved.borrow().clone() + 1;
        *shared_moved.borrow_mut() = assigned;
        return shared_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    let shared_moved = shared.clone();
    let bumpBy10: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = shared_moved.borrow().clone() + 10;
        *shared_moved.borrow_mut() = assigned;
        return shared_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    let a: i64 = (bumpBy1)();
    let b: i64 = (bumpBy10)();
    let c: i64 = (bumpBy1)();
    return a * 10000 + b * 100 + c;
}
