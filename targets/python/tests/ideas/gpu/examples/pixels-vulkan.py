from Vulkan import vulkan_start
from pixels import bits_demo, channels_demo, enlarge_demo, face_picture, print_table, resize_demo, scan_demo

def main() -> int:
    gpu: int = vulkan_start()
    if gpu == 0:
        return 10
    print_table(face_picture())
    code: int = scan_demo(gpu, "")
    if code != 0:
        return code
    code = channels_demo(gpu, "")
    if code != 0:
        return code
    code = bits_demo(gpu, "")
    if code != 0:
        return code
    code = resize_demo(gpu, "")
    if code != 0:
        return code
    return enlarge_demo(gpu, "")

if __name__ == "__main__":
    raise SystemExit(main())
