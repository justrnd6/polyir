#[path = "../Wave.rs"]
mod Wave;
use Wave::{fade, noteFrequency, sine};

pub fn concertAHeight() -> i64 {
    let quarterCycle: f32 = ((0.25 / (noteFrequency(69.0f32) as f64)) as f32);
    return ((sine(noteFrequency(69.0f32) * quarterCycle) * 1000.0f32) as i64);
}

pub fn softAttack() -> i64 {
    return ((fade(0.5f32, 1.0f32, 1.0f32, 4.0f32) * 1000.0f32) as i64);
}

pub fn chordSample(t: f32) -> f32 {
    return ((0.3 * ((sine(noteFrequency(60.0f32) * t) + sine(noteFrequency(64.0f32) * t) + sine(noteFrequency(67.0f32) * t)) as f64)) as f32);
}
