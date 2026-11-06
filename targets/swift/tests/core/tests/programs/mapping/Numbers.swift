import Foundation

func mix(_ h: Int64, _ v: Int64) -> Int64 {
    return (h &* 31 &+ v) % 1000003
}

func wide() -> Int64 {
    let big: Int64 = 9223372036854775807
    let wrapped: Int64 = big &+ 1
    let product: Int64 = 3037000500 &* 3037000500
    let neg: Int64 = 0 &- big
    let quotient: Int64 = (0 &- 7) / 2
    let rest: Int64 = (0 &- 7) % 3
    var h: Int64 = 7
    h = mix(h, wrapped >> 40 % 1000)
    h = mix(h, product >> 33 % 1000)
    h = mix(h, neg >> 50 % 1000)
    h = mix(h, quotient &+ 100)
    h = mix(h, rest &+ 100)
    h = mix(h, big << 3 >> 60)
    h = mix(h, (0 &- 1) >> (5 &+ 100))
    return h
}

func thirtyTwo() -> Int64 {
    let big: Int32 = 2147483647
    let wrapped: Int32 = big &+ 1
    let product: Int32 = Int32(truncatingIfNeeded: 65536 &* 65536)
    let other: Int32 = Int32(truncatingIfNeeded: 123456789 &* 987)
    let quotient: Int32 = Int32(truncatingIfNeeded: (0 &- 7) / 2)
    let rest: Int32 = Int32(truncatingIfNeeded: (0 &- 7) % 3)
    var h: Int64 = 11
    h = mix(h, Int64(truncatingIfNeeded: wrapped) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: product) &+ 5)
    h = mix(h, Int64(truncatingIfNeeded: other) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: quotient) &+ 100)
    h = mix(h, Int64(truncatingIfNeeded: rest) &+ 100)
    h = mix(h, Int64(truncatingIfNeeded: wrapped >> 7) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: big << 5) % 100003)
    return h
}

func unsigned32() -> Int64 {
    let top: UInt32 = 4294967295
    let wrapped: UInt32 = top &+ 2
    let product: UInt32 = UInt32(truncatingIfNeeded: 4000000000 &* 7)
    let under: UInt32 = UInt32(truncatingIfNeeded: 3 &- 5)
    let shifted: UInt32 = top << 4
    var h: Int64 = 13
    h = mix(h, Int64(truncatingIfNeeded: wrapped))
    h = mix(h, Int64(truncatingIfNeeded: product) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: under) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: shifted) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: top >> 3) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: top / 3) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: top % 1000))
    return h
}

func narrow() -> Int64 {
    let a: Int8 = 127
    let b: Int8 = a &+ 1
    let c: Int16 = 32767
    let d: Int16 = c &+ 1
    let e: UInt8 = 250
    let f: UInt8 = e &+ 10
    let g: UInt16 = 65535
    let hh: UInt16 = g &+ 3
    let m: Int8 = Int8(truncatingIfNeeded: 0 &- 128)
    let n: Int8 = m / Int8(truncatingIfNeeded: 0 &- 1)
    var h: Int64 = 17
    h = mix(h, Int64(truncatingIfNeeded: b) &+ 200)
    h = mix(h, Int64(truncatingIfNeeded: d) &+ 40000)
    h = mix(h, Int64(truncatingIfNeeded: f))
    h = mix(h, Int64(truncatingIfNeeded: hh))
    h = mix(h, Int64(truncatingIfNeeded: n) &+ 200)
    h = mix(h, Int64(truncatingIfNeeded: b >> 2) &+ 200)
    h = mix(h, Int64(truncatingIfNeeded: e << 3))
    return h
}

func casts() -> Int64 {
    let big: Int64 = 5000000000
    let cut: Int32 = Int32(truncatingIfNeeded: big)
    let tiny: Int8 = Int8(truncatingIfNeeded: 300)
    let uns: UInt32 = UInt32(truncatingIfNeeded: 0 &- 5)
    let up: UInt64 = UInt64(truncatingIfNeeded: 0 &- 1)
    let back: Int64 = Int64(truncatingIfNeeded: up)
    let f: Double = 3.99
    let g: Double = 0.0 - 3.99
    var h: Int64 = 19
    h = mix(h, Int64(truncatingIfNeeded: cut) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: tiny) &+ 200)
    h = mix(h, Int64(truncatingIfNeeded: uns) % 100003)
    h = mix(h, Int64(truncatingIfNeeded: up >> 60))
    h = mix(h, back &+ 5)
    h = mix(h, Int64(f))
    h = mix(h, Int64(g) &+ 10)
    h = mix(h, Int64(f * 2.5))
    return h
}

func main() -> Int64 {
    var h: Int64 = 1
    h = mix(h, wide())
    h = mix(h, thirtyTwo())
    h = mix(h, unsigned32())
    h = mix(h, narrow())
    h = mix(h, casts())
    return h % 251
}

exit(Int32(truncatingIfNeeded: main()))
