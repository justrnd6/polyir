use std::rc::Rc;

// A variant: the number of its case and the value it holds, of whatever type that case holds.
#[derive(Clone)]
pub struct Variant {
    pub tag: i64,
    pub value: Rc<dyn std::any::Any>,
}

impl Variant {
    pub fn new<T: 'static>(tag: i64, value: T) -> Variant {
        Variant { tag, value: Rc::new(value) }
    }

    pub fn get<T: Clone + 'static>(&self) -> T {
        self.value.downcast_ref::<T>().expect("the variant holds another case").clone()
    }
}

pub fn safeDivide(a: i64, b: i64) -> Result<i64, i64> {
    if b == 0 {
        return Err(99);
    }
    return Ok(a / b);
}

pub fn divideOrDefault(a: i64, b: i64) -> i64 {
    let r: Result<i64, i64> = safeDivide(a, b);
    if r.is_err() {
        return r.clone().err().unwrap();
    }
    return r.clone().ok().unwrap();
}

pub fn eitherCoercion() -> i64 {
    let r: Result<f64, i64> = Ok(3.0);
    return (r.clone().ok().unwrap() as i64);
}

pub fn variantSum(which: i64) -> i64 {
    let mut v: Variant = Variant::new(0, 0);
    if which == 0 {
        v = Variant::new(0, 10);
    }
    if which == 1 {
        v = Variant::new(1, 2.5);
    }
    if which == 2 {
        v = Variant::new(2, 30);
    }
    if v.tag == 0 {
        return v.get::<i64>();
    }
    if v.tag == 1 {
        return ((v.get::<f64>()) as i64) * 100;
    }
    return (v.get::<i64>()) * 1000;
}

pub fn castRoundTrip() -> i64 {
    let d: f64 = 9.0;
    let asI64: i64 = (d as i64);
    let backToD: f64 = (asI64 as f64);
    return (backToD as i64) + asI64;
}
