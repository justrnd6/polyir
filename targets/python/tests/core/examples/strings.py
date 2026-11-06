# A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
# inside the string, and a search starts inside it.
def text_length(text: str) -> int:
    return len(text.encode())

def text_byte(text: str, at: int) -> int:
    data = text.encode()
    if at < 0 or at >= len(data):
        return 0
    return data[at] - 256 if data[at] > 127 else data[at]

def text_slice(text: str, start: int, end: int) -> str:
    data = text.encode()
    start = max(0, min(start, len(data)))
    end = max(start, min(end, len(data)))
    return data[start:end].decode("utf-8", "replace")

def text_find(text: str, part: str, start: int) -> int:
    data = text.encode()
    return data.find(part.encode(), max(0, min(start, len(data))))

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def is_space_char(c: int) -> bool:
    return c == 32 or c == 9 or c == 10 or c == 13

def trim(s: str) -> str:
    from_: int = 0
    while from_ < text_length(s) and is_space_char(text_byte(s, from_)):
        from_ = i64(from_ + 1)
    to: int = text_length(s)
    while to > from_ and is_space_char(text_byte(s, i64(to - 1))):
        to = i64(to - 1)
    return text_slice(s, from_, to)

def main() -> int:
    print("hello from polyir")
    name: str = "café"
    n: int = 3
    print(f"{name} has {str(text_length(name))} bytes; {str(n)} + 1 = {str(i64(n + 1))}")
    print("a tab:\tand a quote: \"done\"")
    print(trim("a text block" + "\n" + "  keeps its inner indent" + "\n"))
    print(text_slice(name, 0, 3))
    print(str(text_find(name, "é", 0)))
    print(str(text_find(name, "tea", 0)))
    print(str(i64(text_length(name) * 10)))
    print(("true" if name == "café" else "false"))
    first: int = text_byte(name, 0)
    print(chr(first & 255))
    print(chr(first & 255))
    return i64(text_length(name) + i64(text_find(name, "é", 0) * 10))

if __name__ == "__main__":
    raise SystemExit(main())
