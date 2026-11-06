#[path = "../../core/stdlib/Math.rs"]
mod Math;
use Math::{clamp, fract};

fn sinf(x: f32) -> f32 { x.sin() }
fn fabsf(x: f32) -> f32 { x.abs() }
fn powf(x: f32, y: f32) -> f32 { x.powf(y) }

pub fn sine(phase: f32) -> f32 {
    return sinf(((6.2831853 * (fract(phase) as f64)) as f32));
}

pub fn square(phase: f32) -> f32 {
    if fract(phase) < 0.5f32 {
        return 1.0f32;
    }
    return ((0.0 - 1.0) as f32);
}

pub fn sawtooth(phase: f32) -> f32 {
    return ((2.0 * (fract(phase) as f64) - 1.0) as f32);
}

pub fn triangle(phase: f32) -> f32 {
    return ((1.0 - 4.0 * (fabsf(fract(phase) - 0.5f32) as f64)) as f32);
}

pub fn noise(seed: i32) -> f32 {
    let a: i32 = (seed * 75 + 74) % 65537;
    let b: i32 = (a * 75 + 74) % 65537;
    return ((2.0 * ((b as f32) as f64) / 65537.0 - 1.0) as f32);
}

pub fn fade(t: f32, attack: f32, release: f32, total: f32) -> f32 {
    let mut level: f32 = if t < attack { t / attack } else { 1.0f32 };
    if total - t < release {
        level = level * clamp((total - t) / release, 0.0f32, 1.0f32);
    }
    return level;
}

pub fn noteFrequency(m: f32) -> f32 {
    return ((440.0 * (powf(2.0f32, (m - 69.0f32) / 12.0f32) as f64)) as f32);
}
