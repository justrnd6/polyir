#[path = "../Metal.rs"]
mod Metal;
use Metal::{metalDevice, metalReadFile};

#[path = "shared/pixels.rs"]
mod pixels;
use pixels::{bitsDemo, channelsDemo, enlargeDemo, facePicture, printTable, resizeDemo, scanDemo};

pub fn main_() -> i64 {
    let device: i64 = metalDevice();
    if device == 0 {
        return 10;
    }
    let source: String = metalReadFile(String::from("pixels.metal"));
    if source.clone() == String::from("") {
        return 11;
    }
    printTable(facePicture());
    let mut code: i64 = scanDemo(device, source.clone());
    if code != 0 {
        return code;
    }
    code = channelsDemo(device, source.clone());
    if code != 0 {
        return code;
    }
    code = bitsDemo(device, source.clone());
    if code != 0 {
        return code;
    }
    code = resizeDemo(device, source.clone());
    if code != 0 {
        return code;
    }
    return enlargeDemo(device, source.clone());
}

fn main() {
    std::process::exit(main_() as i32);
}
