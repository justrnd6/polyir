use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn boxedI64() -> i64 {
    let b: Ref<Box_i64> = Box_i64::new(42);
    return b.getValue();
}

pub fn boxedBoth() -> i64 {
    let bi: Ref<Box_i64> = Box_i64::new(100);
    let bd: Ref<Box_double> = Box_double::new(2.0);
    return bi.getValue() + (bd.getValue() as i64);
}

pub fn pairOfIntAndDouble() -> i64 {
    let p: Ref<Pair_i64_double> = Pair_i64_double::new(7, 8.5);
    return p.sumAsInt();
}

pub fn doubledI64() -> i64 {
    let d: Ref<Doubler_i64> = Doubler_i64::new(21);
    return d.doubled();
}

pub fn containerOk() -> i64 {
    let c: Ref<Container_i64> = Container_i64::new(Ok(42));
    return c.unwrapOrDefault(0);
}

pub fn makePairResult(ok: i64) -> Result<Ref<Pair_i64_i64>, i64> {
    if ok == 0 {
        return Err(-1);
    }
    return Ok(Pair_i64_i64::new(11, 22));
}

pub fn pairResultOrErr(ok: i64) -> i64 {
    let r: Result<Ref<Pair_i64_i64>, i64> = makePairResult(ok);
    if r.is_err() {
        return r.clone().err().unwrap();
    }
    let p: Ref<Pair_i64_i64> = r.clone().ok().unwrap();
    return p.borrow().a * 100 + p.borrow().b;
}

#[derive(Clone)]
pub struct Box_i64 {
    pub value: i64,
}

impl PartialEq for Box_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Box_i64 {
    pub fn new(value: i64) -> Ref<Box_i64> {
        share(Box_i64 { value })
    }
}

pub trait Box_i64Methods {
    fn getValue(&self) -> i64;
}

impl Box_i64Methods for RefCell<Box_i64> {
    fn getValue(&self) -> i64 {
        return self.borrow().value;
    }
}

#[derive(Clone)]
pub struct Box_double {
    pub value: f64,
}

impl PartialEq for Box_double {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Box_double {
    pub fn new(value: f64) -> Ref<Box_double> {
        share(Box_double { value })
    }
}

pub trait Box_doubleMethods {
    fn getValue(&self) -> f64;
}

impl Box_doubleMethods for RefCell<Box_double> {
    fn getValue(&self) -> f64 {
        return self.borrow().value;
    }
}

#[derive(Clone)]
pub struct Pair_i64_double {
    pub a: i64,
    pub b: f64,
}

impl PartialEq for Pair_i64_double {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.a == other.a
            && self.b == other.b
    }
}

impl Pair_i64_double {
    pub fn new(a: i64, b: f64) -> Ref<Pair_i64_double> {
        share(Pair_i64_double { a, b })
    }
}

pub trait Pair_i64_doubleMethods {
    fn sumAsInt(&self) -> i64;
}

impl Pair_i64_doubleMethods for RefCell<Pair_i64_double> {
    fn sumAsInt(&self) -> i64 {
        return (self.borrow().a as i64) + (self.borrow().b as i64);
    }
}

#[derive(Clone)]
pub struct Doubler_i64 {
    pub value: i64,
}

impl PartialEq for Doubler_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Doubler_i64 {
    pub fn new(value: i64) -> Ref<Doubler_i64> {
        share(Doubler_i64 { value })
    }
}

pub trait Doubler_i64Methods {
    fn doubled(&self) -> i64;
}

impl Doubler_i64Methods for RefCell<Doubler_i64> {
    fn doubled(&self) -> i64 {
        return self.borrow().value + self.borrow().value;
    }
}

#[derive(Clone)]
pub struct Container_i64 {
    pub value: Result<i64, i64>,
}

impl PartialEq for Container_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Container_i64 {
    pub fn new(value: Result<i64, i64>) -> Ref<Container_i64> {
        share(Container_i64 { value })
    }
}

pub trait Container_i64Methods {
    fn unwrapOrDefault(&self, def: i64) -> i64;
}

impl Container_i64Methods for RefCell<Container_i64> {
    fn unwrapOrDefault(&self, def: i64) -> i64 {
        if self.borrow().value.clone().is_err() {
            return def;
        }
        return self.borrow().value.clone().clone().ok().unwrap();
    }
}

#[derive(Clone)]
pub struct Pair_i64_i64 {
    pub a: i64,
    pub b: i64,
}

impl PartialEq for Pair_i64_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.a == other.a
            && self.b == other.b
    }
}

impl Pair_i64_i64 {
    pub fn new(a: i64, b: i64) -> Ref<Pair_i64_i64> {
        share(Pair_i64_i64 { a, b })
    }
}

pub trait Pair_i64_i64Methods {
    fn sumAsInt(&self) -> i64;
}

impl Pair_i64_i64Methods for RefCell<Pair_i64_i64> {
    fn sumAsInt(&self) -> i64 {
        return (self.borrow().a as i64) + (self.borrow().b as i64);
    }
}
