use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sqrtf(x: f32) -> f32 { x.sqrt() }
fn sinf(x: f32) -> f32 { x.sin() }
fn cosf(x: f32) -> f32 { x.cos() }

pub fn dft(k: i32, x: Ref<Vec<f32>>, p: Ref<Vec<f32>>) -> f32 {
    let n: i32 = (p.borrow()[0 as usize].clone() as i32);
    let mut re: f32 = 0.0f32;
    let mut im: f32 = 0.0f32;
    {
        let mut j: i64 = 0;
        while j < (n as i64) {
            let turns: f32 = ((k * (j as i32) % n) as f32) / (n as f32);
            re = re + x.borrow()[j as usize].clone() * cosf(((6.2831853 * (turns as f64)) as f32));
            im = im - x.borrow()[j as usize].clone() * sinf(((6.2831853 * (turns as f64)) as f32));
            j = j + 1;
        }
    }
    return ((2.0 * (sqrtf(re * re + im * im) as f64) / ((n as f32) as f64)) as f32);
}
