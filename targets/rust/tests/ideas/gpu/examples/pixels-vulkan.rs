#[path = "../Vulkan.rs"]
mod Vulkan;
use Vulkan::{vulkanStart};

#[path = "shared/pixels.rs"]
mod pixels;
use pixels::{bitsDemo, channelsDemo, enlargeDemo, facePicture, printTable, resizeDemo, scanDemo};

pub fn main_() -> i64 {
    let gpu: i64 = vulkanStart();
    if gpu == 0 {
        return 10;
    }
    printTable(facePicture());
    let mut code: i64 = scanDemo(gpu, String::from(""));
    if code != 0 {
        return code;
    }
    code = channelsDemo(gpu, String::from(""));
    if code != 0 {
        return code;
    }
    code = bitsDemo(gpu, String::from(""));
    if code != 0 {
        return code;
    }
    code = resizeDemo(gpu, String::from(""));
    if code != 0 {
        return code;
    }
    return enlargeDemo(gpu, String::from(""));
}

fn main() {
    std::process::exit(main_() as i32);
}
