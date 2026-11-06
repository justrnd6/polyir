use std::rc::Rc;
use std::cell::RefCell;

#[path = "Metal.rs"]
mod Metal;
use Metal::{metalBuffer, metalDispatch, metalEmptyBuffer};

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn fftStagesGpu(device: i64, stage: i64, first: i64, second: i64, total: i64, batch: i64, direction: i64) -> i64 {
    let mut source: i64 = first;
    let mut target: i64 = second;
    let scratch: i64 = metalEmptyBuffer(device, batch * (total / 2) * 4);
    let mut n: i64 = total;
    let mut s: i64 = 1;
    {
        let mut pass: i64 = 0;
        while pass < 30 {
            if n > 1 {
                let settings: Ref<Vec<i32>> = share(vec![0; 4 as usize]);
                let stored = (total as i32);
                settings.borrow_mut()[0 as usize] = stored;
                let stored = (n as i32);
                settings.borrow_mut()[1 as usize] = stored;
                let stored = (s as i32);
                settings.borrow_mut()[2 as usize] = stored;
                let stored = (direction as i32);
                settings.borrow_mut()[3 as usize] = stored;
                if metalDispatch(device, stage, batch * (total / 2), source, metalBuffer(device, (settings.clone() as i64), 16), target, scratch) == 0 {
                    return 0;
                }
                let swap: i64 = source;
                source = target;
                target = swap;
                n = n / 2;
                s = s * 2;
            }
            pass = pass + 1;
        }
    }
    return source;
}
