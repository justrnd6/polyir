fn putchar(c: i32) -> i32 { print!("{}", c as u8 as char); c }

fn getchar() -> i32 {
    use std::io::Read;
    let mut byte = [0u8];
    if std::io::stdin().read(&mut byte).unwrap_or(0) == 1 { byte[0] as i32 } else { -1 }
}

pub fn writeLine() -> i32 {
    putchar(104);
    putchar(108);
    putchar(108);
    putchar(118);
    putchar(109);
    putchar(32);
    putchar(105);
    putchar(111);
    return putchar(10);
}

pub fn echoOneByte() -> i32 {
    let c: i32 = getchar();
    return putchar(c);
}

pub fn main_() -> i64 {
    writeLine();
    echoOneByte();
    return 0;
}

fn main() {
    std::process::exit(main_() as i32);
}
