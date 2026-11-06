// A string's positions count its bytes. A byte outside the string is 0, a slice keeps what lies inside the string, and a
// search starts inside it.
pub fn text_byte(text: &str, at: i64) -> i8 {
    if at >= 0 && (at as usize) < text.len() { text.as_bytes()[at as usize] as i8 } else { 0 }
}

pub fn text_slice(text: &str, from: i64, to: i64) -> String {
    let size = text.len() as i64;
    let start = from.clamp(0, size);
    let end = to.clamp(start, size);
    String::from_utf8_lossy(&text.as_bytes()[start as usize..end as usize]).into_owned()
}

pub fn text_find(text: &str, part: &str, from: i64) -> i64 {
    let start = from.clamp(0, text.len() as i64) as usize;
    text.get(start..).and_then(|rest| rest.find(part)).map(|at| (at + start) as i64).unwrap_or(-1)
}
