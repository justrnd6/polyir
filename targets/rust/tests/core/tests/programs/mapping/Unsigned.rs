use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

#[derive(Clone)]
pub struct Pixel {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl PartialEq for Pixel {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.r == other.r
            && self.g == other.g
            && self.b == other.b
    }
}

impl Pixel {
    pub fn new(r: u8, g: u8, b: u8) -> Ref<Pixel> {
        share(Pixel { r, g, b })
    }
}

pub trait PixelMethods {
}

impl PixelMethods for RefCell<Pixel> {
}

pub fn comparison() -> i64 {
    let big: u32 = 4000000000;
    if big > 3000000000 && big >= big && (big < 5) == false {
        return 1;
    }
    return 0;
}

pub fn division() -> i64 {
    let big: u32 = 4000000000;
    let half: u32 = big / 2;
    let rest: u32 = big % 7;
    if (half as i64) == 2000000000 && (rest as i64) == 3 {
        return 1;
    }
    return 0;
}

pub fn shifting() -> i64 {
    let big: u32 = 4000000000;
    let top: u64 = ((0 - 1) as u64);
    if ((big >> 28) as i64) == 14 && ((top >> 63) as i64) == 1 {
        return 1;
    }
    return 0;
}

pub fn widening() -> i64 {
    let big: u32 = 4000000000;
    let small: u8 = 200;
    if (big as i64) == 4000000000 && (small as i64) == 200 {
        return 1;
    }
    return 0;
}

pub fn wrapping() -> i64 {
    let a: u8 = 200;
    let b: u8 = 100;
    let sum: u8 = a + b;
    let under: u8 = b - a;
    let product: u16 = 300;
    let wide: u16 = product * 300;
    if (sum as i64) == 44 && (under as i64) == 156 && (wide as i64) == 24464 {
        return 1;
    }
    return 0;
}

pub fn floats() -> i64 {
    let big: u32 = 4000000000;
    if ((big as f64) as i64) == 4000000000 {
        return 1;
    }
    return 0;
}

pub fn largest() -> i64 {
    let top: u64 = ((0 - 1) as u64);
    if top > 5 && top / 2 == 9223372036854775807 {
        return 1;
    }
    return 0;
}

pub fn fnv(n: i64) -> u32 {
    let mut h: u32 = 2166136261;
    {
        let mut i: i64 = 0;
        while i < n {
            h = (h ^ (i as u32)) * 16777619;
            i = i + 1;
        }
    }
    return h;
}

pub fn hashing() -> i64 {
    if (fnv(3) as i64) == 581859880 {
        return 1;
    }
    return 0;
}

pub fn fields() -> i64 {
    let a: Ref<Pixel> = Pixel::new(250, 128, 7);
    let b: Ref<Pixel> = Pixel::new(250, 128, 7);
    if a.clone() == b.clone() && (a.borrow().g as i64) == 128 && a.borrow().r > a.borrow().b {
        return 1;
    }
    return 0;
}

pub fn keys() -> i64 {
    let m: Ref<HashMap<u32, i64>> = share(HashMap::new());
    let k: u32 = 4000000000;
    let stored = 7;
    m.borrow_mut().insert(k, stored);
    let stored = 9;
    m.borrow_mut().insert(5, stored);
    if m.borrow().get(&k).cloned().unwrap_or_else(|| 0) == 7 && m.borrow().get(&5).cloned().unwrap_or_else(|| 0) == 9 {
        return 1;
    }
    return 0;
}

pub fn main_() -> i64 {
    return comparison() + division() + shifting() + widening() + wrapping() + floats() + largest() + hashing() + fields() + keys();
}

fn main() {
    std::process::exit(main_() as i32);
}
