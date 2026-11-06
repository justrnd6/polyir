use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sin(x: f64) -> f64 { x.sin() }
fn cos(x: f64) -> f64 { x.cos() }
fn sqrt(x: f64) -> f64 { x.sqrt() }

pub fn off(got: f32, expected: f32) -> f64 {
    return ((got - expected) as f64);
}

pub fn offInt(got: i64, expected: i64) -> f64 {
    return ((got - expected) as f64);
}

pub fn offRelative(got: f32, expected: f32) -> f64 {
    return (((got - expected) / expected) as f64);
}

pub fn amplitudeAt(data: Ref<Vec<f32>>, start: i64, count: i64, rate: i64, hz: f32) -> f32 {
    let mut re: f64 = 0.0;
    let mut im: f64 = 0.0;
    {
        let mut k: i64 = 0;
        while k < count {
            let angle: f64 = 6.283185307179586 * (hz as f64) * ((start + k) as f64) / (rate as f64);
            re = re + (data.borrow()[(start + k) as usize].clone() as f64) * cos(angle);
            im = im + (data.borrow()[(start + k) as usize].clone() as f64) * sin(angle);
            k = k + 1;
        }
    }
    return ((2.0 * sqrt(re * re + im * im) / (count as f64)) as f32);
}
