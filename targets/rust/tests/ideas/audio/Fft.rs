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
fn logf(x: f32) -> f32 { x.ln() }

pub fn fftStage(t: i32, src: Ref<Vec<f32>>, p: Ref<Vec<i32>>, dst: Ref<Vec<f32>>) -> i32 {
    let total: i32 = p.borrow()[0 as usize].clone();
    let n: i32 = p.borrow()[1 as usize].clone();
    let s: i32 = p.borrow()[2 as usize].clone();
    let half: i32 = total / 2;
    let batch: i32 = t / half;
    let local: i32 = t % half;
    let position: i32 = local / s;
    let column: i32 = local % s;
    let base: i32 = batch * total * 2;
    let a: i32 = base + ((2 * ((column + s * position) as i64)) as i32);
    let b: i32 = base + ((2 * ((column + s * (position + n / 2)) as i64)) as i32);
    let sumRe: f32 = src.borrow()[a as usize].clone() + src.borrow()[b as usize].clone();
    let sumIm: f32 = src.borrow()[(a + 1) as usize].clone() + src.borrow()[(b + 1) as usize].clone();
    let diffRe: f32 = src.borrow()[a as usize].clone() - src.borrow()[b as usize].clone();
    let diffIm: f32 = src.borrow()[(a + 1) as usize].clone() - src.borrow()[(b + 1) as usize].clone();
    let angle: f32 = (((0.0 - 6.2831853) * ((p.borrow()[3 as usize].clone() as f32) as f64) * ((position as f32) as f64) / ((n as f32) as f64)) as f32);
    let twiddleRe: f32 = cosf(angle);
    let twiddleIm: f32 = sinf(angle);
    let first: i32 = base + ((2 * ((column + s * ((2 * (position as i64)) as i32)) as i64)) as i32);
    let second: i32 = base + ((2 * ((column + s * ((2 * (position as i64) + 1) as i32)) as i64)) as i32);
    let stored = sumRe;
    dst.borrow_mut()[first as usize] = stored;
    let stored = sumIm;
    dst.borrow_mut()[(first + 1) as usize] = stored;
    let stored = diffRe * twiddleRe - diffIm * twiddleIm;
    dst.borrow_mut()[second as usize] = stored;
    let stored = diffRe * twiddleIm + diffIm * twiddleRe;
    dst.borrow_mut()[(second + 1) as usize] = stored;
    return 0;
}

pub fn fftMagnitude(k: i32, data: Ref<Vec<f32>>, p: Ref<Vec<i32>>) -> f32 {
    let total: i32 = p.borrow()[0 as usize].clone();
    let re: f32 = data.borrow()[(2 * (k as i64)) as usize].clone();
    let im: f32 = data.borrow()[(2 * (k as i64) + 1) as usize].clone();
    return ((2.0 * (sqrtf(re * re + im * im) as f64) / ((total as f32) as f64)) as f32);
}

pub fn fftFrames(t: i32, signal: Ref<Vec<f32>>, p: Ref<Vec<i32>>, dst: Ref<Vec<f32>>) -> i32 {
    let total: i32 = p.borrow()[0 as usize].clone();
    let frame: i32 = t / total;
    let k: i32 = t % total;
    let window: f32 = ((0.5 - 0.5 * (cosf(((6.2831853 * ((k as f32) as f64) / (((total - 1) as f32) as f64)) as f32)) as f64)) as f32);
    let stored = signal.borrow()[(frame * p.borrow()[1 as usize].clone() + k) as usize].clone() * window;
    dst.borrow_mut()[(2 * (t as i64)) as usize] = stored;
    let stored = 0.0f32;
    dst.borrow_mut()[(2 * (t as i64) + 1) as usize] = stored;
    return 0;
}

pub fn spectrogramPixel(i: i32, data: Ref<Vec<f32>>, p: Ref<Vec<i32>>) -> f32 {
    let total: i32 = p.borrow()[0 as usize].clone();
    let frames: i32 = p.borrow()[1 as usize].clone();
    let rows: i32 = p.borrow()[2 as usize].clone();
    let x: i32 = i % frames;
    let bin: i32 = rows - 1 - i / frames;
    let at: i32 = x * total * 2 + ((2 * (bin as i64)) as i32);
    let re: f32 = data.borrow()[at as usize].clone();
    let im: f32 = data.borrow()[(at + 1) as usize].clone();
    let magnitude: f32 = ((4.0 * (sqrtf(re * re + im * im) as f64) / ((total as f32) as f64)) as f32);
    let mut level: f32 = (((20.0 * (logf(magnitude + 0.0000001f32) as f64) / 2.3025851 + 70.0) / 70.0) as f32);
    if level < 0.0f32 {
        level = 0.0f32;
    }
    if level > 1.0f32 {
        level = 1.0f32;
    }
    return level;
}

pub fn complexMultiply(k: i32, a: Ref<Vec<f32>>, b: Ref<Vec<f32>>, out: Ref<Vec<f32>>) -> i32 {
    let stored = a.borrow()[(2 * (k as i64)) as usize].clone() * b.borrow()[(2 * (k as i64)) as usize].clone() - a.borrow()[(2 * (k as i64) + 1) as usize].clone() * b.borrow()[(2 * (k as i64) + 1) as usize].clone();
    out.borrow_mut()[(2 * (k as i64)) as usize] = stored;
    let stored = a.borrow()[(2 * (k as i64)) as usize].clone() * b.borrow()[(2 * (k as i64) + 1) as usize].clone() + a.borrow()[(2 * (k as i64) + 1) as usize].clone() * b.borrow()[(2 * (k as i64)) as usize].clone();
    out.borrow_mut()[(2 * (k as i64) + 1) as usize] = stored;
    return 0;
}

pub fn realScaled(i: i32, data: Ref<Vec<f32>>, p: Ref<Vec<f32>>) -> f32 {
    return data.borrow()[(2 * (i as i64)) as usize].clone() / p.borrow()[0 as usize].clone();
}
