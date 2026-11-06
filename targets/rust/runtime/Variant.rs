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
