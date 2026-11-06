use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn floorf(x: f32) -> f32 { x.floor() }

pub fn wrap(n: i32, size: i32, mode: i32) -> i32 {
    if mode == 0 {
        if n < 0 {
            return 0;
        }
        if n > size - 1 {
            return size - 1;
        }
        return n;
    }
    if mode == 1 {
        return (n % size + size) % size;
    }
    let period: i32 = size * 2;
    let m: i32 = (n % period + period) % period;
    if m >= size {
        return period - 1 - m;
    }
    return m;
}

pub fn texel(tex: Ref<Vec<f32>>, offset: i32, size: i32, x: i32, y: i32, mode: i32) -> f32 {
    return tex.borrow()[(offset + wrap(y, size, mode) * size + wrap(x, size, mode)) as usize].clone();
}

pub fn nearest(tex: Ref<Vec<f32>>, offset: i32, size: i32, u: f32, v: f32, mode: i32) -> f32 {
    let x: i32 = (floorf(u * (size as f32)) as i32);
    let y: i32 = (floorf(v * (size as f32)) as i32);
    return texel(tex.clone(), offset, size, x, y, mode);
}

pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    return a + (b - a) * t;
}

pub fn bilinear(tex: Ref<Vec<f32>>, offset: i32, size: i32, u: f32, v: f32, mode: i32) -> f32 {
    let sx: f32 = u * (size as f32) - 0.5f32;
    let sy: f32 = v * (size as f32) - 0.5f32;
    let x0: i32 = (floorf(sx) as i32);
    let y0: i32 = (floorf(sy) as i32);
    let fx: f32 = sx - (x0 as f32);
    let fy: f32 = sy - (y0 as f32);
    let top: f32 = lerp(texel(tex.clone(), offset, size, x0, y0, mode), texel(tex.clone(), offset, size, x0 + 1, y0, mode), fx);
    let bottom: f32 = lerp(texel(tex.clone(), offset, size, x0, y0 + 1, mode), texel(tex.clone(), offset, size, x0 + 1, y0 + 1, mode), fx);
    return lerp(top, bottom, fy);
}

pub fn levelOffset(base: i32, level: i32) -> i32 {
    let mut offset: i32 = 0;
    let mut size: i32 = base;
    {
        let mut k: i64 = 0;
        while k < (level as i64) {
            offset = offset + size * size;
            size = size / 2;
            k = k + 1;
        }
    }
    return offset;
}

pub fn levelSize(base: i32, level: i32) -> i32 {
    let mut size: i32 = base;
    {
        let mut k: i64 = 0;
        while k < (level as i64) {
            size = size / 2;
            k = k + 1;
        }
    }
    return size;
}

pub fn mipSample(tex: Ref<Vec<f32>>, base: i32, levels: i32, u: f32, v: f32, level: f32, mode: i32) -> f32 {
    let mut lo: i32 = (floorf(level) as i32);
    if lo < 0 {
        lo = 0;
    }
    if lo > levels - 1 {
        lo = levels - 1;
    }
    let mut hi: i32 = lo + 1;
    if hi > levels - 1 {
        hi = levels - 1;
    }
    let mut amount: f32 = level - (lo as f32);
    if amount < 0.0f32 {
        amount = 0.0f32;
    }
    if amount > 1.0f32 {
        amount = 1.0f32;
    }
    let a: f32 = bilinear(tex.clone(), levelOffset(base, lo), levelSize(base, lo), u, v, mode);
    let b: f32 = bilinear(tex.clone(), levelOffset(base, hi), levelSize(base, hi), u, v, mode);
    return lerp(a, b, amount);
}

pub fn pyramidSize(base: i64) -> i64 {
    let mut total: i64 = 0;
    let mut size: i64 = base;
    {
        let mut k: i64 = 0;
        while k < 16 {
            if size >= 1 {
                total = total + size * size;
                size = size / 2;
            }
            k = k + 1;
        }
    }
    return total;
}

pub fn fillBase(tex: Ref<Vec<f32>>, base: i64) -> i64 {
    {
        let mut y: i64 = 0;
        while y < base {
            {
                let mut x: i64 = 0;
                while x < base {
                    let mut level: f32 = if (x / 4 + y / 4) % 2 == 1 { 0.45f32 } else { 0.85f32 };
                    if x % 4 == 0 || y % 4 == 0 {
                        level = level * 0.6f32;
                    }
                    let stored = level;
                    tex.borrow_mut()[(y * base + x) as usize] = stored;
                    x = x + 1;
                }
            }
            y = y + 1;
        }
    }
    return 0;
}

pub fn buildPyramid(tex: Ref<Vec<f32>>, base: i64) -> i64 {
    fillBase(tex.clone(), base);
    let mut from: i64 = 0;
    let mut size: i64 = base;
    {
        let mut level: i64 = 0;
        while level < 16 {
            if size > 1 {
                shrink(tex.clone(), from, size);
                from = from + size * size;
                size = size / 2;
            }
            level = level + 1;
        }
    }
    return 0;
}

pub fn shrink(tex: Ref<Vec<f32>>, from: i64, size: i64) -> i64 {
    let next: i64 = from + size * size;
    let half: i64 = size / 2;
    {
        let mut y: i64 = 0;
        while y < half {
            {
                let mut x: i64 = 0;
                while x < half {
                    let a: f32 = tex.borrow()[(from + 2 * y * size + 2 * x) as usize].clone();
                    let b: f32 = tex.borrow()[(from + 2 * y * size + 2 * x + 1) as usize].clone();
                    let c: f32 = tex.borrow()[(from + (2 * y + 1) * size + 2 * x) as usize].clone();
                    let d: f32 = tex.borrow()[(from + (2 * y + 1) * size + 2 * x + 1) as usize].clone();
                    let stored = (a + b + c + d) * 0.25f32;
                    tex.borrow_mut()[(next + y * half + x) as usize] = stored;
                    x = x + 1;
                }
            }
            y = y + 1;
        }
    }
    return 0;
}
