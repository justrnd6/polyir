#[path = "Check.rs"]
mod Check;
use Check::{off, offInt};

pub fn checkFloatRemainder() -> f64 {
    let a: f32 = 7.5f32;
    let b: f32 = 2.0f32;
    return off(a % b, 1.5f32);
}

pub fn checkFloatRemainderKeepsTheSignOfTheLeftNumber() -> f64 {
    let a: f32 = ((0.0 - 7.5) as f32);
    let b: f32 = 2.0f32;
    return off(a % b, ((0.0 - 1.5) as f32));
}

pub fn checkFloatRemainderIgnoresTheSignOfTheRightNumber() -> f64 {
    let a: f32 = 7.5f32;
    let b: f32 = ((0.0 - 2.0) as f32);
    return off(a % b, 1.5f32);
}

pub fn checkFloatRemainderOfNumbersThatDivideEvenly() -> f64 {
    let b: f32 = 47.0f32;
    let c: f32 = 97.0f32;
    return off(((2867.0 % (b as f64)) as f32), 0.0f32) + off(((5969.0 % (b as f64)) as f32), 0.0f32) + off(((9409.0 % (c as f64)) as f32), 0.0f32);
}

pub fn checkFloatRemainderOfZero() -> f64 {
    let a: f32 = 0.0f32;
    let b: f32 = 5.0f32;
    return off(a % b, 0.0f32);
}

pub fn checkFloatRemainderOfAFractionOfOne() -> f64 {
    let a: f32 = 0.75f32;
    let b: f32 = 1.0f32;
    return off(a % b, 0.75f32);
}

pub fn checkWholeNumberRemainder() -> f64 {
    let a: i64 = 17;
    let b: i64 = 5;
    return offInt(a % b, 2) + offInt((0 - a) % b, 0 - 2);
}
