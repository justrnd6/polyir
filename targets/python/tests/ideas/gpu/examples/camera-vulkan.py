from Vulkan import vulkan_start
from camera import run_all

def main() -> int:
    gpu: int = vulkan_start()
    if gpu == 0:
        return 10
    return run_all(gpu, "")

if __name__ == "__main__":
    raise SystemExit(main())
