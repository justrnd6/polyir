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

pub fn endsWith(s: String, suffix: String) -> bool {
    if (suffix.len() as i64) > (s.len() as i64) {
        return false;
    }
    return text_slice(&s.clone(), (s.len() as i64) - (suffix.len() as i64), (s.len() as i64)) == suffix.clone();
}

pub fn offText(got: String, want: String) -> f64 {
    if got.clone() == want.clone() {
        return 0.0;
    }
    return 1.0;
}

pub fn dayName(day: i64) -> String {
    let mut name: String = String::from("?");
    match day {
        0 | 6 => {
            name = String::from("weekend");
        }
        1 => {
            name = String::from("monday");
        }
        _ => {
            name = String::from("weekday");
        }
    }
    return name.clone();
}

pub fn checkNumbers() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", dayName(0), dayName(6)), dayName(1)), dayName(3)), String::from("weekendweekendmondayweekday"));
}

pub fn classOf(c: i8) -> String {
    let mut kind: String = String::from("other");
    match c {
        97 | 101 | 105 | 111 | 117 => {
            kind = String::from("vowel");
        }
        32 => {
            kind = String::from("space");
        }
        _ => {}
    }
    return kind.clone();
}

pub fn checkChars() -> f64 {
    return offText(format!("{}{}", format!("{}{}", classOf(101), classOf(32)), classOf(122)), String::from("vowelspaceother"));
}

pub fn verb(command: String) -> String {
    let mut done: String = String::from("");
    if command.clone() == String::from("start") || command.clone() == String::from("go") {
        done = String::from("began");
    } else if command.clone() == String::from("stop") {
        done = String::from("halted");
    } else {
        done = String::from("?");
    }
    return done.clone();
}

pub fn checkStrings() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", verb(String::from("go")), verb(String::from("start"))), verb(String::from("stop"))), verb(String::from("x"))), String::from("beganbeganhalted?"));
}

pub fn checkBooleans() -> f64 {
    let mut said: String = String::from("");
    {
        let mut i: i64 = 0;
        while i < 2 {
            let match_57_9: bool = i == 0;
            match match_57_9 {
                true => {
                    said = format!("{}{}", said.clone(), String::from("yes"));
                }
                false => {
                    said = format!("{}{}", said.clone(), String::from("no"));
                }
                _ => {}
            }
            i = i + 1;
        }
    }
    return offText(said.clone(), String::from("yesno"));
}

pub fn outcome(r: Result<i64, String>) -> String {
    let mut said: String = String::from("");
    if r.is_ok() {
        let v: i64 = r.clone().ok().unwrap();
        said = format!("{}{}", String::from("ok "), v.to_string());
    } else if r.is_err() {
        let e: String = r.clone().err().unwrap();
        said = format!("{}{}", String::from("err "), e.clone());
    }
    return said.clone();
}

pub fn checkSums() -> f64 {
    let mut count: i64 = 0;
    let found: Option<i64> = Some(4);
    if found.is_some() {
        let n: i64 = found.clone().unwrap();
        count = n;
    } else if found.is_none() {
        count = -1;
    }
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", outcome(Ok(3)), String::from(", ")), outcome(Err(String::from("no")))), format!("{}{}", String::from(", "), count.to_string())), String::from("ok 3, err no, 4"));
}

pub fn status(code: i64) -> String {
    let mut said: String = String::from("");
    if code == 200 || code == 204 {
        said = String::from("done");
    } else if code >= 500 {
        said = format!("{}{}", String::from("retry "), code.to_string());
    } else if code >= 400 {
        let s: i64 = code;
        said = format!("{}{}", String::from("fail "), s.to_string());
    } else {
        said = format!("{}{}", String::from("? "), code.to_string());
    }
    return said.clone();
}

pub fn checkConditionsAmongKeys() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", status(204), String::from("; ")), status(503)), String::from("; ")), status(404)), String::from("; ")), status(301)), String::from("done; retry 503; fail 404; ? 301"));
}

pub fn counter() -> i64 {
    return 500;
}

pub fn checkCallSubject() -> f64 {
    let mut said: String = String::from("");
    let match_107_5: i64 = counter();
    if match_107_5 == 200 {
        said = String::from("ok");
    } else if match_107_5 >= 500 {
        let s: i64 = match_107_5;
        said = format!("{}{}", String::from("server "), s.to_string());
    } else {
        said = String::from("other");
    }
    return offText(said.clone(), String::from("server 500"));
}

pub fn step(name: String) -> String {
    let mut said: String = String::from("");
    if endsWith(name.clone(), String::from(".poly")) {
        said = format!("{}{}", format!("{}{}", String::from("compile "), name.clone()), String::from(";"));
    } else {
        said = format!("{}{}", format!("{}{}", String::from("skip "), name.clone()), String::from(";"));
    }
    return said.clone();
}

pub fn checkConditions() -> f64 {
    return offText(format!("{}{}", step(String::from("a.poly")), step(String::from("b.md"))), String::from("compile a.poly;skip b.md;"));
}
