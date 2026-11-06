use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn atomicAdd(counts: Ref<Vec<i32>>, slot: i32, amount: i32) -> i32 {
    let old: i32 = counts.borrow()[slot as usize].clone();
    let stored = old + amount;
    counts.borrow_mut()[slot as usize] = stored;
    return old;
}

pub fn atomicAddFloat(sums: Ref<Vec<f32>>, slot: i32, amount: f32) -> f32 {
    let old: f32 = sums.borrow()[slot as usize].clone();
    let stored = old + amount;
    sums.borrow_mut()[slot as usize] = stored;
    return old;
}

pub fn atomicMin(values: Ref<Vec<i32>>, slot: i32, candidate: i32) -> i32 {
    let old: i32 = values.borrow()[slot as usize].clone();
    if candidate < old {
        let stored = candidate;
        values.borrow_mut()[slot as usize] = stored;
    }
    return old;
}

pub fn atomicMax(values: Ref<Vec<i32>>, slot: i32, candidate: i32) -> i32 {
    let old: i32 = values.borrow()[slot as usize].clone();
    if candidate > old {
        let stored = candidate;
        values.borrow_mut()[slot as usize] = stored;
    }
    return old;
}

pub fn groupSize() -> i32 {
    return 1;
}

pub fn localIndex(i: i32) -> i32 {
    return 0;
}

pub fn groupIndex(i: i32) -> i32 {
    return i;
}

pub fn barrier() -> i32 {
    return 0;
}
