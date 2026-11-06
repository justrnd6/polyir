#[path = "../../../core/stdlib/Math.rs"]
mod Math;
use Math::{clamp, fract, mix, sign};

#[path = "../../../core/tests/programs/Check.rs"]
mod Check;
use Check::{off, offInt};

#[path = "../Wave.rs"]
mod Wave;
use Wave::{fade, noise, noteFrequency, sawtooth, sine, square, triangle};

pub fn checkSignOfNegative() -> f64 {
    return off(sign(((0.0 - 3.0) as f32)), ((0.0 - 1.0) as f32));
}

pub fn checkSignOfZero() -> f64 {
    return off(sign(0.0f32), 0.0f32);
}

pub fn checkSignOfPositive() -> f64 {
    return off(sign(2.0f32), 1.0f32);
}

pub fn checkFractOfPositive() -> f64 {
    return off(fract(2.75f32), 0.75f32);
}

pub fn checkFractOfNegative() -> f64 {
    return off(fract(((0.0 - 0.25) as f32)), 0.75f32);
}

pub fn checkClampAbove() -> f64 {
    return off(clamp(5.0f32, ((0.0 - 1.0) as f32), 2.0f32), 2.0f32);
}

pub fn checkClampBelow() -> f64 {
    return off(clamp(((0.0 - 5.0) as f32), ((0.0 - 1.0) as f32), 2.0f32), ((0.0 - 1.0) as f32));
}

pub fn checkMix() -> f64 {
    return off(mix(10.0f32, 20.0f32, 0.25f32), 12.5f32);
}

pub fn checkSinePeak() -> f64 {
    return off(sine(0.25f32), 1.0f32);
}

pub fn checkSineTrough() -> f64 {
    return off(sine(0.75f32), ((0.0 - 1.0) as f32));
}

pub fn checkSineZeroCrossing() -> f64 {
    return off(sine(1.5f32), 0.0f32);
}

pub fn checkSquareHigh() -> f64 {
    return off(square(0.25f32), 1.0f32);
}

pub fn checkSquareLow() -> f64 {
    return off(square(0.75f32), ((0.0 - 1.0) as f32));
}

pub fn checkSawtoothStart() -> f64 {
    return off(sawtooth(0.0f32), ((0.0 - 1.0) as f32));
}

pub fn checkSawtoothThreeQuarters() -> f64 {
    return off(sawtooth(0.75f32), 0.5f32);
}

pub fn checkTriangleStart() -> f64 {
    return off(triangle(0.0f32), ((0.0 - 1.0) as f32));
}

pub fn checkTrianglePeak() -> f64 {
    return off(triangle(0.5f32), 1.0f32);
}

pub fn checkTriangleMiddle() -> f64 {
    return off(triangle(0.25f32), 0.0f32);
}

pub fn checkConcertA() -> f64 {
    return off(noteFrequency(69.0f32), 440.0f32);
}

pub fn checkOctaveAboveConcertA() -> f64 {
    return off(noteFrequency(81.0f32), 880.0f32);
}

pub fn checkFadeInHalfway() -> f64 {
    return off(fade(0.5f32, 1.0f32, 1.0f32, 4.0f32), 0.5f32);
}

pub fn checkFadeHolds() -> f64 {
    return off(fade(2.0f32, 1.0f32, 1.0f32, 4.0f32), 1.0f32);
}

pub fn checkFadeOutHalfway() -> f64 {
    return off(fade(3.5f32, 1.0f32, 1.0f32, 4.0f32), 0.5f32);
}

pub fn checkNoiseRange() -> f64 {
    let mut outside: i64 = 0;
    {
        let mut seed: i64 = 0;
        while seed < 1000 {
            if noise((seed as i32)) > 1.0f32 || noise((seed as i32)) < ((0.0 - 1.0) as f32) {
                outside = outside + 1;
            }
            seed = seed + 1;
        }
    }
    return offInt(outside, 0);
}

pub fn checkNoiseRepeats() -> f64 {
    return off(noise(42) - noise(42), 0.0f32);
}

pub fn checkNoiseVaries() -> f64 {
    if noise(1) == noise(2) && noise(2) == noise(3) {
        return 1.0;
    }
    return 0.0;
}
