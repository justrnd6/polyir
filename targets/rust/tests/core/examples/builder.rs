use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

// Stops the program with a message, as polyir's `fail` does.
pub fn fail(message: &str) -> ! {
    eprintln!("trap: {}", message);
    std::process::exit(1)
}

#[derive(Clone)]
pub struct Shape {
    pub side: i64,
    pub name: String,
    pub tags: Ref<Vec<String>>,
}

impl PartialEq for Shape {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.side == other.side
            && self.name == other.name
            && self.tags == other.tags
    }
}

impl Shape {
    pub fn new(side: i64, name: String, tags: Ref<Vec<String>>) -> Ref<Shape> {
        share(Shape { side, name, tags })
    }
}

pub trait ShapeMethods {
    fn size(&self) -> Ref<Shape_Size>;
}

impl ShapeMethods for RefCell<Shape> {
    fn size(&self) -> Ref<Shape_Size> {
        return Shape_Size::new(self.borrow().side * self.borrow().side);
    }
}

pub fn Shape_square(side: i64) -> Ref<Shape> {
    return Shape_builder().side(side).name(String::from("square")).build();
}

pub fn Shape_builder() -> Ref<Shape_Builder> {
    return Shape_Builder::new(None, String::from("shape"), share(Vec::new()));
}

#[derive(Clone)]
pub struct Shape_Size {
    pub area: i64,
}

impl PartialEq for Shape_Size {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.area == other.area
    }
}

impl Shape_Size {
    pub fn new(area: i64) -> Ref<Shape_Size> {
        share(Shape_Size { area })
    }
}

pub trait Shape_SizeMethods {
}

impl Shape_SizeMethods for RefCell<Shape_Size> {
}

#[derive(Clone)]
pub struct Shape_Builder {
    pub __side: Option<i64>,
    pub __name: String,
    pub __tags: Ref<Vec<String>>,
}

impl PartialEq for Shape_Builder {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.__side == other.__side
            && self.__name == other.__name
            && self.__tags == other.__tags
    }
}

impl Shape_Builder {
    pub fn new(__side: Option<i64>, __name: String, __tags: Ref<Vec<String>>) -> Ref<Shape_Builder> {
        share(Shape_Builder { __side, __name, __tags })
    }
}

pub trait Shape_BuilderMethods {
    fn side(&self, value: i64) -> Ref<Shape_Builder>;
    fn name(&self, value: String) -> Ref<Shape_Builder>;
    fn tags(&self, value: Ref<Vec<String>>) -> Ref<Shape_Builder>;
    fn build(&self) -> Ref<Shape>;
}

impl Shape_BuilderMethods for RefCell<Shape_Builder> {
    fn side(&self, value: i64) -> Ref<Shape_Builder> {
        let assigned = Some(value);
        self.borrow_mut().__side = assigned;
        return self;
    }

    fn name(&self, value: String) -> Ref<Shape_Builder> {
        let assigned = value.clone();
        self.borrow_mut().__name = assigned;
        return self;
    }

    fn tags(&self, value: Ref<Vec<String>>) -> Ref<Shape_Builder> {
        let assigned = value.clone();
        self.borrow_mut().__tags = assigned;
        return self;
    }

    fn build(&self) -> Ref<Shape> {
        if self.borrow().__side.clone().is_none() {
            fail(&String::from("Shape.build: side is not set"));
        }
        return Shape::new(self.borrow().__side.clone().clone().unwrap(), self.borrow().__name.clone(), self.borrow().__tags.clone());
    }
}

pub fn tagged(b: Ref<Shape_Builder>) -> Ref<Shape_Builder> {
    let tags: Ref<Vec<String>> = share(vec![String::from("red"), String::from("big")]);
    return b.tags(tags.clone());
}

pub fn main_() -> () {
    let first: Ref<Shape> = tagged(Shape_builder()).side(2).build();
    let second: Ref<Shape> = Shape_square(3);
    let size: Ref<Shape_Size> = second.size();
    println!("{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", first.borrow().name.clone(), String::from(" ")), first.borrow().side.to_string()), String::from(" ")), first.size().borrow().area.to_string()), String::from(" [")), first.borrow().tags.clone().borrow()[0 as usize].clone()), String::from(", ")), first.borrow().tags.clone().borrow()[1 as usize].clone()), String::from("] ")), second.borrow().name.clone()), String::from(" ")), second.borrow().side.to_string()), String::from(" ")), size.borrow().area.to_string()));
}

fn main() {
    main_();
}
