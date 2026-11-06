use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sqrtf(x: f32) -> f32 { x.sqrt() }

pub fn scale(x: f32, k: f32) -> f32 {
    return x * k + 1.0f32;
}

pub fn norm(a: f32, b: f32) -> f32 {
    let s: f32 = scale(a, 2.0f32) + b * b;
    return sqrtf(s);
}

pub fn sumTo(n: i32) -> i32 {
    let mut acc: i32 = 0;
    {
        let mut i: i64 = 0;
        while i < (n as i64) {
            if i % 2 == 0 {
                acc = acc + (i as i32);
            }
            i = i + 1;
        }
    }
    return acc;
}

pub fn lane(v: [f32; 4]) -> f32 {
    return v[0 as usize] + v[3 as usize];
}

pub fn smooth(i: i32, src: Ref<Vec<f32>>, n: i32) -> f32 {
    if i == 0 {
        return src.borrow()[i as usize].clone();
    }
    if i == n - 1 {
        return src.borrow()[i as usize].clone();
    }
    return (src.borrow()[(i - 1) as usize].clone() + src.borrow()[i as usize].clone() + src.borrow()[(i + 1) as usize].clone()) / 3.0f32;
}

pub fn leftover(a: f32, b: f32) -> f32 {
    return a % b;
}

pub fn close(a: f32, b: f32) -> i64 {
    let mut diff: f32 = a - b;
    if diff < 0.0f32 {
        diff = ((0.0 - (diff as f64)) as f32);
    }
    let mut size: f32 = b;
    if size < 0.0f32 {
        size = ((0.0 - (size as f64)) as f32);
    }
    if diff <= ((0.000001 * ((size + 1.0f32) as f64)) as f32) {
        return 1;
    }
    return 0;
}
