use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

#[path = "Check.rs"]
mod Check;
use Check::{offInt};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign() -> f64 {
    let big: u32 = 4000000000;
    let mut seen: i64 = 0;
    if big > 3000000000 {
        seen = seen + 1;
    }
    if big >= big {
        seen = seen + 10;
    }
    if big < 5 {
        seen = seen + 100;
    }
    return offInt(seen, 11);
}

pub fn checkAnUnsignedDivisionAndRemainder() -> f64 {
    let big: u32 = 4000000000;
    let half: u32 = big / 2;
    let rest: u32 = big % 7;
    return offInt((half as i64), 2000000000) + offInt((rest as i64), 3);
}

pub fn checkAnUnsignedRightShiftFillsWithZeros() -> f64 {
    let big: u32 = 4000000000;
    let shifted: u32 = big >> 28;
    let m: u64 = ((0 - 1) as u64);
    return offInt((shifted as i64), 14) + offInt(((m >> 63) as i64), 1);
}

pub fn checkAnUnsignedWidensWithZeroBits() -> f64 {
    let big: u32 = 4000000000;
    let small: u8 = 200;
    return offInt((big as i64), 4000000000) + offInt((small as i64), 200);
}

pub fn checkNarrowUnsignedArithmeticWrapsAtItsSize() -> f64 {
    let b: u8 = 200;
    let c: u8 = 100;
    let sum: u8 = b + c;
    let under: u8 = c - b;
    return offInt((sum as i64), 44) + offInt((under as i64), 156);
}

pub fn checkAnUnsignedGoesToAFloatAsItsValue() -> f64 {
    let big: u32 = 4000000000;
    let asDouble: f64 = (big as f64);
    return offInt((asDouble as i64), 4000000000);
}

pub fn checkTheLargestU64IsLargerThanFive() -> f64 {
    let m: u64 = ((0 - 1) as u64);
    let mut seen: i64 = 0;
    if m > 5 {
        seen = seen + 1;
    }
    if m / 2 == 9223372036854775807 {
        seen = seen + 10;
    }
    return offInt(seen, 11);
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

pub fn checkAnUnsignedFieldKeepsItsValueAndStructsCompare() -> f64 {
    let a: Ref<Pixel> = Pixel::new(250, 128, 7);
    let b: Ref<Pixel> = Pixel::new(250, 128, 7);
    let mut seen: i64 = 0;
    if a.clone() == b.clone() {
        seen = seen + 1;
    }
    seen = seen + (a.borrow().g as i64);
    return offInt(seen, 129);
}

pub fn checkUnsignedKeysAreOrdinaryKeys() -> f64 {
    let m: Ref<HashMap<u32, i64>> = share(HashMap::new());
    let k: u32 = 4000000000;
    let stored = 7;
    m.borrow_mut().insert(k, stored);
    let stored = 9;
    m.borrow_mut().insert(5, stored);
    return offInt(m.borrow().get(&k).cloned().unwrap_or_else(|| 0), 7) + offInt(m.borrow().get(&5).cloned().unwrap_or_else(|| 0), 9);
}
