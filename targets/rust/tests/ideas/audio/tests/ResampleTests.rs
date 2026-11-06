use std::rc::Rc;
use std::cell::RefCell;

#[path = "../../../core/tests/programs/Check.rs"]
mod Check;
use Check::{amplitudeAt, off, offInt};

#[path = "../Resample.rs"]
mod Resample;
use Resample::{resampledCount};

#[path = "../Sound.rs"]
mod Sound;
use Sound::{Sound, soundNew, soundStereo};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sin(x: f64) -> f64 { x.sin() }

pub fn sineSound(hz: f32, seconds: f32, rate: i64) -> Ref<Sound> {
    let s: Ref<Sound> = soundNew(seconds, rate);
    {
        let mut i: i64 = 0;
        while i < s.borrow().count {
            let stored = (sin(6.283185307179586 * (hz as f64) * (i as f64) / (rate as f64)) as f32);
            s.borrow().samples.clone().borrow_mut()[i as usize] = stored;
            i = i + 1;
        }
    }
    return s.clone();
}

pub fn checkCountGoingUp() -> f64 {
    return offInt(resampledCount(44100, 44100, 48000), 48000);
}

pub fn checkCountGoingDown() -> f64 {
    return offInt(resampledCount(1000, 8000, 4000), 500);
}

pub fn checkCountRounds() -> f64 {
    return offInt(resampledCount(10, 3, 4), 13);
}

pub fn checkSoundKeepsItsLength() -> f64 {
    let s: Ref<Sound> = sineSound(440.0f32, 0.5f32, 8000).resample(12000);
    return offInt(s.borrow().rate, 12000) + offInt(s.borrow().count, 6000) + offInt(s.borrow().channels, 1) + off(s.seconds(), 0.5f32);
}

pub fn checkUpsamplingFollowsTheWave() -> f64 {
    let s: Ref<Sound> = sineSound(440.0f32, 1.0f32, 8000).resample(12000);
    let mut worst: f64 = 0.0;
    {
        let mut j: i64 = 1000;
        while j < 11000 {
            let expected: f64 = sin(6.283185307179586 * 440.0 * (j as f64) / 12000.0);
            let mut gap: f64 = (s.borrow().samples.clone().borrow()[j as usize].clone() as f64) - expected;
            if gap < 0.0 {
                gap = 0.0 - gap;
            }
            if gap > worst {
                worst = gap;
            }
            j = j + 7;
        }
    }
    return worst;
}

pub fn checkDownsamplingRemovesAliases() -> f64 {
    let s: Ref<Sound> = sineSound(3000.0f32, 1.0f32, 8000).resample(4000);
    return (amplitudeAt(s.borrow().samples.clone(), 500, 2000, 4000, 1000.0f32) as f64);
}

pub fn checkDownsamplingKeepsLowTones() -> f64 {
    let s: Ref<Sound> = sineSound(1000.0f32, 1.0f32, 8000).resample(4000);
    return off(amplitudeAt(s.borrow().samples.clone(), 500, 2000, 4000, 1000.0f32), 1.0f32);
}

pub fn checkRoundTrip() -> f64 {
    let original: Ref<Sound> = sineSound(300.0f32, 1.0f32, 8000);
    let back: Ref<Sound> = original.resample(12000).resample(8000);
    let mut worst: f64 = 0.0;
    {
        let mut j: i64 = 1000;
        while j < 7000 {
            let mut gap: f64 = ((back.borrow().samples.clone().borrow()[j as usize].clone() - original.borrow().samples.clone().borrow()[j as usize].clone()) as f64);
            if gap < 0.0 {
                gap = 0.0 - gap;
            }
            if gap > worst {
                worst = gap;
            }
            j = j + 11;
        }
    }
    return worst;
}

pub fn checkConstantStaysConstant() -> f64 {
    let s: Ref<Sound> = soundNew(0.5f32, 8000);
    {
        let mut i: i64 = 0;
        while i < s.borrow().count {
            let stored = 1.0f32;
            s.borrow().samples.clone().borrow_mut()[i as usize] = stored;
            i = i + 1;
        }
    }
    let up: Ref<Sound> = s.resample(22050);
    let down: Ref<Sound> = s.resample(3000);
    return off(up.borrow().samples.clone().borrow()[(up.borrow().count / 2) as usize].clone(), 1.0f32) + off(down.borrow().samples.clone().borrow()[(down.borrow().count / 2) as usize].clone(), 1.0f32);
}

pub fn checkStereoResampling() -> f64 {
    let left: Ref<Sound> = sineSound(440.0f32, 0.5f32, 8000);
    let right: Ref<Sound> = sineSound(880.0f32, 0.5f32, 8000);
    let out: Ref<Sound> = soundStereo(left.clone(), right.clone()).resample(16000);
    return offInt(out.borrow().channels, 2) + offInt(out.borrow().count, 8000) + off(amplitudeAt(out.channel(0).borrow().samples.clone(), 2000, 4000, 16000, 440.0f32), 1.0f32) + off(amplitudeAt(out.channel(1).borrow().samples.clone(), 2000, 4000, 16000, 880.0f32), 1.0f32);
}

pub fn checkSameRateIsACopy() -> f64 {
    let s: Ref<Sound> = sineSound(440.0f32, 0.1f32, 8000);
    let copy: Ref<Sound> = s.resample(8000);
    let mut worst: f64 = 0.0;
    {
        let mut i: i64 = 100;
        while i < 700 {
            let mut gap: f64 = ((copy.borrow().samples.clone().borrow()[i as usize].clone() - s.borrow().samples.clone().borrow()[i as usize].clone()) as f64);
            if gap < 0.0 {
                gap = 0.0 - gap;
            }
            if gap > worst {
                worst = gap;
            }
            i = i + 1;
        }
    }
    return worst + offInt(copy.borrow().count, s.borrow().count);
}
