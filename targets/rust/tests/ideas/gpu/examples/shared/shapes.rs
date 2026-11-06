use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sinf(x: f32) -> f32 { x.sin() }

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

pub fn blend7(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, g: f32, k: f32) -> f32 {
    return k * (a + ((2.0 * (b as f64)) as f32) + ((3.0 * (c as f64)) as f32)) + (d - e) * (f + g);
}

pub fn plasma(x: i32, y: i32, width: i32, height: i32, phase: f32) -> f32 {
    let u: f32 = (x as f32) / (width as f32);
    let v: f32 = (y as f32) / (height as f32);
    let a: f32 = sinf(((10.0 * (u as f64) + (phase as f64)) as f32));
    let b: f32 = sinf(((8.0 * (v as f64) - (phase as f64)) as f32));
    let c: f32 = sinf(((6.0 * ((u + v) as f64) + 2.0 * (phase as f64)) as f32));
    return ((0.5 + 0.1667 * ((a + b + c) as f64)) as f32);
}

pub fn vignette(pixel: f32, x: i32, y: i32, width: i32, height: i32) -> f32 {
    let dx: f32 = (x as f32) / (width as f32) - 0.5f32;
    let dy: f32 = (y as f32) / (height as f32) - 0.5f32;
    let edge: f32 = ((1.0 - 1.6 * ((dx * dx + dy * dy) as f64)) as f32);
    return pixel * edge;
}

pub fn tally(i: i32, counts: Ref<Vec<i32>>, sums: Ref<Vec<f32>>, lows: Ref<Vec<i32>>, highs: Ref<Vec<i32>>) -> i32 {
    let value: i32 = i * 7919 % 10007;
    atomicAdd(counts.clone(), i % 4, 1);
    atomicAddFloat(sums.clone(), i % 4, 0.5f32);
    atomicMin(lows.clone(), 0, value);
    atomicMax(highs.clone(), 0, value);
    return 0;
}

pub fn reported(i: i32) -> i32 {
    return i * 7919 % 10007;
}

pub fn nearly(a: f32, b: f32, tolerance: f32) -> i64 {
    let mut gap: f32 = a - b;
    if gap < 0.0f32 {
        gap = ((0.0 - (gap as f64)) as f32);
    }
    if gap <= tolerance {
        return 1;
    }
    return 0;
}
