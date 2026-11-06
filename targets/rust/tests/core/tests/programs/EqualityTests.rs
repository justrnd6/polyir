use std::rc::Rc;
use std::cell::RefCell;

#[path = "Check.rs"]
mod Check;
use Check::{offInt};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

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

#[derive(Clone)]
pub struct Point {
    pub x: i64,
    pub y: i64,
}

impl PartialEq for Point {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Point {
    pub fn new(x: i64, y: i64) -> Ref<Point> {
        share(Point { x, y })
    }
}

pub trait PointMethods {
}

impl PointMethods for RefCell<Point> {
}

#[derive(Clone)]
pub struct Line {
    pub a: Ref<Point>,
    pub b: Ref<Point>,
}

impl PartialEq for Line {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.a == other.a
            && self.b == other.b
    }
}

impl Line {
    pub fn new(a: Ref<Point>, b: Ref<Point>) -> Ref<Line> {
        share(Line { a, b })
    }
}

pub trait LineMethods {
}

impl LineMethods for RefCell<Line> {
}

#[derive(Clone)]
pub struct Named {
    pub name: String,
    pub tags: Ref<Vec<i64>>,
}

impl PartialEq for Named {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.name == other.name
            && self.tags == other.tags
    }
}

impl Named {
    pub fn new(name: String, tags: Ref<Vec<i64>>) -> Ref<Named> {
        share(Named { name, tags })
    }
}

pub trait NamedMethods {
}

impl NamedMethods for RefCell<Named> {
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
}

impl CounterMethods for RefCell<Counter> {
}

#[derive(Clone)]
pub struct Digit {
    pub v: i64,
}

impl PartialEq for Digit {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.v == other.v
    }
}

impl Digit {
    pub fn new(v: i64) -> Ref<Digit> {
        share(Digit { v })
    }
}

pub trait DigitMethods {
    fn equals(&self, other_: Ref<Digit>) -> bool;
}

impl DigitMethods for RefCell<Digit> {
    fn equals(&self, other_: Ref<Digit>) -> bool {
        return self.borrow().v % 10 == other_.borrow().v % 10;
    }
}

#[derive(Clone)]
pub struct Wrapped {
    pub inner: Result<i64, String>,
}

impl PartialEq for Wrapped {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.inner == other.inner
    }
}

impl Wrapped {
    pub fn new(inner: Result<i64, String>) -> Ref<Wrapped> {
        share(Wrapped { inner })
    }
}

pub trait WrappedMethods {
}

impl WrappedMethods for RefCell<Wrapped> {
}

pub fn checkStructsAreEqualFieldByField() -> f64 {
    let p: Ref<Point> = Point::new(1, 2);
    let q: Ref<Point> = Point::new(1, 2);
    let r: Ref<Point> = Point::new(1, 3);
    let mut score: i64 = 0;
    if p.clone() == q.clone() {
        score = score + 1;
    }
    if p.clone() != r.clone() {
        score = score + 2;
    }
    if p.clone() == r.clone() {
        score = score + 100;
    }
    return offInt(score, 3);
}

pub fn checkNestedStructsAreEqualRecursively() -> f64 {
    let p: Ref<Point> = Point::new(1, 2);
    let z: Ref<Point> = Point::new(3, 4);
    let a: Ref<Line> = Line::new(p.clone(), z.clone());
    let b: Ref<Line> = Line::new(Point::new(1, 2), Point::new(3, 4));
    let c: Ref<Line> = Line::new(z.clone(), p.clone());
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != c.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkAStringAndAListInsideAStructAreComparedByContent() -> f64 {
    let t1: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 1; t1.borrow_mut().push(pushed) };
    let t2: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 1; t2.borrow_mut().push(pushed) };
    let a: Ref<Named> = Named::new(format!("{}{}", String::from("a"), String::from("b")), t1.clone());
    let b: Ref<Named> = Named::new(String::from("ab"), t2.clone());
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    { let pushed = 2; t2.borrow_mut().push(pushed) };
    if a.clone() != b.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkStringsCompareByCharacters() -> f64 {
    let x: String = String::from("match");
    let y: String = format!("{}{}", String::from("ma"), String::from("tch"));
    let mut score: i64 = 0;
    if x.clone() == y.clone() {
        score = score + 1;
    }
    if x.clone() != String::from("other") {
        score = score + 2;
    }
    if x.clone() == y.clone() {
        score = score + 100;
    }
    return offInt(score, 3);
}

pub fn checkAStructWithVarFieldsIsComparedByWhatItHoldsNow() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 2);
    let b: Ref<Counter> = Counter::new(1, 2);
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    let assigned = 5;
    b.borrow_mut().n = assigned;
    if a.clone() != b.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkIdentityIsTheSameObject() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 2);
    let b: Ref<Counter> = Counter::new(1, 2);
    let alias: Ref<Counter> = a.clone();
    let mut score: i64 = 0;
    if a.clone() == alias.clone() {
        score = score + 1;
    }
    if a.clone() != b.clone() {
        score = score + 2;
    }
    if a.clone() == b.clone() {
        score = score + 100;
    }
    return offInt(score, 3);
}

pub fn checkAnImmutableStructComparedByIdentityIsAnObject() -> f64 {
    let p: Ref<Point> = Point::new(1, 2);
    let q: Ref<Point> = Point::new(1, 2);
    let same: Ref<Point> = p.clone();
    let mut score: i64 = 0;
    if p.clone() == same.clone() {
        score = score + 1;
    }
    if p.clone() != q.clone() {
        score = score + 2;
    }
    if p.clone() == q.clone() {
        score = score + 4;
    }
    return offInt(score, 7);
}

pub fn checkAValStructIsADifferentObjectThatIsEqual() -> f64 {
    let a: Ref<Counter> = Counter::new(1, 2);
    let b: Ref<Counter> = share(a.borrow().clone());
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != b.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkAnEqualsMethodDecidesWhatEqualMeans() -> f64 {
    let a: Ref<Digit> = Digit::new(13);
    let b: Ref<Digit> = Digit::new(23);
    let c: Ref<Digit> = Digit::new(24);
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != c.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkListsAreEqualElementByElement() -> f64 {
    let a: Ref<Vec<i64>> = share(Vec::new());
    let b: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 1; a.borrow_mut().push(pushed) };
    { let pushed = 2; a.borrow_mut().push(pushed) };
    { let pushed = 1; b.borrow_mut().push(pushed) };
    { let pushed = 2; b.borrow_mut().push(pushed) };
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    { let pushed = 3; b.borrow_mut().push(pushed) };
    if a.clone() != b.clone() {
        score = score + 2;
    }
    if a.clone() == b.clone() {
        score = score + 100;
    }
    return offInt(score, 3);
}

pub fn checkListsOfStructsAreEqualDeeply() -> f64 {
    let a: Ref<Vec<Ref<Point>>> = share(Vec::new());
    let b: Ref<Vec<Ref<Point>>> = share(Vec::new());
    { let pushed = Point::new(1, 2); a.borrow_mut().push(pushed) };
    { let pushed = Point::new(1, 2); b.borrow_mut().push(pushed) };
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    { let pushed = Point::new(5, 6); a.borrow_mut().push(pushed) };
    { let pushed = Point::new(5, 7); b.borrow_mut().push(pushed) };
    if a.clone() != b.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload() -> f64 {
    let a: Result<i64, i64> = Ok(5);
    let b: Result<i64, i64> = Ok(5);
    let c: Result<i64, i64> = Err(5);
    let d: Result<i64, i64> = Ok(6);
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != c.clone() {
        score = score + 2;
    }
    if a.clone() != d.clone() {
        score = score + 4;
    }
    return offInt(score, 7);
}

pub fn checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload() -> f64 {
    let a: Variant = Variant::new(1, 2.5);
    let b: Variant = Variant::new(1, 2.5);
    let c: Variant = Variant::new(0, 2);
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != c.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkAStructHoldingAnEitherIsComparedThroughIt() -> f64 {
    let a: Ref<Wrapped> = Wrapped::new(Ok(1));
    let b: Ref<Wrapped> = Wrapped::new(Ok(1));
    let c: Ref<Wrapped> = Wrapped::new(Err(String::from("no")));
    let mut score: i64 = 0;
    if a.clone() == b.clone() {
        score = score + 1;
    }
    if a.clone() != c.clone() {
        score = score + 2;
    }
    return offInt(score, 3);
}

pub fn checkNanIsNotEqualToItself() -> f64 {
    let nan: f64 = 0.0 / 0.0;
    let mut score: i64 = 0;
    if nan == nan {
        score = score + 100;
    }
    if nan != nan {
        score = score + 1;
    }
    return offInt(score, 1);
}
