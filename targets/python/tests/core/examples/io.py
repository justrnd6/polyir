import sys

def putchar(c: int) -> int:
    sys.stdout.write(chr(c & 255))
    return c

def getchar() -> int:
    data = sys.stdin.buffer.read(1)
    return data[0] if data else -1

def write_line() -> int:
    putchar(104)
    putchar(108)
    putchar(108)
    putchar(118)
    putchar(109)
    putchar(32)
    putchar(105)
    putchar(111)
    return putchar(10)

def echo_one_byte() -> int:
    c: int = getchar()
    return putchar(c)

def main() -> int:
    write_line()
    echo_one_byte()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
