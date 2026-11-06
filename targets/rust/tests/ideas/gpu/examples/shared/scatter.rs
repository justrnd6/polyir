use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn floorf(x: f32) -> f32 { x.floor() }
fn sinf(x: f32) -> f32 { x.sin() }
fn cosf(x: f32) -> f32 { x.cos() }

pub fn atomicAdd(counts: Ref<Vec<i32>>, slot: i32, amount: i32) -> i32 {
    let old: i32 = counts.borrow()[slot as usize].clone();
    let stored = old + amount;
    counts.borrow_mut()[slot as usize] = stored;
    return old;
}

pub fn reverseCopy(i: i32, src: Ref<Vec<f32>>, n: i32, dst: Ref<Vec<f32>>) -> i32 {
    let stored = src.borrow()[i as usize].clone();
    dst.borrow_mut()[(n - 1 - i) as usize] = stored;
    return 0;
}

pub fn histogram(i: i32, data: Ref<Vec<i32>>, bins: i32, counts: Ref<Vec<i32>>) -> i32 {
    let bin: i32 = data.borrow()[i as usize].clone() % bins;
    atomicAdd(counts.clone(), bin, 1);
    return 0;
}

pub fn histogramRacy(i: i32, data: Ref<Vec<i32>>, bins: i32, counts: Ref<Vec<i32>>) -> i32 {
    let bin: i32 = data.borrow()[i as usize].clone() % bins;
    let stored = counts.borrow()[bin as usize].clone() + 1;
    counts.borrow_mut()[bin as usize] = stored;
    return 0;
}

pub fn splat(i: i32, p: Ref<Vec<f32>>, cells: Ref<Vec<i32>>) -> i32 {
    let w: i32 = (p.borrow()[0 as usize].clone() as i32);
    let arm: i32 = i % 2;
    let t: f32 = ((i / 2 % 20000) as f32) / 20000.0f32;
    let angle: f32 = t * 9.0f32 + (arm as f32) * 3.14159265f32;
    let radius: f32 = ((0.08 + 0.85 * (t as f64)) as f32);
    let h1: i32 = (i * 75 + 74) % 65537;
    let h2: i32 = (h1 * 75 + 74) % 65537;
    let spread: f32 = ((0.015 + 0.09 * (t as f64)) as f32);
    let x: f32 = radius * cosf(angle) + ((h1 as f32) / 65537.0f32 - 0.5f32) * spread * 2.0f32;
    let y: f32 = radius * sinf(angle) + ((h2 as f32) / 65537.0f32 - 0.5f32) * spread * 2.0f32;
    let px: i32 = (floorf((x * 0.5f32 + 0.5f32) * (w as f32)) as i32);
    let py: i32 = (floorf((y * 0.5f32 + 0.5f32) * (w as f32)) as i32);
    if px >= 0 && px < w && py >= 0 && py < w {
        atomicAdd(cells.clone(), py * w + px, 1);
    }
    return 0;
}
