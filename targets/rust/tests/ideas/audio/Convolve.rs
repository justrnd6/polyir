use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn convolveSample(i: i32, x: Ref<Vec<f32>>, h: Ref<Vec<f32>>, p: Ref<Vec<i32>>) -> f32 {
    let nx: i32 = p.borrow()[0 as usize].clone();
    let nh: i32 = p.borrow()[1 as usize].clone();
    let mut first: i32 = i - (nx - 1);
    if first < 0 {
        first = 0;
    }
    let mut last: i32 = i;
    if last > nh - 1 {
        last = nh - 1;
    }
    let mut sum: f32 = 0.0f32;
    {
        let mut j: i32 = first;
        while j <= last {
            sum = sum + x.borrow()[(i - j) as usize].clone() * h.borrow()[j as usize].clone();
            j = j + 1;
        }
    }
    return sum;
}
