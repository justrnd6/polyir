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

def contains(s: str, part: str) -> bool:
    return text_find(s, part, 0) >= 0

def starts_with(s: str, prefix: str) -> bool:
    if text_length(prefix) > text_length(s):
        return False
    return text_slice(s, 0, text_length(prefix)) == prefix

def ends_with(s: str, suffix: str) -> bool:
    if text_length(suffix) > text_length(s):
        return False
    return text_slice(s, i64(text_length(s) - text_length(suffix)), text_length(s)) == suffix

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

def lines(s: str) -> list[str]:
    return split(s, "\n")

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

def replace(s: str, old: str, by: str) -> str:
    return join(split(s, old), by)

def repeat(s: str, n: int) -> str:
    out: str = ""
    i: int = 0
    while i < n:
        out = out + s
        i = i64(i + 1)
    return out

def indent(s: str, n: int) -> str:
    all: list[str] = lines(s)
    pad: str = repeat(" ", n)
    i: int = 0
    while i < len(all):
        if text_length(all[i]) > 0:
            all[i] = pad + all[i]
        i = i64(i + 1)
    return join(all, "\n")

def indent_rest(s: str, n: int) -> str:
    first: int = text_find(s, "\n", 0)
    if first < 0:
        return s
    return text_slice(s, 0, i64(first + 1)) + indent(text_slice(s, i64(first + 1), text_length(s)), n)

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

def off_text(got: str, want: str) -> float:
    if got == want:
        return 0.0
    return 1.0

def off_int(got: int, want: int) -> float:
    return float(i64(got - want))

def check_escapes() -> float:
    s: str = "a\tb\n\"c\" \\ $"
    return off_int(text_length(s), 11)

def check_char_escapes() -> float:
    return off_int(i64(i64(10 + 39) + 92), i64(i64(10 + 39) + 92))

def check_interpolation() -> float:
    name: str = "zlib"
    n: int = 42
    return off_text(f"lib {name} v{i64(n + 1)} ok={'true' if n > 1 else 'false'}", "lib zlib v43 ok=true")

def check_interpolated_method_calls() -> float:
    words: list[str] = ["a", "b", "c"]
    return off_text(f"[{join(words, ", ")}]", "[a, b, c]")

def check_text_block() -> float:
    name: str = "demo"
    text: str = f"project({name})" + "\n" + "  add(\"x\")" + "\n" + "\n" + "done" + "\n"
    return off_text(text, "project(demo)\n  add(\"x\")\n\ndone\n")

def check_text_block_without_final_newline() -> float:
    text: str = "one" + "\n" + "two"
    return off_text(text, "one\ntwo")

def check_text_block_keeps_indent_of_multi_line_value() -> float:
    inner: str = "a\nb"
    text: str = "list:" + "\n" + "  " + indent_rest(inner, 2) + "\n" + "end" + "\n"
    return off_text(text, "list:\n  a\n  b\nend\n")

def check_str_of_values() -> float:
    c: int = 120
    return off_text(f"{i64(0 - 7)}{'true' if True else 'false'}{'true' if False else 'false'}{chr(c & 255)}{"s"}{12}", "-7truefalsexs12")

def check_char_at() -> float:
    s: str = "hello"
    return off_int(text_byte(s, 1), 101)

def check_slice_and_find() -> float:
    s: str = "key=value"
    at: int = text_find(s, "=", 0)
    return off_text(text_slice(s, 0, at) + "|" + text_slice(s, i64(at + 1), text_length(s)), "key|value") + off_int(text_find(s, "z", 0), i64(0 - 1))

def check_slice_clamps() -> float:
    s: str = "abc"
    return off_text(text_slice(s, i64(0 - 5), 99) + text_slice(s, 2, 1), "abc")

def check_find_from() -> float:
    s: str = "a.b.c"
    return off_int(text_find(s, ".", 2), 3)

def check_list_literal_and_for_in() -> float:
    total: int = 0
    for w in ["cmake", "-S", "src"]:
        total = i64(total + text_length(w))
    return off_int(total, 10)

def check_split_and_join() -> float:
    parts: list[str] = split("a,b,,c", ",")
    return off_int(len(parts), 4) + off_text(join(parts, "+"), "a+b++c")

def check_trim_and_contains() -> float:
    s: str = "  \tpadded \n"
    return off_text(trim(s), "padded") + off_int(int(contains(s, "dd")), 1) + off_int(int(contains(s, "x")), 0)

def check_starts_and_ends() -> float:
    s: str = "CMakeLists.txt"
    both: bool = starts_with(s, "CMake") and ends_with(s, ".txt") and not ends_with(s, "CMake")
    return off_int(int(both), 1)

def check_replace_repeat_indent() -> float:
    s: str = replace("a-b-c", "-", "::")
    return off_text(s + "|" + repeat("ab", 3) + "|" + indent("x\n\ny", 2), "a::b::c|ababab|  x\n\n  y")

def check_lines() -> float:
    return off_int(len(lines("one\ntwo\nthree")), 3)

def check_fill() -> float:
    values: dict[str, str] = {}
    values["name"] = "gcd"
    values["lang"] = "C"
    return off_text(fill("project(${name} LANGUAGES ${lang}) ${missing}", values), "project(gcd LANGUAGES C) ${missing}")

def write_deps(out: StringBuilder, deps: list[str]) -> None:
    out.indent(4)
    for d in deps:
        out.line(f"implementation(\"{d}\")")
    out.dedent()

def check_string_builder() -> float:
    out: StringBuilder = StringBuilder([], [])
    out.line("dependencies {")
    write_deps(out, ["a:b:1", "c:d:2"])
    out.line("}")
    want: str = "dependencies {" + "\n" + "    implementation(\"a:b:1\")" + "\n" + "    implementation(\"c:d:2\")" + "\n" + "}" + "\n"
    return off_text(out.text(), want) + off_int(int(out.is_empty()), 0)
