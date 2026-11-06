use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

#[path = "Check.rs"]
mod Check;
use Check::{offInt};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

// A sparse grid keeps the cells written; a read finds the last write, else the zero.
#[derive(Clone, PartialEq)]
pub struct Sparse<T> {
    pub cells: Vec<(i64, i64, T)>,
    pub zero: T,
}

impl<T: Clone> Sparse<T> {
    pub fn new(zero: T) -> Ref<Sparse<T>> {
        share(Sparse { cells: Vec::new(), zero })
    }

    pub fn get(&self, row: i64, col: i64) -> T {
        self.cells.iter().rev().find(|cell| cell.0 == row && cell.1 == col).map(|cell| cell.2.clone()).unwrap_or(self.zero.clone())
    }

    pub fn set(&mut self, row: i64, col: i64, value: T) {
        self.cells.push((row, col, value));
    }
}

#[derive(Clone)]
pub struct Counter {
    pub n: i64,
    pub id: i64,
}

impl PartialEq for Counter {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.n == other.n
            && self.id == other.id
    }
}

impl Counter {
    pub fn new(n: i64, id: i64) -> Ref<Counter> {
        share(Counter { n, id })
    }
}

pub trait CounterMethods {
    fn bump(&self) -> i64;
}

impl CounterMethods for RefCell<Counter> {
    fn bump(&self) -> i64 {
        let assigned = self.borrow().n + 1;
        self.borrow_mut().n = assigned;
        return self.borrow().n;
    }
}

#[derive(Clone)]
pub struct Pt {
    pub x: i64,
    pub y: i64,
}

impl PartialEq for Pt {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Pt {
    pub fn new(x: i64, y: i64) -> Ref<Pt> {
        share(Pt { x, y })
    }
}

pub trait PtMethods {
}

impl PtMethods for RefCell<Pt> {
}

#[derive(Clone)]
pub struct Holder {
    pub items_: Ref<Vec<i64>>,
    pub inner: Ref<Counter>,
}

impl PartialEq for Holder {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.items_ == other.items_
            && self.inner == other.inner
    }
}

impl Holder {
    pub fn new(items_: Ref<Vec<i64>>, inner: Ref<Counter>) -> Ref<Holder> {
        share(Holder { items_, inner })
    }
}

pub trait HolderMethods {
}

impl HolderMethods for RefCell<Holder> {
}

pub trait Reading {
    fn read(&self) -> i64;
}

#[derive(Clone)]
pub struct Gauge {
    pub level: i64,
}

impl PartialEq for Gauge {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.level == other.level
    }
}

impl Gauge {
    pub fn new(level: i64) -> Ref<Gauge> {
        share(Gauge { level })
    }
}

pub trait GaugeMethods {
}

impl GaugeMethods for RefCell<Gauge> {
}

impl Reading for RefCell<Gauge> {
    fn read(&self) -> i64 {
        return self.borrow().level;
    }
}

pub fn addTen(c: Ref<Counter>) -> i64 {
    let assigned = c.borrow().n + 10;
    c.borrow_mut().n = assigned;
    return 0;
}

pub fn addTenOnACopy(c: Ref<Counter>) -> i64 {
    let c: Ref<Counter> = share(c.borrow().clone());
    let assigned = c.borrow().n + 10;
    c.borrow_mut().n = assigned;
    return c.borrow().n;
}

pub fn checkTwoNamesForOneObjectShareIt() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 7);
    let b: Ref<Counter> = a.clone();
    let assigned = 5;
    b.borrow_mut().n = assigned;
    return offInt(a.borrow().n, 5) + offInt(a.borrow().id, 7);
}

pub fn checkACalleeChangesTheCallersObject() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 0);
    addTen(a.clone());
    return offInt(a.borrow().n, 11);
}

pub fn checkAValParameterIsTheCalleesOwnCopy() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 0);
    let seen: i64 = addTenOnACopy(a.clone());
    return offInt(seen, 11) + offInt(a.borrow().n, 1);
}

pub fn checkValInFrontOfAnExpressionCopies() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 3);
    let b: Ref<Counter> = share(a.borrow().clone());
    let assigned = 50;
    b.borrow_mut().n = assigned;
    return offInt(a.borrow().n, 1) + offInt(b.borrow().n, 50) + offInt(b.borrow().id, 3);
}

pub fn checkCopyMethodAndValAgree() -> f64 {
    let a: Ref<Counter> = Counter::new(4, 0);
    let b: Ref<Counter> = share(a.borrow().clone());
    let assigned = 9;
    a.borrow_mut().n = assigned;
    return offInt(b.borrow().n, 4);
}

pub fn checkAMethodChangesThis() -> f64 {
    let a: Ref<Counter> = Counter::new(0, 0);
    a.bump();
    a.bump();
    return offInt(a.borrow().n, 2);
}

pub fn checkAnImmutableStructIsAValue() -> f64 {
    let p: Ref<Pt> = Pt::new(1, 2);
    let q: Ref<Pt> = share(p.borrow().clone());
    let r: Ref<Pt> = share(p.borrow().clone());
    return offInt(q.borrow().x + r.borrow().y, 3);
}

pub fn checkAFieldHoldingAnotherReferenceStruct() -> f64 {
    let h: Ref<Holder> = Holder::new(share(Vec::new()), Counter::new(1, 0));
    let alias: Ref<Holder> = h.clone();
    let assigned = 8;
    alias.borrow().inner.clone().borrow_mut().n = assigned;
    { let pushed = 5; alias.borrow().items_.clone().borrow_mut().push(pushed) };
    return offInt(h.borrow().inner.clone().borrow().n, 8) + offInt((h.borrow().items_.clone().borrow().len() as i64), 1);
}

pub fn checkAFieldCanBeReplacedWithAnotherObject() -> f64 {
    let h: Ref<Holder> = Holder::new(share(Vec::new()), Counter::new(1, 0));
    let keep: Ref<Counter> = h.borrow().inner.clone();
    let assigned = Counter::new(2, 0);
    h.borrow_mut().inner = assigned;
    return offInt(keep.borrow().n, 1) + offInt(h.borrow().inner.clone().borrow().n, 2);
}

pub fn checkAGenericStructWithAVarField() -> f64 {
    let c: Ref<Cell_i64> = Cell_i64::new(1);
    let d: Ref<Cell_i64> = c.clone();
    let assigned = 9;
    d.borrow_mut().v = assigned;
    return offInt(c.borrow().v, 9);
}

pub fn checkAnInterfaceValueSeesLaterChanges() -> f64 {
    let g: Ref<Gauge> = Gauge::new(1);
    let r: Rc<dyn Reading> = g.clone();
    let assigned = 42;
    g.borrow_mut().level = assigned;
    return offInt(r.read(), 42);
}

pub fn checkAStructInADarrayIsTheSameObject() -> f64 {
    let all: Ref<Vec<Ref<Counter>>> = share(Vec::new());
    let a: Ref<Counter> = Counter::new(1, 0);
    { let pushed = a.clone(); all.borrow_mut().push(pushed) };
    let assigned = 6;
    a.borrow_mut().n = assigned;
    return offInt(all.borrow()[0 as usize].clone().borrow().n, 6);
}

pub fn checkAStructInAClosureIsShared() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 0);
    let a_moved = a.clone();
    let f: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = a_moved.borrow().n + 1;
        a_moved.borrow_mut().n = assigned;
        return a_moved.borrow().n;
    }) as Rc<dyn Fn() -> i64>;
    (f)();
    return offInt(a.borrow().n, 2);
}

pub fn makeCounter(start: i64) -> Ref<Counter> {
    let c: Ref<Counter> = Counter::new(start, 1);
    let assigned = c.borrow().n + 1;
    c.borrow_mut().n = assigned;
    return c.clone();
}

pub fn lend(c: Ref<Counter>) -> i64 {
    return c.borrow().n * 2;
}

pub fn keepIn(store: Ref<Vec<Ref<Counter>>>, c: Ref<Counter>) -> i64 {
    { let pushed = c.clone(); store.borrow_mut().push(pushed) };
    return 0;
}

pub fn checkAReturnedObjectSurvives() -> f64 {
    let a: Ref<Counter> = makeCounter(5);
    let b: Ref<Counter> = makeCounter(10);
    return offInt(a.borrow().n * 100 + b.borrow().n, 611);
}

pub fn checkAnObjectLentToACalleeIsUsableAfterwards() -> f64 {
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < 100 {
            let c: Ref<Counter> = Counter::new(i, 0);
            total = total + lend(c.clone()) + c.borrow().n;
            let assigned = 0;
            c.borrow_mut().n = assigned;
            i = i + 1;
        }
    }
    return offInt(total, 3 * 4950);
}

pub fn checkAnObjectAnotherCalleeKeepsSurvives() -> f64 {
    let store: Ref<Vec<Ref<Counter>>> = share(Vec::new());
    {
        let mut i: i64 = 0;
        while i < 4 {
            let c: Ref<Counter> = Counter::new(i * 10, 0);
            keepIn(store.clone(), c.clone());
            i = i + 1;
        }
    }
    return offInt(store.borrow()[0 as usize].clone().borrow().n + store.borrow()[1 as usize].clone().borrow().n + store.borrow()[2 as usize].clone().borrow().n + store.borrow()[3 as usize].clone().borrow().n, 60);
}

pub fn checkAValParameterCopyIsReleasedAndTheOriginalIsNot() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 0);
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < 50 {
            total = total + addTenOnACopy(a.clone());
            i = i + 1;
        }
    }
    return offInt(total, 50 * 11) + offInt(a.borrow().n, 1);
}

pub fn earlyReturnWithObjects(n: i64) -> i64 {
    let c: Ref<Counter> = Counter::new(n, 0);
    if n > 5 {
        return c.borrow().n * 2;
    }
    c.bump();
    return c.borrow().n;
}

pub fn checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue() -> f64 {
    return offInt(earlyReturnWithObjects(9) * 100 + earlyReturnWithObjects(2), 1800 + 3);
}

pub fn checkLetVarAndValOnReferenceStructs() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 0);
    let alias: Ref<Counter> = a.clone();
    let copy: Ref<Counter> = share(a.borrow().clone());
    let mut moving: Ref<Counter> = a.clone();
    let assigned = 10;
    a.borrow_mut().n = assigned;
    moving = Counter::new(100, 0);
    let assigned = moving.borrow().n + 1;
    moving.borrow_mut().n = assigned;
    return offInt(alias.borrow().n, 10) + offInt(copy.borrow().n, 1) + offInt(moving.borrow().n, 101) + offInt(a.borrow().n, 10);
}

pub fn checkValOnAContainerIsAnIndependentCopy() -> f64 {
    let d: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 4; d.borrow_mut().push(pushed) };
    let e: Ref<Vec<i64>> = share(d.borrow().clone());
    { let pushed = 5; d.borrow_mut().push(pushed) };
    return offInt((e.borrow().len() as i64), 1) + offInt((d.borrow().len() as i64), 2);
}

#[derive(Clone)]
pub struct Dot {
    pub x: i64,
    pub y: i64,
}

impl PartialEq for Dot {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Dot {
    pub fn new(x: i64, y: i64) -> Ref<Dot> {
        share(Dot { x, y })
    }
}

pub trait DotMethods {
}

impl DotMethods for RefCell<Dot> {
}

#[derive(Clone)]
pub struct Span {
    pub lo: i64,
    pub hi: i64,
}

impl PartialEq for Span {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.lo == other.lo
            && self.hi == other.hi
    }
}

impl Span {
    pub fn new(lo: i64, hi: i64) -> Ref<Span> {
        share(Span { lo, hi })
    }
}

pub trait SpanMethods {
    fn width(&self) -> i64;
}

impl SpanMethods for RefCell<Span> {
    fn width(&self) -> i64 {
        return self.borrow().hi - self.borrow().lo;
    }
}

#[derive(Clone)]
pub struct Meter {
    pub reading: i64,
    pub id: i64,
}

impl PartialEq for Meter {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.reading == other.reading
            && self.id == other.id
    }
}

impl Meter {
    pub fn new(reading: i64, id: i64) -> Ref<Meter> {
        share(Meter { reading, id })
    }
}

pub trait MeterMethods {
    fn bump(&self) -> i64;
}

impl MeterMethods for RefCell<Meter> {
    fn bump(&self) -> i64 {
        let assigned = self.borrow().reading + 1;
        self.borrow_mut().reading = assigned;
        return self.borrow().reading;
    }
}

#[derive(Clone)]
pub struct Plate {
    pub code: i64,
}

impl PartialEq for Plate {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.code == other.code
    }
}

impl Plate {
    pub fn new(code: i64) -> Ref<Plate> {
        share(Plate { code })
    }
}

pub trait PlateMethods {
}

impl PlateMethods for RefCell<Plate> {
}

impl Reading for RefCell<Plate> {
    fn read(&self) -> i64 {
        return self.borrow().code;
    }
}

pub fn checkShorthandStructsWork() -> f64 {
    let d: Ref<Dot> = Dot::new(3, 4);
    let s: Ref<Span> = Span::new(2, 9);
    let w: Ref<Wrapper_i64> = Wrapper_i64::new(5);
    let r: Rc<dyn Reading> = Plate::new(7);
    return offInt(d.borrow().x + d.borrow().y, 7) + offInt(s.width(), 7) + offInt(w.borrow().value, 5) + offInt(r.read(), 7);
}

pub fn checkAShorthandStructWithAVarFieldIsAReference() -> f64 {
    let m: Ref<Meter> = Meter::new(1, 2);
    let alias: Ref<Meter> = m.clone();
    alias.bump();
    return offInt(m.borrow().reading, 2) + offInt(m.borrow().id, 2);
}

pub fn checkAHashmapOfObjectsIsUsableThenReleased() -> f64 {
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < 100 {
            let m: Ref<HashMap<i64, Ref<Counter>>> = share(HashMap::new());
            let stored = Counter::new(i, 1);
            m.borrow_mut().insert(1, stored);
            let stored = Counter::new(5, 2);
            m.borrow_mut().insert(2, stored);
            let stored = Counter::new(6, 2);
            m.borrow_mut().insert(2, stored);
            m.borrow().get(&1).cloned().unwrap_or_else(|| unreachable!("a struct has no zero value here")).bump();
            total = total + m.borrow().get(&1).cloned().unwrap_or_else(|| unreachable!("a struct has no zero value here")).borrow().n - i + m.borrow().get(&2).cloned().unwrap_or_else(|| unreachable!("a struct has no zero value here")).borrow().n + (m.borrow().len() as i64);
            i = i + 1;
        }
    }
    return offInt(total, 100 * (1 + 6 + 2));
}

pub fn checkASparseOfStringsIsUsableThenReleased() -> f64 {
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < 100 {
            let s: Ref<Sparse<String>> = Sparse::new(String::new());
            let stored = format!("{}{}", String::from("a"), String::from("b"));
            s.borrow_mut().set(0, 0, stored);
            let stored = format!("{}{}", String::from("c"), String::from("d"));
            s.borrow_mut().set(2, 3, stored);
            if s.borrow().get(2, 3) == String::from("cd") {
                total = total + (s.borrow().cells.len() as i64);
            }
            i = i + 1;
        }
    }
    return offInt(total, 200);
}

#[derive(Clone)]
pub struct Cell_i64 {
    pub v: i64,
}

impl PartialEq for Cell_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.v == other.v
    }
}

impl Cell_i64 {
    pub fn new(v: i64) -> Ref<Cell_i64> {
        share(Cell_i64 { v })
    }
}

pub trait Cell_i64Methods {
}

impl Cell_i64Methods for RefCell<Cell_i64> {
}

#[derive(Clone)]
pub struct Wrapper_i64 {
    pub value: i64,
}

impl PartialEq for Wrapper_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Wrapper_i64 {
    pub fn new(value: i64) -> Ref<Wrapper_i64> {
        share(Wrapper_i64 { value })
    }
}

pub trait Wrapper_i64Methods {
}

impl Wrapper_i64Methods for RefCell<Wrapper_i64> {
}
