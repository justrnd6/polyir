use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

#[derive(Clone)]
pub struct Geometry_Point {
    pub x: i64,
    pub y: i64,
}

impl PartialEq for Geometry_Point {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Geometry_Point {
    pub fn new(x: i64, y: i64) -> Ref<Geometry_Point> {
        share(Geometry_Point { x, y })
    }
}

pub trait Geometry_PointMethods {
    fn manhattan(&self) -> i64;
    fn absolute(&self, n: i64) -> i64;
}

impl Geometry_PointMethods for RefCell<Geometry_Point> {
    fn manhattan(&self) -> i64 {
        return self.absolute(self.borrow().x) + self.absolute(self.borrow().y);
    }

    fn absolute(&self, n: i64) -> i64 {
        return if n < 0 { 0 - n } else { n };
    }
}

pub fn Geometry_clamp(n: i64, low: i64, high: i64) -> i64 {
    return if n < low { low } else { if n > high { high } else { n } };
}

pub fn Geometry_distance(a: Ref<Geometry_Point>, b: Ref<Geometry_Point>) -> i64 {
    let step: Ref<Geometry_Point> = Geometry_Point::new(a.borrow().x - b.borrow().x, a.borrow().y - b.borrow().y);
    return Geometry_clamp(step.manhattan(), 0, 100);
}

pub trait Geometry_Shapes_Shape {
    fn area(&self) -> i64;
}

#[derive(Clone)]
pub struct Geometry_Shapes_Square {
    pub side: i64,
}

impl PartialEq for Geometry_Shapes_Square {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.side == other.side
    }
}

impl Geometry_Shapes_Square {
    pub fn new(side: i64) -> Ref<Geometry_Shapes_Square> {
        share(Geometry_Shapes_Square { side })
    }
}

pub trait Geometry_Shapes_SquareMethods {
}

impl Geometry_Shapes_SquareMethods for RefCell<Geometry_Shapes_Square> {
}

impl Geometry_Shapes_Shape for RefCell<Geometry_Shapes_Square> {
    fn area(&self) -> i64 {
        return self.borrow().side * self.borrow().side;
    }
}

#[derive(Clone)]
pub struct Geometry_Shapes_Rect {
    pub w: i64,
    pub h: i64,
}

impl PartialEq for Geometry_Shapes_Rect {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.w == other.w
            && self.h == other.h
    }
}

impl Geometry_Shapes_Rect {
    pub fn new(w: i64, h: i64) -> Ref<Geometry_Shapes_Rect> {
        share(Geometry_Shapes_Rect { w, h })
    }
}

pub trait Geometry_Shapes_RectMethods {
}

impl Geometry_Shapes_RectMethods for RefCell<Geometry_Shapes_Rect> {
}

impl Geometry_Shapes_Shape for RefCell<Geometry_Shapes_Rect> {
    fn area(&self) -> i64 {
        return self.borrow().w * self.borrow().h;
    }
}

pub fn Geometry_Shapes_total(shapes: Ref<Vec<Rc<dyn Geometry_Shapes_Shape>>>) -> i64 {
    let mut sum: i64 = 0;
    let s_items = shapes.borrow().clone();
    for s in s_items {
        sum = sum + s.area();
    }
    return sum;
}

pub fn main_() -> () {
    let a: Ref<Geometry_Point> = Geometry_Point::new(1, 2);
    let b: Ref<Geometry_Point> = Geometry_Point::new(4, -2);
    let shapes: Ref<Vec<Rc<dyn Geometry_Shapes_Shape>>> = share(vec![Geometry_Shapes_Square::new(3), Geometry_Shapes_Rect::new(2, 5)]);
    let size: String = if Geometry_Shapes_total(shapes.clone()) > 10 { String::from("big") } else { String::from("small") };
    println!("{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", Geometry_distance(a.clone(), b.clone()).to_string(), String::from(" ")), size.clone()), String::from(" ")), larger_i64(3, 7, false).to_string()), String::from(" ")), larger_string(String::from("x"), String::from("y"), true)), String::from(" ")), firstOf_string(share(vec![String::from("first"), String::from("second")]))));
}

pub fn larger_i64(a: i64, b: i64, aFirst: bool) -> i64 {
    return if aFirst { a } else { b };
}

pub fn larger_string(a: String, b: String, aFirst: bool) -> String {
    return if aFirst { a.clone() } else { b.clone() };
}

pub fn firstOf_string(xs: Ref<Vec<String>>) -> String {
    return xs.borrow()[0 as usize].clone();
}

fn main() {
    main_();
}
