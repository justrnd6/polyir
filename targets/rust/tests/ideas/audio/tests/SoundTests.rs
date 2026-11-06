use std::rc::Rc;
use std::cell::RefCell;

#[path = "../../../core/tests/programs/Check.rs"]
mod Check;
use Check::{off, offInt};

#[path = "../Sound.rs"]
mod Sound;
use Sound::{Sound, soundNew};

#[path = "../Wave.rs"]
mod Wave;
use Wave::{sine};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn quietTone() -> Ref<Sound> {
    let s: Ref<Sound> = soundNew(0.1f32, 1000);
    s.tone(0, 125.0f32, 0.5f32, 0.0f32, 0.1f32);
    return s.clone();
}

pub fn checkNewSoundIsMono() -> f64 {
    return offInt(soundNew(1.0f32, 8000).borrow().channels, 1);
}

pub fn checkNewSoundLength() -> f64 {
    return offInt(soundNew(2.5f32, 8000).borrow().count, 20000);
}

pub fn checkSeconds() -> f64 {
    return off(soundNew(2.5f32, 8000).seconds(), 2.5f32);
}

pub fn checkTonePeak() -> f64 {
    return off(quietTone().peak(), 0.5f32);
}

pub fn checkSquareTonePeak() -> f64 {
    let s: Ref<Sound> = soundNew(0.1f32, 1000);
    s.tone(1, 125.0f32, 0.4f32, 0.0f32, 0.1f32);
    return off(s.peak(), 0.4f32);
}

pub fn checkScale() -> f64 {
    let s: Ref<Sound> = quietTone();
    s.scale(0.5f32);
    return off(s.peak(), 0.25f32);
}

pub fn checkMixAdds() -> f64 {
    let a: Ref<Sound> = quietTone();
    a.add(quietTone(), 0.0f32, 1.0f32);
    return off(a.peak(), 1.0f32);
}

pub fn checkNormalize() -> f64 {
    let s: Ref<Sound> = soundNew(0.1f32, 1000);
    s.tone(0, 125.0f32, 0.3f32, 0.0f32, 0.1f32);
    s.normalize(0.9f32);
    return off(s.peak(), 0.9f32);
}

pub fn checkDelayedMixKeepsLength() -> f64 {
    let a: Ref<Sound> = soundNew(1.0f32, 1000);
    a.add(quietTone(), 0.5f32, 1.0f32);
    return offInt(a.borrow().count, 1000);
}

pub fn checkDelayedMixIsSilentBefore() -> f64 {
    let a: Ref<Sound> = soundNew(1.0f32, 1000);
    a.add(quietTone(), 0.5f32, 1.0f32);
    return off(a.borrow().samples.clone().borrow()[499 as usize].clone(), 0.0f32);
}

pub fn checkDelayedMixStartsLater() -> f64 {
    let a: Ref<Sound> = soundNew(1.0f32, 1000);
    a.add(quietTone(), 0.5f32, 1.0f32);
    return off(a.borrow().samples.clone().borrow()[525 as usize].clone(), 0.35355339f32);
}

pub fn checkToneFadesIn() -> f64 {
    let s: Ref<Sound> = soundNew(0.5f32, 8000);
    s.tone(0, 440.0f32, 1.0f32, 0.0f32, 0.5f32);
    return off(s.borrow().samples.clone().borrow()[0 as usize].clone(), 0.0f32);
}

pub fn checkNoiseAmplitude() -> f64 {
    let s: Ref<Sound> = soundNew(0.1f32, 8000);
    s.noise(0.25f32, 7);
    let loudest: f32 = s.peak();
    if loudest > 0.25f32 {
        return 1.0;
    }
    if loudest < 0.2f32 {
        return 1.0;
    }
    return 0.0;
}

pub fn filterGain(kind: i64, hz: f32, cutoff: f32) -> f32 {
    let s: Ref<Sound> = soundNew(1.0f32, 8000);
    let data: Ref<Vec<f32>> = s.borrow().samples.clone();
    {
        let mut i: i64 = 0;
        while i < 8000 {
            let stored = sine(hz * (i as f32) / 8000.0f32);
            data.borrow_mut()[i as usize] = stored;
            i = i + 1;
        }
    }
    if kind == 0 {
        s.lowPass(cutoff);
    } else {
        s.highPass(cutoff);
    }
    let mut loudest: f32 = 0.0f32;
    {
        let mut i: i64 = 6000;
        while i < 8000 {
            let mut level: f32 = data.borrow()[i as usize].clone();
            if level < 0.0f32 {
                level = ((0.0 - (level as f64)) as f32);
            }
            if level > loudest {
                loudest = level;
            }
            i = i + 1;
        }
    }
    return loudest;
}

pub fn checkLowPassCutsHighTone() -> f64 {
    return off(filterGain(0, 1000.0f32, 500.0f32), 0.3987f32);
}

pub fn checkLowPassKeepsLowTone() -> f64 {
    return off(filterGain(0, 200.0f32, 500.0f32), 0.9031f32);
}

pub fn checkHighPassKeepsHighTone() -> f64 {
    return off(filterGain(1, 1000.0f32, 500.0f32), 0.7225f32);
}

pub fn checkHighPassCutsLowTone() -> f64 {
    return off(filterGain(1, 200.0f32, 500.0f32), 0.3613f32);
}

pub fn checkEchoFirstRepeat() -> f64 {
    let s: Ref<Sound> = soundNew(0.5f32, 1000);
    let stored = 1.0f32;
    s.borrow().samples.clone().borrow_mut()[0 as usize] = stored;
    s.echo(0.1f32, 0.5f32);
    return off(s.borrow().samples.clone().borrow()[100 as usize].clone(), 0.5f32);
}

pub fn checkEchoSecondRepeat() -> f64 {
    let s: Ref<Sound> = soundNew(0.5f32, 1000);
    let stored = 1.0f32;
    s.borrow().samples.clone().borrow_mut()[0 as usize] = stored;
    s.echo(0.1f32, 0.5f32);
    return off(s.borrow().samples.clone().borrow()[200 as usize].clone(), 0.25f32);
}

pub fn convolved() -> Ref<Sound> {
    let a: Ref<Sound> = soundNew(0.002f32, 1000);
    let r: Ref<Sound> = soundNew(0.003f32, 1000);
    let stored = 1.0f32;
    a.borrow().samples.clone().borrow_mut()[0 as usize] = stored;
    let stored = 2.0f32;
    a.borrow().samples.clone().borrow_mut()[1 as usize] = stored;
    let stored = 1.0f32;
    r.borrow().samples.clone().borrow_mut()[0 as usize] = stored;
    let stored = 3.0f32;
    r.borrow().samples.clone().borrow_mut()[2 as usize] = stored;
    return a.convolve(r.clone());
}

pub fn checkConvolutionLength() -> f64 {
    return offInt(convolved().borrow().count, 4);
}

pub fn checkConvolutionValues() -> f64 {
    let y: Ref<Vec<f32>> = convolved().borrow().samples.clone();
    return off(y.borrow()[0 as usize].clone(), 1.0f32) + off(y.borrow()[1 as usize].clone(), 2.0f32) + off(y.borrow()[2 as usize].clone(), 3.0f32) + off(y.borrow()[3 as usize].clone(), 6.0f32);
}

pub fn reverbOfImpulse() -> Ref<Sound> {
    let s: Ref<Sound> = soundNew(1.0f32, 10000);
    let stored = 1.0f32;
    s.borrow().samples.clone().borrow_mut()[0 as usize] = stored;
    s.reverb(1.0f32);
    return s.clone();
}

pub fn checkReverbStart() -> f64 {
    return off(reverbOfImpulse().borrow().samples.clone().borrow()[0 as usize].clone(), 0.20525f32);
}

pub fn checkReverbFirstEcho() -> f64 {
    return off(reverbOfImpulse().borrow().samples.clone().borrow()[297 as usize].clone(), 0.03924f32);
}
