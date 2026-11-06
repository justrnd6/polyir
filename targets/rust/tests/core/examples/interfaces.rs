use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub trait Shape {
    fn area(&self) -> i64;
    fn scaled(&self, p0: i64) -> i64;
}

pub trait Named {
    fn id(&self) -> i64;
}

pub trait Combiner {
    fn combine(&self, p0: Rc<dyn Shape>) -> i64;
}

#[derive(Clone)]
pub struct Square {
    pub side: i64,
}

impl PartialEq for Square {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.side == other.side
    }
}

impl Square {
    pub fn new(side: i64) -> Ref<Square> {
        share(Square { side })
    }
}

pub trait SquareMethods {
}

impl SquareMethods for RefCell<Square> {
}

impl Shape for RefCell<Square> {
    fn area(&self) -> i64 {
        return self.borrow().side * self.borrow().side;
    }

    fn scaled(&self, k: i64) -> i64 {
        return self.borrow().side * k;
    }
}
impl Named for RefCell<Square> {
    fn id(&self) -> i64 {
        return 1;
    }
}

#[derive(Clone)]
pub struct Rect {
    pub w: i64,
    pub h: i64,
}

impl PartialEq for Rect {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.w == other.w
            && self.h == other.h
    }
}

impl Rect {
    pub fn new(w: i64, h: i64) -> Ref<Rect> {
        share(Rect { w, h })
    }
}

pub trait RectMethods {
}

impl RectMethods for RefCell<Rect> {
}

impl Shape for RefCell<Rect> {
    fn area(&self) -> i64 {
        return self.borrow().w * self.borrow().h;
    }

    fn scaled(&self, k: i64) -> i64 {
        return self.borrow().w * k;
    }
}

#[derive(Clone)]
pub struct Summer {
    pub base: i64,
}

impl PartialEq for Summer {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.base == other.base
    }
}

impl Summer {
    pub fn new(base: i64) -> Ref<Summer> {
        share(Summer { base })
    }
}

pub trait SummerMethods {
}

impl SummerMethods for RefCell<Summer> {
}

impl Combiner for RefCell<Summer> {
    fn combine(&self, other_: Rc<dyn Shape>) -> i64 {
        return self.borrow().base + other_.area();
    }
}

#[derive(Clone)]
pub struct Holder {
    pub shape: Rc<dyn Shape>,
    pub tag: i64,
}

impl PartialEq for Holder {
    fn eq(&self, other: &Self) -> bool {
        true
            && Rc::ptr_eq(&self.shape, &other.shape)
            && self.tag == other.tag
    }
}

impl Holder {
    pub fn new(shape: Rc<dyn Shape>, tag: i64) -> Ref<Holder> {
        share(Holder { shape, tag })
    }
}

pub trait HolderMethods {
}

impl HolderMethods for RefCell<Holder> {
}

pub fn totalArea(s: Rc<dyn Shape>) -> i64 {
    return s.area();
}

pub fn directCall() -> i64 {
    return totalArea(Square::new(3)) * 100 + totalArea(Rect::new(4, 5));
}

pub fn pick(which: i64) -> Rc<dyn Shape> {
    if which == 0 {
        return Square::new(6);
    }
    return Rect::new(2, 7);
}

pub fn chosen() -> i64 {
    let a: Rc<dyn Shape> = pick(0);
    let b: Rc<dyn Shape> = pick(1);
    return a.area() * 1000 + b.area() + b.scaled(10);
}

pub fn sumAll() -> i64 {
    let shapes: Ref<Vec<Rc<dyn Shape>>> = share(Vec::new());
    { let pushed = Square::new(2); shapes.borrow_mut().push(pushed) };
    { let pushed = Rect::new(3, 4); shapes.borrow_mut().push(pushed) };
    { let pushed = Square::new(5); shapes.borrow_mut().push(pushed) };
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < (shapes.borrow().len() as i64) {
            total = total + shapes.borrow()[i as usize].clone().area();
            i = i + 1;
        }
    }
    return total;
}

pub fn viaField() -> i64 {
    let h: Ref<Holder> = Holder::new(Rect::new(5, 6), 7);
    return h.borrow().shape.clone().area() * 10 + h.borrow().tag;
}

pub fn twoInterfaces() -> i64 {
    let sq: Ref<Square> = Square::new(4);
    let n: Rc<dyn Named> = sq.clone();
    let s: Rc<dyn Shape> = sq.clone();
    return n.id() * 1000 + s.area();
}

pub fn interfaceArgument() -> i64 {
    let c: Rc<dyn Combiner> = Summer::new(100);
    return c.combine(Rect::new(3, 3));
}

pub fn main_() -> i64 {
    return directCall() + chosen() + sumAll() + viaField() + twoInterfaces() + interfaceArgument();
}

fn main() {
    std::process::exit(main_() as i32);
}
