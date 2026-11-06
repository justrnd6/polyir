#[path = "Check.rs"]
mod Check;
use Check::{off};

fn floorf(x: f32) -> f32 { x.floor() }

pub fn sign(x: f32) -> f32 {
    if x > 0.0f32 {
        return 1.0f32;
    }
    if x < 0.0f32 {
        return ((0.0 - 1.0) as f32);
    }
    return 0.0f32;
}

pub fn fract(x: f32) -> f32 {
    return x - floorf(x);
}

pub fn clamp(x: f32, low: f32, high: f32) -> f32 {
    if x < low {
        return low;
    }
    if x > high {
        return high;
    }
    return x;
}

pub fn mix(a: f32, b: f32, t: f32) -> f32 {
    return a + (b - a) * t;
}

pub fn checkSign() -> f64 {
    return off(sign(((0.0 - 3.0) as f32)), ((0.0 - 1.0) as f32)) + off(sign(0.0f32), 0.0f32) + off(sign(2.0f32), 1.0f32);
}

pub fn checkFract() -> f64 {
    return off(fract(2.75f32), 0.75f32) + off(fract(((0.0 - 0.25) as f32)), 0.75f32);
}

pub fn checkClamp() -> f64 {
    return off(clamp(5.0f32, ((0.0 - 1.0) as f32), 2.0f32), 2.0f32) + off(clamp(((0.0 - 5.0) as f32), ((0.0 - 1.0) as f32), 2.0f32), ((0.0 - 1.0) as f32));
}

pub fn checkMix() -> f64 {
    return off(mix(10.0f32, 20.0f32, 0.25f32), 12.5f32);
}

pub fn checkNestedCalls() -> f64 {
    return off(clamp(fract(1.75f32) * 2.0f32, ((0.0 - 1.0) as f32), 1.0f32), 1.0f32) + off(mix(sign(2.0f32), fract(0.5f32), 0.5f32), 0.75f32);
}

pub fn rescale(x: f32) -> f32 {
    return x * 3.0f32;
}

pub fn checkOwnFunctionsStillWork() -> f64 {
    return off(rescale(fract(1.25f32)), 0.75f32);
}
