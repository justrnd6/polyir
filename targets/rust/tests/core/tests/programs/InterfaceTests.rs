use std::rc::Rc;
use std::cell::RefCell;

#[path = "../../examples/interfaces.rs"]
mod interfaces;
use interfaces::{Rect, Shape, Square, chosen, directCall, interfaceArgument, sumAll, twoInterfaces, viaField};

#[path = "Check.rs"]
mod Check;
use Check::{off, offInt};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn checkConversionAtACall() -> f64 {
    return offInt(directCall(), 920);
}

pub fn checkConversionAtAReturn() -> f64 {
    return offInt(chosen(), 36034);
}

pub fn checkDarrayOfInterfaceValues() -> f64 {
    return offInt(sumAll(), 41);
}

pub fn checkInterfaceAsAStructField() -> f64 {
    return offInt(viaField(), 307);
}

pub fn checkOneStructTwoInterfaces() -> f64 {
    return offInt(twoInterfaces(), 1016);
}

pub fn checkInterfaceMethodTakingAnInterface() -> f64 {
    return offInt(interfaceArgument(), 109);
}

pub fn checkDispatchFollowsTheValue() -> f64 {
    let a: Rc<dyn Shape> = Square::new(5);
    let b: Rc<dyn Shape> = Rect::new(5, 2);
    return offInt(a.area(), 25) + offInt(b.area(), 10);
}

pub fn checkSameMethodNameOnTwoStructs() -> f64 {
    let s: Ref<Square> = Square::new(3);
    let r: Ref<Rect> = Rect::new(3, 4);
    return offInt(s.area() + r.area(), 21);
}

pub fn checkConvertedValueIsACopy() -> f64 {
    let s: Ref<Square> = Square::new(4);
    let a: Rc<dyn Shape> = s.clone();
    let b: Rc<dyn Shape> = s.clone();
    return offInt(a.area() + b.area(), 32);
}

pub trait Scaler {
    fn half(&self, p0: f32) -> f32;
    fn offset(&self, p0: i32) -> i64;
}

#[derive(Clone)]
pub struct Halver {
    pub base: i64,
}

impl PartialEq for Halver {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.base == other.base
    }
}

impl Halver {
    pub fn new(base: i64) -> Ref<Halver> {
        share(Halver { base })
    }
}

pub trait HalverMethods {
}

impl HalverMethods for RefCell<Halver> {
}

impl Scaler for RefCell<Halver> {
    fn half(&self, x: f32) -> f32 {
        return x / 2.0f32;
    }

    fn offset(&self, by: i32) -> i64 {
        return self.borrow().base + (by as i64);
    }
}

pub fn checkArgumentWidthsFollowTheInterface() -> f64 {
    let s: Rc<dyn Scaler> = Halver::new(40);
    return off(s.half(9.0f32), 4.5f32) + offInt(s.offset(2), 42);
}

pub fn checkInterfaceInAnEither() -> f64 {
    let r: Result<Rc<dyn Shape>, i64> = Ok(Rect::new(4, 4));
    if r.is_err() {
        return 1.0;
    }
    let s: Rc<dyn Shape> = r.clone().ok().unwrap();
    return offInt(s.area(), 16);
}

pub fn checkInterfaceCapturedByAClosure() -> f64 {
    let s: Rc<dyn Shape> = Square::new(7);
    let s_moved = s.clone();
    let f: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        return s_moved.area();
    }) as Rc<dyn Fn() -> i64>;
    return offInt((f)(), 49);
}

pub fn checkInterfaceThroughAVar() -> f64 {
    let mut s: Rc<dyn Shape> = Square::new(2);
    let first: i64 = s.area();
    s = Rect::new(3, 5);
    return offInt(first * 100 + s.area(), 415);
}

pub trait Weighted {
    fn weight(&self) -> i64;
}

pub fn totalWeight(a: Rc<dyn Weighted>, b: Rc<dyn Weighted>) -> i64 {
    return a.weight() + b.weight();
}

pub fn checkGenericStructConvertsToAnInterface() -> f64 {
    let a: Ref<Cell_i64> = Cell_i64::new(5);
    let b: Ref<Cell_double> = Cell_double::new(2.5);
    return offInt(totalWeight(a.clone(), b.clone()), 14) + offInt(a.get(), 5) + off((b.get() as f32), 2.5f32);
}

pub fn checkGenericInterfaceValuesInADarray() -> f64 {
    let all: Ref<Vec<Rc<dyn Weighted>>> = share(Vec::new());
    let a: Ref<Cell_i64> = Cell_i64::new(1);
    { let pushed = a.clone(); all.borrow_mut().push(pushed) };
    { let pushed = a.clone(); all.borrow_mut().push(pushed) };
    return offInt(all.borrow()[0 as usize].clone().weight() + all.borrow()[1 as usize].clone().weight(), 14);
}

#[derive(Clone)]
pub struct Cell_i64 {
    pub value: i64,
}

impl PartialEq for Cell_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Cell_i64 {
    pub fn new(value: i64) -> Ref<Cell_i64> {
        share(Cell_i64 { value })
    }
}

pub trait Cell_i64Methods {
    fn get(&self) -> i64;
}

impl Cell_i64Methods for RefCell<Cell_i64> {
    fn get(&self) -> i64 {
        return self.borrow().value;
    }
}

impl Weighted for RefCell<Cell_i64> {
    fn weight(&self) -> i64 {
        return 7;
    }
}

#[derive(Clone)]
pub struct Cell_double {
    pub value: f64,
}

impl PartialEq for Cell_double {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Cell_double {
    pub fn new(value: f64) -> Ref<Cell_double> {
        share(Cell_double { value })
    }
}

pub trait Cell_doubleMethods {
    fn get(&self) -> f64;
}

impl Cell_doubleMethods for RefCell<Cell_double> {
    fn get(&self) -> f64 {
        return self.borrow().value;
    }
}

impl Weighted for RefCell<Cell_double> {
    fn weight(&self) -> i64 {
        return 7;
    }
}
