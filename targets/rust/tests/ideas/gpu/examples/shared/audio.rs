use std::rc::Rc;
use std::cell::RefCell;

#[path = "../../../../core/stdlib/Math.rs"]
mod Math;
use Math::{sign};

#[path = "../../../audio/Wave.rs"]
mod Wave;
use Wave::{fade, sine};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn floorf(x: f32) -> f32 { x.floor() }
fn sinf(x: f32) -> f32 { x.sin() }

pub fn synth(i: i32, p: Ref<Vec<f32>>) -> f32 {
    let t: f32 = (i as f32) / p.borrow()[0 as usize].clone();
    let envelope: f32 = fade(t, p.borrow()[2 as usize].clone(), p.borrow()[3 as usize].clone(), p.borrow()[4 as usize].clone());
    let wobble: f32 = p.borrow()[6 as usize].clone() * sine(p.borrow()[5 as usize].clone() * t);
    let mut sum: f32 = 0.0f32;
    {
        let mut k: i64 = 0;
        while k < ((p.borrow()[1 as usize].clone() as i32) as i64) {
            let cycles: f32 = p.borrow()[(8 + 2 * k) as usize].clone() * t;
            sum = sum + p.borrow()[(9 + 2 * k) as usize].clone() * sinf(((6.2831853 * ((cycles - floorf(cycles)) as f64) + (wobble as f64)) as f32));
            k = k + 1;
        }
    }
    return p.borrow()[7 as usize].clone() * envelope * sum;
}

pub fn squareWave(i: i32, p: Ref<Vec<f32>>) -> f32 {
    return sign(sine(p.borrow()[1 as usize].clone() * (i as f32) / p.borrow()[0 as usize].clone()));
}

pub fn partialSum(i: i32, p: Ref<Vec<f32>>) -> f32 {
    let x: f32 = ((6.2831853 * (((i % (p.borrow()[0 as usize].clone() as i32)) as f32) as f64) / (p.borrow()[0 as usize].clone() as f64)) as f32);
    let mut sum: f32 = 0.0f32;
    {
        let mut h: i64 = 1;
        while h <= ((p.borrow()[2 as usize].clone() as i32) as i64) {
            let number: f32 = (h as f32);
            let wave: f32 = sinf(number * x);
            if p.borrow()[1 as usize].clone() < 0.5f32 {
                if h % 2 == 1 {
                    sum = sum + wave / number;
                }
            }
            if p.borrow()[1 as usize].clone() > 0.5f32 && p.borrow()[1 as usize].clone() < 1.5f32 {
                if h % 2 == 1 {
                    sum = sum + wave / number;
                }
                if h % 2 == 0 {
                    sum = sum - wave / number;
                }
            }
            if p.borrow()[1 as usize].clone() > 1.5f32 {
                if h % 4 == 1 {
                    sum = sum + wave / (number * number);
                }
                if h % 4 == 3 {
                    sum = sum - wave / (number * number);
                }
            }
            h = h + 1;
        }
    }
    if p.borrow()[1 as usize].clone() < 0.5f32 {
        return ((1.2732395 * (sum as f64)) as f32);
    }
    if p.borrow()[1 as usize].clone() < 1.5f32 {
        return ((0.6366198 * (sum as f64)) as f32);
    }
    return ((0.8105695 * (sum as f64)) as f32);
}

pub fn melody(i: i32, notes: Ref<Vec<f32>>, p: Ref<Vec<f32>>) -> f32 {
    let t: f32 = (i as f32) / p.borrow()[0 as usize].clone();
    let mut sum: f32 = 0.0f32;
    {
        let mut n: i64 = 0;
        while n < ((p.borrow()[1 as usize].clone() as i32) as i64) {
            let start: f32 = notes.borrow()[(3 * n) as usize].clone();
            let length: f32 = notes.borrow()[(3 * n + 1) as usize].clone();
            let local: f32 = t - start;
            if local >= 0.0f32 && local < length {
                let envelope: f32 = fade(local, 0.02f32, 0.08f32, length);
                let f: f32 = notes.borrow()[(3 * n + 2) as usize].clone();
                sum = sum + envelope * (sine(f * t) + ((0.5 * (sine(((2.0 * (f as f64) * (t as f64)) as f32)) as f64)) as f32) + ((0.25 * (sine(((3.0 * (f as f64) * (t as f64)) as f32)) as f64)) as f32));
            }
            n = n + 1;
        }
    }
    return p.borrow()[2 as usize].clone() * sum;
}
