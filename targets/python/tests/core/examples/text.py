from __future__ import annotations
from dataclasses import dataclass

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

# Shortens a list to at most `count` items, and gives it back.
def truncate(items: list, count: int) -> list:
    del items[max(count, 0):]
    return items

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

def split(s: str, sep: str) -> list[str]:
    parts: list[str] = []
    if text_length(sep) == 0:
        parts.append(s)
        return parts
    from_: int = 0
    at: int = text_find(s, sep, 0)
    while at >= 0:
        parts.append(text_slice(s, from_, at))
        from_ = i64(at + text_length(sep))
        at = text_find(s, sep, from_)
    parts.append(text_slice(s, from_, text_length(s)))
    return parts

def join(parts: list[str], sep: str) -> str:
    out: str = ""
    i: int = 0
    while i < len(parts):
        if i > 0:
            out = out + sep
        out = out + parts[i]
        i = i64(i + 1)
    return out

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

def repeat(s: str, n: int) -> str:
    out: str = ""
    i: int = 0
    while i < n:
        out = out + s
        i = i64(i + 1)
    return out

def fill(template: str, values: dict[str, str]) -> str:
    out: str = ""
    from_: int = 0
    at: int = text_find(template, "${", 0)
    while at >= 0:
        close: int = text_find(template, "}", at)
        if close < 0:
            return out + text_slice(template, from_, text_length(template))
        out = out + text_slice(template, from_, at) + filled_name(text_slice(template, at, i64(close + 1)), values)
        from_ = i64(close + 1)
        at = text_find(template, "${", from_)
    return out + text_slice(template, from_, text_length(template))

def filled_name(placeholder: str, values: dict[str, str]) -> str:
    name: str = trim(text_slice(placeholder, 2, i64(text_length(placeholder) - 1)))
    if name in values:
        return values.get(name, "")
    return placeholder

@dataclass
class StringBuilder:
    pieces: list[str]
    indents: list[int]

    def add(self, s: str) -> StringBuilder:
        self.pieces.append(s)
        return self

    def line(self, s: str) -> StringBuilder:
        if text_length(s) > 0:
            self.pieces.append(repeat(" ", self.indent_now()))
        self.pieces.append(s)
        self.pieces.append("\n")
        return self

    def blank(self) -> StringBuilder:
        self.pieces.append("\n")
        return self

    def lines(self, all: list[str]) -> StringBuilder:
        i: int = 0
        while i < len(all):
            self.line(all[i])
            i = i64(i + 1)
        return self

    def indent(self, n: int) -> StringBuilder:
        self.indents.append(i64(self.indent_now() + n))
        return self

    def dedent(self) -> StringBuilder:
        if len(self.indents) > 0:
            truncate(self.indents, i64(len(self.indents) - 1))
        return self

    def indent_now(self) -> int:
        if len(self.indents) == 0:
            return 0
        return self.indents[i64(len(self.indents) - 1)]

    def length(self) -> int:
        return text_length(self.text())

    def is_empty(self) -> bool:
        return self.length() == 0

    def text(self) -> str:
        return join(self.pieces, "")

@dataclass
class Dep:
    name: str
    version: str

def cmake_lists(name: str, sources: list[str], deps: list[Dep]) -> str:
    out: StringBuilder = StringBuilder([], [])
    for d in deps:
        out.line(f"pkg_check_modules({d.name} REQUIRED IMPORTED_TARGET {d.name}>={d.version})")
    return "cmake_minimum_required(VERSION 3.20)" + "\n" + "project(" + name + " LANGUAGES C)" + "\n" + "find_package(PkgConfig REQUIRED)" + "\n" + trim(out.text()) + "\n" + "add_executable(" + name + " " + join(sources, " ") + ")" + "\n"

def checksum(s: str) -> int:
    sum_: int = 0
    i: int = 0
    while i < text_length(s):
        sum_ = rem(i64(i64(sum_ * 31) + text_byte(s, i)), 1000003)
        i = i64(i + 1)
    return sum_

def main() -> int:
    deps: list[Dep] = [Dep("zlib", "1.2"), Dep("sdl2", "2.0")]
    text: str = cmake_lists("demo", ["main.c", "util.c"], deps)
    values: dict[str, str] = {}
    values["who"] = "polyir"
    hello: str = fill("hello ${who}", values)
    return rem(i64(i64(checksum(text) + i64(len(split("a, b ,c", ",")) * 7)) + text_length(hello)), 256)

if __name__ == "__main__":
    raise SystemExit(main())
