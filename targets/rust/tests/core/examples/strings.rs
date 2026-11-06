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

pub fn isSpaceChar(c: i8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 13;
}

pub fn trim(s: String) -> String {
    let mut from: i64 = 0;
    while from < (s.len() as i64) && isSpaceChar(text_byte(&s.clone(), from)) {
        from = from + 1;
    }
    let mut to: i64 = (s.len() as i64);
    while to > from && isSpaceChar(text_byte(&s.clone(), to - 1)) {
        to = to - 1;
    }
    return text_slice(&s.clone(), from, to);
}

pub fn main_() -> i64 {
    println!("{}", String::from("hello from polyir"));
    let name: String = String::from("café");
    let n: i64 = 3;
    println!("{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", name.clone(), String::from(" has ")), (name.len() as i64).to_string()), String::from(" bytes; ")), n.to_string()), String::from(" + 1 = ")), (n + 1).to_string()));
    println!("{}", String::from("a tab:\tand a quote: \"done\""));
    println!("{}", trim(format!("{}{}", format!("{}{}", format!("{}{}", String::from("a text block"), String::from("\n")), String::from("  keeps its inner indent")), String::from("\n"))));
    println!("{}", text_slice(&name.clone(), 0, 3));
    println!("{}", text_find(&name.clone(), &String::from("é"), 0).to_string());
    println!("{}", text_find(&name.clone(), &String::from("tea"), 0).to_string());
    println!("{}", ((name.len() as i64) * 10).to_string());
    println!("{}", (name.clone() == String::from("café")).to_string());
    let first: i8 = text_byte(&name.clone(), 0);
    println!("{}", ((first as u8) as char).to_string());
    println!("{}", ((first as u8) as char).to_string());
    return (name.len() as i64) + text_find(&name.clone(), &String::from("é"), 0) * 10;
}

fn main() {
    std::process::exit(main_() as i32);
}
