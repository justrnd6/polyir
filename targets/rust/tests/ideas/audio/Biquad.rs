use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn sin(x: f64) -> f64 { x.sin() }
fn cos(x: f64) -> f64 { x.cos() }
fn pow(x: f64, y: f64) -> f64 { x.powf(y) }
fn sqrt(x: f64) -> f64 { x.sqrt() }

#[derive(Clone)]
pub struct Biquad {
    pub b0: f64,
    pub b1: f64,
    pub b2: f64,
    pub a1: f64,
    pub a2: f64,
}

impl PartialEq for Biquad {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.b0 == other.b0
            && self.b1 == other.b1
            && self.b2 == other.b2
            && self.a1 == other.a1
            && self.a2 == other.a2
    }
}

impl Biquad {
    pub fn new(b0: f64, b1: f64, b2: f64, a1: f64, a2: f64) -> Ref<Biquad> {
        share(Biquad { b0, b1, b2, a1, a2 })
    }
}

pub trait BiquadMethods {
    fn run(&self, data: Ref<Vec<f32>>, count: i64) -> i64;
    fn gain(&self, hz: f32, rate: i64) -> f32;
}

impl BiquadMethods for RefCell<Biquad> {
    fn run(&self, data: Ref<Vec<f32>>, count: i64) -> i64 {
        let mut x1: f64 = 0.0;
        let mut x2: f64 = 0.0;
        let mut y1: f64 = 0.0;
        let mut y2: f64 = 0.0;
        {
            let mut i: i64 = 0;
            while i < count {
                let x: f64 = (data.borrow()[i as usize].clone() as f64);
                let y: f64 = self.borrow().b0 * x + self.borrow().b1 * x1 + self.borrow().b2 * x2 - self.borrow().a1 * y1 - self.borrow().a2 * y2;
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
                let stored = (y as f32);
                data.borrow_mut()[i as usize] = stored;
                i = i + 1;
            }
        }
        return 0;
    }

    fn gain(&self, hz: f32, rate: i64) -> f32 {
        let w: f64 = 6.283185307179586 * (hz as f64) / (rate as f64);
        let c1: f64 = cos(w);
        let s1: f64 = sin(w);
        let c2: f64 = cos(2.0 * w);
        let s2: f64 = sin(2.0 * w);
        let topReal: f64 = self.borrow().b0 + self.borrow().b1 * c1 + self.borrow().b2 * c2;
        let topImag: f64 = 0.0 - (self.borrow().b1 * s1 + self.borrow().b2 * s2);
        let bottomReal: f64 = 1.0 + self.borrow().a1 * c1 + self.borrow().a2 * c2;
        let bottomImag: f64 = 0.0 - (self.borrow().a1 * s1 + self.borrow().a2 * s2);
        let strength: f64 = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
        return (sqrt(strength) as f32);
    }
}

pub fn biquadFrom(b0: f64, b1: f64, b2: f64, a0: f64, a1: f64, a2: f64) -> Ref<Biquad> {
    return Biquad::new(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

pub fn biquadAngle(rate: i64, hz: f32) -> f64 {
    return 6.283185307179586 * (hz as f64) / (rate as f64);
}

pub fn biquadLowPass(rate: i64, hz: f32, q: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let alpha: f64 = sin(w) / (2.0 * (q as f64));
    return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

pub fn biquadHighPass(rate: i64, hz: f32, q: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let alpha: f64 = sin(w) / (2.0 * (q as f64));
    return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

pub fn biquadBandPass(rate: i64, hz: f32, q: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let alpha: f64 = sin(w) / (2.0 * (q as f64));
    return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

pub fn biquadNotch(rate: i64, hz: f32, q: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let alpha: f64 = sin(w) / (2.0 * (q as f64));
    return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

pub fn biquadBell(rate: i64, hz: f32, q: f32, gainDb: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let alpha: f64 = sin(w) / (2.0 * (q as f64));
    let a: f64 = pow(10.0, (gainDb as f64) / 40.0);
    return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
}

pub fn biquadLowShelf(rate: i64, hz: f32, gainDb: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let a: f64 = pow(10.0, (gainDb as f64) / 40.0);
    let beta: f64 = sqrt(2.0 * a) * sin(w);
    return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
}

pub fn biquadHighShelf(rate: i64, hz: f32, gainDb: f32) -> Ref<Biquad> {
    let w: f64 = biquadAngle(rate, hz);
    let c: f64 = cos(w);
    let a: f64 = pow(10.0, (gainDb as f64) / 40.0);
    let beta: f64 = sqrt(2.0 * a) * sin(w);
    return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
}
