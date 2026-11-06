use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

fn fmaxf(x: f32, y: f32) -> f32 { x.max(y) }

pub fn atomicAddFloat(sums: Ref<Vec<f32>>, slot: i32, amount: f32) -> f32 {
    let old: f32 = sums.borrow()[slot as usize].clone();
    let stored = old + amount;
    sums.borrow_mut()[slot as usize] = stored;
    return old;
}

pub fn groupSize() -> i32 {
    return 1;
}

pub fn localIndex(i: i32) -> i32 {
    return 0;
}

pub fn groupIndex(i: i32) -> i32 {
    return i;
}

pub fn barrier() -> i32 {
    return 0;
}

pub fn blockSum(i: i32, src: Ref<Vec<f32>>, n: i32, tile: Ref<Vec<f32>>, partial: Ref<Vec<f32>>) -> i32 {
    let lane: i32 = localIndex(i);
    if i < n {
        let stored = src.borrow()[i as usize].clone();
        tile.borrow_mut()[lane as usize] = stored;
    } else {
        let stored = 0.0f32;
        tile.borrow_mut()[lane as usize] = stored;
    }
    barrier();
    {
        let mut s: i32 = groupSize() / 2;
        while s > 0 {
            if lane < s {
                let stored = tile.borrow()[lane as usize].clone() + tile.borrow()[(lane + s) as usize].clone();
                tile.borrow_mut()[lane as usize] = stored;
            }
            barrier();
            s = s / 2;
        }
    }
    if lane == 0 {
        let stored = tile.borrow()[0 as usize].clone();
        partial.borrow_mut()[groupIndex(i) as usize] = stored;
    }
    return 0;
}

pub fn blockMax(i: i32, src: Ref<Vec<f32>>, n: i32, tile: Ref<Vec<f32>>, partial: Ref<Vec<f32>>) -> i32 {
    let lane: i32 = localIndex(i);
    if i < n {
        let stored = src.borrow()[i as usize].clone();
        tile.borrow_mut()[lane as usize] = stored;
    } else {
        let stored = src.borrow()[0 as usize].clone();
        tile.borrow_mut()[lane as usize] = stored;
    }
    barrier();
    {
        let mut s: i32 = groupSize() / 2;
        while s > 0 {
            if lane < s {
                let stored = fmaxf(tile.borrow()[lane as usize].clone(), tile.borrow()[(lane + s) as usize].clone());
                tile.borrow_mut()[lane as usize] = stored;
            }
            barrier();
            s = s / 2;
        }
    }
    if lane == 0 {
        let stored = tile.borrow()[0 as usize].clone();
        partial.borrow_mut()[groupIndex(i) as usize] = stored;
    }
    return 0;
}

pub fn total(i: i32, src: Ref<Vec<f32>>, n: i32, tile: Ref<Vec<f32>>, sum: Ref<Vec<f32>>) -> i32 {
    let lane: i32 = localIndex(i);
    if i < n {
        let stored = src.borrow()[i as usize].clone();
        tile.borrow_mut()[lane as usize] = stored;
    } else {
        let stored = 0.0f32;
        tile.borrow_mut()[lane as usize] = stored;
    }
    barrier();
    {
        let mut s: i32 = groupSize() / 2;
        while s > 0 {
            if lane < s {
                let stored = tile.borrow()[lane as usize].clone() + tile.borrow()[(lane + s) as usize].clone();
                tile.borrow_mut()[lane as usize] = stored;
            }
            barrier();
            s = s / 2;
        }
    }
    if lane == 0 {
        atomicAddFloat(sum.clone(), 0, tile.borrow()[0 as usize].clone());
    }
    return 0;
}

pub fn groupsFor(n: i64) -> i64 {
    return (n + 255) / 256;
}

pub fn launchCount(n: i64) -> i64 {
    let mut launches: i64 = 0;
    let mut left: i64 = n;
    {
        let mut k: i64 = 0;
        while k < 10 {
            if left > 1 {
                left = groupsFor(left);
                launches = launches + 1;
            }
            k = k + 1;
        }
    }
    return launches;
}
