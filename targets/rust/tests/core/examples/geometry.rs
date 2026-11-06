#[path = "shapes.rs"]
mod shapes;
use shapes::{Rect, area};

pub fn rectArea(w: i64, h: i64) -> i64 {
    return area(Rect::new(w, h));
}
