import Foundation

// A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
// inside the string, and a search starts inside it. A char (`Int8`) written is the character of its byte.
fileprivate func textLength(_ text: String) -> Int64 {
    Int64(text.utf8.count)
}

fileprivate func textByte(_ text: String, _ at: Int64) -> Int8 {
    let bytes = Array(text.utf8)
    return at >= 0 && at < Int64(bytes.count) ? Int8(bitPattern: bytes[Int(at)]) : 0
}

fileprivate func textSlice(_ text: String, _ from: Int64, _ to: Int64) -> String {
    let bytes = Array(text.utf8)
    let start = min(max(Int(from), 0), bytes.count)
    let end = min(max(Int(to), start), bytes.count)
    return String(decoding: bytes[start..<end], as: UTF8.self)
}

fileprivate func textFind(_ text: String, _ part: String, _ from: Int64) -> Int64 {
    let bytes = Array(text.utf8)
    let wanted = Array(part.utf8)
    var at = min(max(Int(from), 0), bytes.count)
    while at + wanted.count <= bytes.count {
        if Array(bytes[at..<(at + wanted.count)]) == wanted {
            return Int64(at)
        }
        at += 1
    }
    return -1
}

fileprivate func charText(_ c: Int8) -> String {
    String(UnicodeScalar(UInt8(bitPattern: c)))
}

func isSpaceChar(_ c: Int8) -> Bool {
    return c == 32 || c == 9 || c == 10 || c == 13
}

func trim(_ s: String) -> String {
    var from: Int64 = 0
    while from < textLength(s) && isSpaceChar(textByte(s, from)) {
        from = from &+ 1
    }
    var to: Int64 = textLength(s)
    while to > from && isSpaceChar(textByte(s, to &- 1)) {
        to = to &- 1
    }
    return textSlice(s, from, to)
}

func main() -> Int64 {
    print("hello from polyir")
    let name: String = "café"
    let n: Int64 = 3
    print("\(name) has \(String(textLength(name))) bytes; \(String(n)) + 1 = \(String(n &+ 1))")
    print("a tab:\tand a quote: \"done\"")
    print(trim("a text block" + "\n" + "  keeps its inner indent" + "\n"))
    print(textSlice(name, 0, 3))
    print(String(textFind(name, "é", 0)))
    print(String(textFind(name, "tea", 0)))
    print(String(textLength(name) &* 10))
    print(String(name == "café"))
    let first: Int8 = textByte(name, 0)
    print(charText(first))
    print(charText(first))
    return textLength(name) &+ textFind(name, "é", 0) &* 10
}

exit(Int32(truncatingIfNeeded: main()))
