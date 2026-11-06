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
