use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

#[derive(Clone)]
pub struct Rect {
    pub width: i64,
    pub height: i64,
}

impl PartialEq for Rect {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.width == other.width
            && self.height == other.height
    }
}

impl Rect {
    pub fn new(width: i64, height: i64) -> Ref<Rect> {
        share(Rect { width, height })
    }
}

pub trait RectMethods {
}

impl RectMethods for RefCell<Rect> {
}

pub fn area(r: Ref<Rect>) -> i64 {
    return r.borrow().width * r.borrow().height;
}
