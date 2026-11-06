from Metal import metal_device, metal_read_file
from pixels import bits_demo, channels_demo, enlarge_demo, face_picture, print_table, resize_demo, scan_demo

def main() -> int:
    device: int = metal_device()
    if device == 0:
        return 10
    source: str = metal_read_file("pixels.metal")
    if source == "":
        return 11
    print_table(face_picture())
    code: int = scan_demo(device, source)
    if code != 0:
        return code
    code = channels_demo(device, source)
    if code != 0:
        return code
    code = bits_demo(device, source)
    if code != 0:
        return code
    code = resize_demo(device, source)
    if code != 0:
        return code
    return enlarge_demo(device, source)

if __name__ == "__main__":
    raise SystemExit(main())
