use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn collisionAndOverwrite() -> i64 {
    let hm: Ref<HashMap<i64, i64>> = share(HashMap::new());
    let stored = 100;
    hm.borrow_mut().insert(0, stored);
    let stored = 500;
    hm.borrow_mut().insert(5, stored);
    let stored = 10;
    hm.borrow_mut().insert(1, stored);
    let stored = 20;
    hm.borrow_mut().insert(1, stored);
    return hm.borrow().get(&0).cloned().unwrap_or_else(|| 0) * 10000 + hm.borrow().get(&5).cloned().unwrap_or_else(|| 0) * 10 + hm.borrow().get(&1).cloned().unwrap_or_else(|| 0);
}

pub fn missingKeyIsZero() -> i64 {
    let hm: Ref<HashMap<i64, i64>> = share(HashMap::new());
    let stored = 7;
    hm.borrow_mut().insert(0, stored);
    return hm.borrow().get(&42).cloned().unwrap_or_else(|| 0);
}

pub fn sizeCheck() -> i64 {
    let hm: Ref<HashMap<i64, i64>> = share(HashMap::new());
    let stored = 1;
    hm.borrow_mut().insert(0, stored);
    let stored = 2;
    hm.borrow_mut().insert(1, stored);
    let stored = 99;
    hm.borrow_mut().insert(1, stored);
    return (hm.borrow().len() as i64) * 100;
}

pub fn stringKeyedWriteAndReadBack() -> i64 {
    let hm: Ref<HashMap<String, i64>> = share(HashMap::new());
    let stored = 1;
    hm.borrow_mut().insert(String::from("one"), stored);
    let stored = 2;
    hm.borrow_mut().insert(String::from("two"), stored);
    let stored = 11;
    hm.borrow_mut().insert(String::from("one"), stored);
    return hm.borrow().get(&String::from("one")).cloned().unwrap_or_else(|| 0) * 100 + hm.borrow().get(&String::from("two")).cloned().unwrap_or_else(|| 0);
}

pub fn stringKeyMissingIsZero() -> i64 {
    let hm: Ref<HashMap<String, i64>> = share(HashMap::new());
    let stored = 5;
    hm.borrow_mut().insert(String::from("present"), stored);
    return hm.borrow().get(&String::from("absent")).cloned().unwrap_or_else(|| 0);
}

pub fn hashmapPtrValueMissIsSafe() -> i64 {
    let hm: Ref<HashMap<i64, String>> = share(HashMap::new());
    let stored = String::from("present");
    hm.borrow_mut().insert(1, stored);
    let missing: String = hm.borrow().get(&99).cloned().unwrap_or_else(|| String::new());
    return 1;
}
