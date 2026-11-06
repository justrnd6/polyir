#[path = "../Vulkan.rs"]
mod Vulkan;
use Vulkan::{vulkanStart};

#[path = "shared/handed.rs"]
mod handed;
use handed::{runAll};

pub fn main_() -> i64 {
    let gpu: i64 = vulkanStart();
    if gpu == 0 {
        return 10;
    }
    return runAll(gpu);
}

fn main() {
    std::process::exit(main_() as i32);
}
