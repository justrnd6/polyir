from Metal import metal_device, metal_read_file
from camera import run_all

def main() -> int:
    device: int = metal_device()
    if device == 0:
        return 10
    source: str = metal_read_file("camera.metal")
    if source == "":
        return 11
    return run_all(device, source)

if __name__ == "__main__":
    raise SystemExit(main())
