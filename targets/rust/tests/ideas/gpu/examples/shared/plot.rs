use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn drawStems(pic: Ref<Vec<f32>>, w: i64, h: i64, values: Ref<Vec<f32>>, n: i64, low: f32, high: f32) -> i64 {
    {
        let mut k: i64 = 0;
        while k < w * h {
            let stored = 0.96f32;
            pic.borrow_mut()[k as usize] = stored;
            k = k + 1;
        }
    }
    let zero: i64 = ((high / (high - low) * (h as f32)) as i64);
    {
        let mut x: i64 = 0;
        while x < w {
            let stored = 0.55f32;
            pic.borrow_mut()[(zero * w + x) as usize] = stored;
            x = x + 1;
        }
    }
    {
        let mut j: i64 = 0;
        while j < n {
            let column: i64 = ((((j as f32) + 0.5f32) * (w as f32) / (n as f32)) as i64);
            let mut top: i64 = (((high - values.borrow()[j as usize].clone()) / (high - low) * ((h - 1) as f32)) as i64);
            if top < 0 {
                top = 0;
            }
            if top > h - 1 {
                top = h - 1;
            }
            let from: i64 = minOf(top, zero);
            let to: i64 = maxOf(top, zero);
            {
                let mut y: i64 = from;
                while y <= to {
                    let stored = 0.1f32;
                    pic.borrow_mut()[(y * w + column) as usize] = stored;
                    y = y + 1;
                }
            }
            {
                let mut dy: i64 = 0 - 1;
                while dy <= 1 {
                    {
                        let mut dx: i64 = 0 - 1;
                        while dx <= 1 {
                            let yy: i64 = top + dy;
                            let xx: i64 = column + dx;
                            if yy >= 0 && yy < h && xx >= 0 && xx < w {
                                let stored = 0.1f32;
                                pic.borrow_mut()[(yy * w + xx) as usize] = stored;
                            }
                            dx = dx + 1;
                        }
                    }
                    dy = dy + 1;
                }
            }
            j = j + 1;
        }
    }
    return 0;
}

pub fn minOf(a: i64, b: i64) -> i64 {
    if a < b {
        return a;
    }
    return b;
}

pub fn maxOf(a: i64, b: i64) -> i64 {
    if a > b {
        return a;
    }
    return b;
}

pub fn drawWave(pic: Ref<Vec<f32>>, w: i64, h: i64, values: Ref<Vec<f32>>, n: i64, low: f32, high: f32) -> i64 {
    {
        let mut k: i64 = 0;
        while k < w * h {
            let stored = 0.96f32;
            pic.borrow_mut()[k as usize] = stored;
            k = k + 1;
        }
    }
    let zero: i64 = ((high / (high - low) * (h as f32)) as i64);
    {
        let mut x: i64 = 0;
        while x < w {
            let stored = 0.55f32;
            pic.borrow_mut()[(zero * w + x) as usize] = stored;
            x = x + 1;
        }
    }
    {
        let mut x: i64 = 0;
        while x < w {
            let from: i64 = x * n / w;
            let mut upto: i64 = (x + 1) * n / w;
            if upto <= from {
                upto = from + 1;
            }
            let mut lowest: f32 = values.borrow()[from as usize].clone();
            let mut highest: f32 = values.borrow()[from as usize].clone();
            {
                let mut j: i64 = from;
                while j < upto {
                    if values.borrow()[j as usize].clone() < lowest {
                        lowest = values.borrow()[j as usize].clone();
                    }
                    if values.borrow()[j as usize].clone() > highest {
                        highest = values.borrow()[j as usize].clone();
                    }
                    j = j + 1;
                }
            }
            let mut top: i64 = (((high - highest) / (high - low) * ((h - 1) as f32)) as i64);
            let mut bottom: i64 = (((high - lowest) / (high - low) * ((h - 1) as f32)) as i64);
            if top < 0 {
                top = 0;
            }
            if bottom > h - 1 {
                bottom = h - 1;
            }
            {
                let mut y: i64 = top;
                while y <= bottom {
                    let stored = 0.1f32;
                    pic.borrow_mut()[(y * w + x) as usize] = stored;
                    y = y + 1;
                }
            }
            x = x + 1;
        }
    }
    return 0;
}

pub fn drawCurve(pic: Ref<Vec<f32>>, w: i64, h: i64, values: Ref<Vec<f32>>, n: i64, low: f32, high: f32) -> i64 {
    {
        let mut k: i64 = 0;
        while k < w * h {
            let stored = 0.96f32;
            pic.borrow_mut()[k as usize] = stored;
            k = k + 1;
        }
    }
    let zero: i64 = ((high / (high - low) * (h as f32)) as i64);
    {
        let mut x: i64 = 0;
        while x < w {
            let stored = 0.55f32;
            pic.borrow_mut()[(zero * w + x) as usize] = stored;
            x = x + 1;
        }
    }
    let mut previous: i64 = curveRow(values.borrow()[0 as usize].clone(), h, low, high);
    {
        let mut x: i64 = 0;
        while x < w {
            let row: i64 = curveRow(values.borrow()[(x * n / w) as usize].clone(), h, low, high);
            {
                let mut y: i64 = minOf(row, previous);
                while y <= maxOf(row, previous) {
                    let stored = 0.1f32;
                    pic.borrow_mut()[(y * w + x) as usize] = stored;
                    y = y + 1;
                }
            }
            previous = row;
            x = x + 1;
        }
    }
    return 0;
}

pub fn curveRow(value: f32, h: i64, low: f32, high: f32) -> i64 {
    let mut row: i64 = (((high - value) / (high - low) * ((h - 1) as f32)) as i64);
    if row < 0 {
        row = 0;
    }
    if row > h - 1 {
        row = h - 1;
    }
    return row;
}
