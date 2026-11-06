#[path = "../Metal.rs"]
mod Metal;
use Metal::{metalDevice, metalReadFile};

#[path = "shared/camera.rs"]
mod camera;
use camera::{runAll};

pub fn main_() -> i64 {
    let device: i64 = metalDevice();
    if device == 0 {
        return 10;
    }
    let source: String = metalReadFile(String::from("camera.metal"));
    if source.clone() == String::from("") {
        return 11;
    }
    return runAll(device, source.clone());
}

fn main() {
    std::process::exit(main_() as i32);
}
