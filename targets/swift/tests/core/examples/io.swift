import Foundation

func writeLine() -> Int32 {
    putchar(Int32(truncatingIfNeeded: 104))
    putchar(Int32(truncatingIfNeeded: 108))
    putchar(Int32(truncatingIfNeeded: 108))
    putchar(Int32(truncatingIfNeeded: 118))
    putchar(Int32(truncatingIfNeeded: 109))
    putchar(Int32(truncatingIfNeeded: 32))
    putchar(Int32(truncatingIfNeeded: 105))
    putchar(Int32(truncatingIfNeeded: 111))
    return putchar(Int32(truncatingIfNeeded: 10))
}

func echoOneByte() -> Int32 {
    let c: Int32 = getchar()
    return putchar(Int32(truncatingIfNeeded: c))
}

func main() -> Int64 {
    writeLine()
    echoOneByte()
    return 0
}

exit(Int32(truncatingIfNeeded: main()))
