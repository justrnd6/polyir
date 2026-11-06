use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn pairSum(i: i32, src: Ref<Vec<f32>>, n: i32) -> f32 {
    let a: i32 = ((2 * (i as i64)) as i32);
    let mut total: f32 = src.borrow()[a as usize].clone();
    if a + 1 < n {
        total = total + src.borrow()[(a + 1) as usize].clone();
    }
    return total;
}

pub fn pairSumInt(i: i32, src: Ref<Vec<i32>>, n: i32) -> i32 {
    let a: i32 = ((2 * (i as i64)) as i32);
    let mut total: i32 = src.borrow()[a as usize].clone();
    if a + 1 < n {
        total = total + src.borrow()[(a + 1) as usize].clone();
    }
    return total;
}

pub fn pairMax(i: i32, src: Ref<Vec<f32>>, n: i32) -> f32 {
    let a: i32 = ((2 * (i as i64)) as i32);
    let mut best: f32 = src.borrow()[a as usize].clone();
    if a + 1 < n {
        if src.borrow()[(a + 1) as usize].clone() > best {
            best = src.borrow()[(a + 1) as usize].clone();
        }
    }
    return best;
}

pub fn passCount(n: i64) -> i64 {
    let mut passes: i64 = 0;
    let mut left: i64 = n;
    {
        let mut k: i64 = 0;
        while k < 40 {
            if left > 1 {
                left = (left + 1) / 2;
                passes = passes + 1;
            }
            k = k + 1;
        }
    }
    return passes;
}

pub fn cpuSum(values: Ref<Vec<f32>>, n: i64) -> f32 {
    let mut total: f32 = 0.0f32;
    {
        let mut i: i64 = 0;
        while i < n {
            total = total + values.borrow()[i as usize].clone();
            i = i + 1;
        }
    }
    return total;
}

pub fn cpuMax(values: Ref<Vec<f32>>, n: i64) -> f32 {
    let mut best: f32 = values.borrow()[0 as usize].clone();
    {
        let mut i: i64 = 1;
        while i < n {
            if values.borrow()[i as usize].clone() > best {
                best = values.borrow()[i as usize].clone();
            }
            i = i + 1;
        }
    }
    return best;
}

pub fn fillTestData(values: Ref<Vec<f32>>, n: i64) -> i64 {
    {
        let mut i: i64 = 0;
        while i < n {
            let stored = ((i % 7) as f32);
            values.borrow_mut()[i as usize] = stored;
            i = i + 1;
        }
    }
    let stored = 1000.0f32;
    values.borrow_mut()[(n / 3) as usize] = stored;
    return 0;
}
