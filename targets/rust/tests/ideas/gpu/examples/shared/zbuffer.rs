use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn fminf(x: f32, y: f32) -> f32 { x.min(y) }
fn fmaxf(x: f32, y: f32) -> f32 { x.max(y) }
fn floorf(x: f32) -> f32 { x.floor() }
fn ceilf(x: f32) -> f32 { x.ceil() }

pub fn atomicMin(values: Ref<Vec<i32>>, slot: i32, candidate: i32) -> i32 {
    let old: i32 = values.borrow()[slot as usize].clone();
    if candidate < old {
        let stored = candidate;
        values.borrow_mut()[slot as usize] = stored;
    }
    return old;
}

pub fn emptyKey() -> i32 {
    return 2147483647;
}

pub fn rasterize(t: i32, tris: Ref<Vec<f32>>, width: i32, height: i32, depth: Ref<Vec<i32>>) -> i32 {
    let base: i32 = t * 10;
    let x0: f32 = tris.borrow()[base as usize].clone();
    let y0: f32 = tris.borrow()[(base + 1) as usize].clone();
    let z0: f32 = tris.borrow()[(base + 2) as usize].clone();
    let x1: f32 = tris.borrow()[(base + 3) as usize].clone();
    let y1: f32 = tris.borrow()[(base + 4) as usize].clone();
    let z1: f32 = tris.borrow()[(base + 5) as usize].clone();
    let x2: f32 = tris.borrow()[(base + 6) as usize].clone();
    let y2: f32 = tris.borrow()[(base + 7) as usize].clone();
    let z2: f32 = tris.borrow()[(base + 8) as usize].clone();
    let area: f32 = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    let left: i32 = (fmaxf(0.0f32, floorf(fminf(x0, fminf(x1, x2)))) as i32);
    let right: i32 = (fminf((width as f32) - 1.0f32, ceilf(fmaxf(x0, fmaxf(x1, x2)))) as i32);
    let top: i32 = (fmaxf(0.0f32, floorf(fminf(y0, fminf(y1, y2)))) as i32);
    let bottom: i32 = (fminf((height as f32) - 1.0f32, ceilf(fmaxf(y0, fmaxf(y1, y2)))) as i32);
    {
        let mut y: i32 = top;
        while y <= bottom {
            {
                let mut x: i32 = left;
                while x <= right {
                    let px: f32 = (x as f32) + 0.5f32;
                    let py: f32 = (y as f32) + 0.5f32;
                    let w0: f32 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
                    let w1: f32 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
                    let w2: f32 = ((1.0 - (w0 as f64) - (w1 as f64)) as f32);
                    if w0 >= 0.0f32 && w1 >= 0.0f32 && w2 >= 0.0f32 {
                        let z: f32 = w0 * z0 + w1 * z1 + w2 * z2;
                        atomicMin(depth.clone(), y * width + x, ((z * 1048575.0f32) as i32) * 512 + t);
                    }
                    x = x + 1;
                }
            }
            y = y + 1;
        }
    }
    return 0;
}

pub fn shade(p: i32, depth: Ref<Vec<i32>>, tris: Ref<Vec<f32>>) -> f32 {
    let key: i32 = depth.borrow()[p as usize].clone();
    if key == 2147483647 {
        return 0.06f32;
    }
    let id: i32 = key % 512;
    let z: f32 = ((key / 512) as f32) / 1048575.0f32;
    return tris.borrow()[(id * 10 + 9) as usize].clone() * ((1.0 - 0.45 * (z as f64)) as f32);
}

pub fn makeScene(tris: Ref<Vec<f32>>, count: i64, width: i64, height: i64) -> i64 {
    let mut seed: i64 = 12345;
    {
        let mut t: i64 = 0;
        while t < count {
            let cx: f32 = nextRandom(seed) * (width as f32);
            seed = nextSeed(seed);
            let cy: f32 = nextRandom(seed) * (height as f32);
            seed = nextSeed(seed);
            let size: f32 = ((30.0 + ((nextRandom(seed) * 110.0f32) as f64)) as f32);
            seed = nextSeed(seed);
            {
                let mut corner: i64 = 0;
                while corner < 3 {
                    let stored = cx + (nextRandom(seed) - 0.5f32) * 2.0f32 * size;
                    tris.borrow_mut()[(t * 10 + corner * 3) as usize] = stored;
                    seed = nextSeed(seed);
                    let stored = cy + (nextRandom(seed) - 0.5f32) * 2.0f32 * size;
                    tris.borrow_mut()[(t * 10 + corner * 3 + 1) as usize] = stored;
                    seed = nextSeed(seed);
                    let stored = nextRandom(seed);
                    tris.borrow_mut()[(t * 10 + corner * 3 + 2) as usize] = stored;
                    seed = nextSeed(seed);
                    corner = corner + 1;
                }
            }
            let stored = ((0.25 + 0.75 * (nextRandom(seed) as f64)) as f32);
            tris.borrow_mut()[(t * 10 + 9) as usize] = stored;
            seed = nextSeed(seed);
            t = t + 1;
        }
    }
    return 0;
}

pub fn nextSeed(seed: i64) -> i64 {
    return (seed * 1103515245 + 12345) % 2147483648;
}

pub fn nextRandom(seed: i64) -> f32 {
    return ((seed / 65536 % 32768) as f32) / 32768.0f32;
}
