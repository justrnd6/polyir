import sys
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

# Stops the program with a message, as polyir's `fail` does.
def fail(message: str):
    print("trap: " + message, file=sys.stderr)
    sys.exit(1)

# The value an optional holds; an empty one stops the program.
def present(value):
    if value is None:
        fail("unwrap of an empty optional")
    return value

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

def ends_with(s: str, suffix: str) -> bool:
    if text_length(suffix) > text_length(s):
        return False
    return text_slice(s, i64(text_length(s) - text_length(suffix)), text_length(s)) == suffix

def off_text(got: str, want: str) -> float:
    if got == want:
        return 0.0
    return 1.0

def day_name(day: int) -> str:
    name: str = "?"
    match day:
        case 0 | 6:
            name = "weekend"
        case 1:
            name = "monday"
        case _:
            name = "weekday"
    return name

def check_numbers() -> float:
    return off_text(day_name(0) + day_name(6) + day_name(1) + day_name(3), "weekendweekendmondayweekday")

def class_of(c: int) -> str:
    kind: str = "other"
    match c:
        case 97 | 101 | 105 | 111 | 117:
            kind = "vowel"
        case 32:
            kind = "space"
    return kind

def check_chars() -> float:
    return off_text(class_of(101) + class_of(32) + class_of(122), "vowelspaceother")

def verb(command: str) -> str:
    done: str = ""
    match command:
        case "start" | "go":
            done = "began"
        case "stop":
            done = "halted"
        case _:
            done = "?"
    return done

def check_strings() -> float:
    return off_text(verb("go") + verb("start") + verb("stop") + verb("x"), "beganbeganhalted?")

def check_booleans() -> float:
    said: str = ""
    i: int = 0
    while i < 2:
        match_57_9: bool = i == 0
        match match_57_9:
            case True:
                said = said + "yes"
            case False:
                said = said + "no"
        i = i64(i + 1)
    return off_text(said, "yesno")

def outcome(r: Ok | Err) -> str:
    said: str = ""
    if isinstance(r, Ok):
        v: int = r.value
        said = f"ok {v}"
    elif isinstance(r, Err):
        e: str = r.value
        said = f"err {e}"
    return said

def check_sums() -> float:
    count: int = 0
    found: int | None = 4
    if found is not None:
        n: int = present(found)
        count = n
    elif found is None:
        count = -1
    return off_text(f"{outcome(Ok(3))}, {outcome(Err("no"))}, {count}", "ok 3, err no, 4")

def status(code: int) -> str:
    said: str = ""
    if code == 200 or code == 204:
        said = "done"
    elif code >= 500:
        said = f"retry {code}"
    elif code >= 400:
        s: int = code
        said = f"fail {s}"
    else:
        said = f"? {code}"
    return said

def check_conditions_among_keys() -> float:
    return off_text(status(204) + "; " + status(503) + "; " + status(404) + "; " + status(301), "done; retry 503; fail 404; ? 301")

def counter() -> int:
    return 500

def check_call_subject() -> float:
    said: str = ""
    match_107_5: int = counter()
    if match_107_5 == 200:
        said = "ok"
    elif match_107_5 >= 500:
        s: int = match_107_5
        said = f"server {s}"
    else:
        said = "other"
    return off_text(said, "server 500")

def step(name: str) -> str:
    said: str = ""
    if ends_with(name, ".poly"):
        said = "compile " + name + ";"
    else:
        said = "skip " + name + ";"
    return said

def check_conditions() -> float:
    return off_text(step("a.poly") + step("b.md"), "compile a.poly;skip b.md;")
