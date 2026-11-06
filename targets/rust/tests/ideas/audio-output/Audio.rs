use std::rc::Rc;
use std::cell::RefCell;

#[path = "Stream.rs"]
mod Stream;
use Stream::{Stream, soundStream, streamNew};

#[path = "macos.rs"]
mod macos;
use macos::{audioStreamRun};

#[path = "../audio/Sound.rs"]
mod Sound;
use Sound::{Sound};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn audioStreamAt(fill: fn(i64, Ref<Vec<f32>>, i64, i64) -> i64, state: i64, channels: i64, rate: i64, seconds: f32, volume: f32) -> i64 {
    return audioStreamRun(streamNew(fill.clone(), state, channels, rate, seconds, volume));
}

pub fn audioStream(fill: fn(i64, Ref<Vec<f32>>, i64, i64) -> i64, state: i64, channels: i64, rate: i64, seconds: f32) -> i64 {
    return audioStreamAt(fill.clone(), state, channels, rate, seconds, 1.0f32);
}

pub fn soundPlay(s: Ref<Sound>) -> i64 {
    return audioStreamRun(soundStream(s.clone(), 1.0f32));
}

pub fn audioPlay(samples: Ref<Vec<f32>>, count: i64, rate: i64) -> i64 {
    return soundPlay(Sound::new(samples.clone(), count, rate, 1));
}
