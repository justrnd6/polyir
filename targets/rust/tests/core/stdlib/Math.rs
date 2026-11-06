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
