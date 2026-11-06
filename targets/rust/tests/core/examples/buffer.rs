use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn fillToCapacity() -> i64 {
    let arr: Ref<Vec<i64>> = share(vec![0; 4 as usize]);
    let stored = 10;
    arr.borrow_mut()[0 as usize] = stored;
    let stored = 20;
    arr.borrow_mut()[1 as usize] = stored;
    let stored = 30;
    arr.borrow_mut()[2 as usize] = stored;
    let stored = 40;
    arr.borrow_mut()[3 as usize] = stored;
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone() + arr.borrow()[3 as usize].clone();
}

pub fn capacityCheck() -> i64 {
    let arr: Ref<Vec<i64>> = share(vec![0; 4 as usize]);
    let stored = 1;
    arr.borrow_mut()[0 as usize] = stored;
    let stored = 2;
    arr.borrow_mut()[1 as usize] = stored;
    return (arr.borrow().len() as i64);
}

pub fn letBoundIndexedFill() -> i64 {
    let arr: Ref<Vec<i64>> = share(vec![0; 3 as usize]);
    let stored = 5;
    arr.borrow_mut()[0 as usize] = stored;
    let stored = 6;
    arr.borrow_mut()[1 as usize] = stored;
    let stored = 7;
    arr.borrow_mut()[2 as usize] = stored;
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone();
}

pub fn sumViaLoop(n: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(vec![0; 10 as usize]);
    {
        let mut i: i64 = 0;
        while i < n {
            let stored = i;
            arr.borrow_mut()[i as usize] = stored;
            i = i + 1;
        }
    }
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < n {
            total = total + arr.borrow()[i as usize].clone();
            i = i + 1;
        }
    }
    return total;
}

pub fn stringElements() -> i64 {
    let names: Ref<Vec<String>> = share(vec![String::new(); 2 as usize]);
    let stored = String::from("first");
    names.borrow_mut()[0 as usize] = stored;
    let stored = String::from("second");
    names.borrow_mut()[1 as usize] = stored;
    if names.borrow()[0 as usize].clone() != String::from("first") {
        return 0;
    }
    if names.borrow()[1 as usize].clone() != String::from("second") {
        return 0;
    }
    return 1;
}

pub fn sumParam(arr: Ref<Vec<i64>>) -> i64 {
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone() + (arr.borrow().len() as i64);
}

pub fn viaParam() -> i64 {
    let arr: Ref<Vec<i64>> = share(vec![0; 3 as usize]);
    let stored = 100;
    arr.borrow_mut()[0 as usize] = stored;
    let stored = 200;
    arr.borrow_mut()[1 as usize] = stored;
    let stored = 300;
    arr.borrow_mut()[2 as usize] = stored;
    return sumParam(arr.clone());
}
