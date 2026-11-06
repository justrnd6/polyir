use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn sumBuf(buf: Ref<Vec<f64>>, len: i64) -> f64 {
    let mut i: i64 = 0;
    let mut total: f64 = 0.0;
    while i < len {
        i = i + 1;
        total = total + buf.borrow()[i as usize].clone();
    }
    return total;
}
