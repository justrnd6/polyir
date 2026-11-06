use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn histTest() -> i64 {
    let hist: Ref<Vec<i64>> = share(vec![0; 4 as usize]);
    let stored = 10;
    hist.borrow_mut()[0 as usize] = stored;
    let stored = 20;
    hist.borrow_mut()[1 as usize] = stored;
    let stored = 30;
    hist.borrow_mut()[2 as usize] = stored;
    let stored = 40;
    hist.borrow_mut()[3 as usize] = stored;
    return hist.borrow()[1 as usize].clone() + hist.borrow()[2 as usize].clone();
}

pub fn lenCheck() -> i64 {
    let hist: Ref<Vec<i64>> = share(vec![0; 4 as usize]);
    return (hist.borrow().len() as i64);
}

pub fn sumFixed(arr: Ref<Vec<i64>>) -> i64 {
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone() + arr.borrow()[3 as usize].clone();
}

pub fn lenOnParam(arr: Ref<Vec<i64>>) -> i64 {
    return (arr.borrow().len() as i64);
}

pub fn callWithLocal() -> i64 {
    let hist: Ref<Vec<i64>> = share(vec![0; 4 as usize]);
    let stored = 1;
    hist.borrow_mut()[0 as usize] = stored;
    let stored = 2;
    hist.borrow_mut()[1 as usize] = stored;
    let stored = 3;
    hist.borrow_mut()[2 as usize] = stored;
    let stored = 4;
    hist.borrow_mut()[3 as usize] = stored;
    return sumFixed(hist.clone()) * 100 + lenOnParam(hist.clone());
}
