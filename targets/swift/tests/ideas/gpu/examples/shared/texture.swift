import Foundation

// What polyir shares is a class: two names for one list, map or set see each other's changes. Sizes and positions are Int64.
final class MutableList<T>: Sequence {
    var items: [T]

    init(_ items: [T] = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    subscript(at: Int64) -> T {
        get { items[Int(at)] }
        set { items[Int(at)] = newValue }
    }

    func append(_ item: T) {
        items.append(item)
    }

    func makeIterator() -> IndexingIterator<[T]> {
        items.makeIterator()
    }
}

extension MutableList: Equatable where T: Equatable {
    static func == (a: MutableList, b: MutableList) -> Bool {
        a.items == b.items
    }
}

extension MutableList: Hashable where T: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableMap<K: Hashable, V> {
    var items: [K: V]

    init(_ items: [K: V] = [:]) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    var keys: [K] {
        Array(items.keys)
    }

    subscript(key: K) -> V? {
        get { items[key] }
        set { items[key] = newValue }
    }
}

extension MutableMap: Equatable where V: Equatable {
    static func == (a: MutableMap, b: MutableMap) -> Bool {
        a.items == b.items
    }
}

extension MutableMap: Hashable where V: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableSet<T: Hashable>: Sequence, Hashable {
    var items: Set<T>

    init(_ items: Set<T> = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    func makeIterator() -> Set<T>.Iterator {
        items.makeIterator()
    }

    static func == (a: MutableSet, b: MutableSet) -> Bool {
        a.items == b.items
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

func wrap(_ n: Int32, _ size: Int32, _ mode: Int32) -> Int32 {
    if mode == 0 {
        if n < 0 {
            return 0
        }
        if n > size &- 1 {
            return size &- 1
        }
        return n
    }
    if mode == 1 {
        return (n % size &+ size) % size
    }
    let period: Int32 = size &* 2
    let m: Int32 = (n % period &+ period) % period
    if m >= size {
        return (period &- 1) &- m
    }
    return m
}

func texel(_ tex: MutableList<Float>, _ offset: Int32, _ size: Int32, _ x: Int32, _ y: Int32, _ mode: Int32) -> Float {
    return tex[Int64((offset &+ wrap(y, size, mode) &* size) &+ wrap(x, size, mode))]
}

func nearest(_ tex: MutableList<Float>, _ offset: Int32, _ size: Int32, _ u: Float, _ v: Float, _ mode: Int32) -> Float {
    let x: Int32 = Int32(floorf(u * Float(size)))
    let y: Int32 = Int32(floorf(v * Float(size)))
    return texel(tex, offset, size, x, y, mode)
}

func lerp(_ a: Float, _ b: Float, _ t: Float) -> Float {
    return a + (b - a) * t
}

func bilinear(_ tex: MutableList<Float>, _ offset: Int32, _ size: Int32, _ u: Float, _ v: Float, _ mode: Int32) -> Float {
    let sx: Float = u * Float(size) - 0.5
    let sy: Float = v * Float(size) - 0.5
    let x0: Int32 = Int32(floorf(sx))
    let y0: Int32 = Int32(floorf(sy))
    let fx: Float = sx - Float(x0)
    let fy: Float = sy - Float(y0)
    let top: Float = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, x0 &+ 1, y0, mode), fx)
    let bottom: Float = lerp(texel(tex, offset, size, x0, y0 &+ 1, mode), texel(tex, offset, size, x0 &+ 1, y0 &+ 1, mode), fx)
    return lerp(top, bottom, fy)
}

func levelOffset(_ base: Int32, _ level: Int32) -> Int32 {
    var offset: Int32 = 0
    var size: Int32 = base
    do {
        var k: Int64 = 0
        while k < Int64(truncatingIfNeeded: level) {
            offset = offset &+ size &* size
            size = size / 2
            k = k &+ 1
        }
    }
    return offset
}

func levelSize(_ base: Int32, _ level: Int32) -> Int32 {
    var size: Int32 = base
    do {
        var k: Int64 = 0
        while k < Int64(truncatingIfNeeded: level) {
            size = size / 2
            k = k &+ 1
        }
    }
    return size
}

func mipSample(_ tex: MutableList<Float>, _ base: Int32, _ levels: Int32, _ u: Float, _ v: Float, _ level: Float, _ mode: Int32) -> Float {
    var lo: Int32 = Int32(floorf(level))
    if lo < 0 {
        lo = 0
    }
    if lo > levels &- 1 {
        lo = levels &- 1
    }
    var hi: Int32 = lo &+ 1
    if hi > levels &- 1 {
        hi = levels &- 1
    }
    var amount: Float = level - Float(lo)
    if amount < 0.0 {
        amount = 0.0
    }
    if amount > 1.0 {
        amount = 1.0
    }
    let a: Float = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode)
    let b: Float = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode)
    return lerp(a, b, amount)
}

func pyramidSize(_ base: Int64) -> Int64 {
    var total: Int64 = 0
    var size: Int64 = base
    do {
        var k: Int64 = 0
        while k < 16 {
            if size >= 1 {
                total = total &+ size &* size
                size = size / 2
            }
            k = k &+ 1
        }
    }
    return total
}

func fillBase(_ tex: MutableList<Float>, _ base: Int64) -> Int64 {
    do {
        var y: Int64 = 0
        while y < base {
            do {
                var x: Int64 = 0
                while x < base {
                    var level: Float = (x / 4 &+ y / 4) % 2 == 1 ? 0.45 : 0.85
                    if x % 4 == 0 || y % 4 == 0 {
                        level = level * 0.6
                    }
                    tex[y &* base &+ x] = level
                    x = x &+ 1
                }
            }
            y = y &+ 1
        }
    }
    return 0
}

func buildPyramid(_ tex: MutableList<Float>, _ base: Int64) -> Int64 {
    fillBase(tex, base)
    var from: Int64 = 0
    var size: Int64 = base
    do {
        var level: Int64 = 0
        while level < 16 {
            if size > 1 {
                shrink(tex, from, size)
                from = from &+ size &* size
                size = size / 2
            }
            level = level &+ 1
        }
    }
    return 0
}

func shrink(_ tex: MutableList<Float>, _ from: Int64, _ size: Int64) -> Int64 {
    let next: Int64 = from &+ size &* size
    let half: Int64 = size / 2
    do {
        var y: Int64 = 0
        while y < half {
            do {
                var x: Int64 = 0
                while x < half {
                    let a: Float = tex[(from &+ 2 &* y &* size) &+ 2 &* x]
                    let b: Float = tex[((from &+ 2 &* y &* size) &+ 2 &* x) &+ 1]
                    let c: Float = tex[(from &+ (2 &* y &+ 1) &* size) &+ 2 &* x]
                    let d: Float = tex[((from &+ (2 &* y &+ 1) &* size) &+ 2 &* x) &+ 1]
                    tex[(next &+ y &* half) &+ x] = (a + b + c + d) * 0.25
                    x = x &+ 1
                }
            }
            y = y &+ 1
        }
    }
    return 0
}
