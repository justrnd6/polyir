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

func atomicMin(_ values: MutableList<Int32>, _ slot: Int32, _ candidate: Int32) -> Int32 {
    let old: Int32 = values[Int64(slot)]
    if candidate < old {
        values[Int64(slot)] = candidate
    }
    return old
}

func emptyKey() -> Int32 {
    return 2147483647
}

func rasterize(_ t: Int32, _ tris: MutableList<Float>, _ width: Int32, _ height: Int32, _ depth: MutableList<Int32>) -> Int32 {
    let base: Int32 = t &* 10
    let x0: Float = tris[Int64(base)]
    let y0: Float = tris[Int64(base &+ 1)]
    let z0: Float = tris[Int64(base &+ 2)]
    let x1: Float = tris[Int64(base &+ 3)]
    let y1: Float = tris[Int64(base &+ 4)]
    let z1: Float = tris[Int64(base &+ 5)]
    let x2: Float = tris[Int64(base &+ 6)]
    let y2: Float = tris[Int64(base &+ 7)]
    let z2: Float = tris[Int64(base &+ 8)]
    let area: Float = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    let left: Int32 = Int32(fmaxf(0.0, floorf(fminf(x0, fminf(x1, x2)))))
    let right: Int32 = Int32(fminf(Float(width) - 1.0, ceilf(fmaxf(x0, fmaxf(x1, x2)))))
    let top: Int32 = Int32(fmaxf(0.0, floorf(fminf(y0, fminf(y1, y2)))))
    let bottom: Int32 = Int32(fminf(Float(height) - 1.0, ceilf(fmaxf(y0, fmaxf(y1, y2)))))
    do {
        var y: Int32 = top
        while y <= bottom {
            do {
                var x: Int32 = left
                while x <= right {
                    let px: Float = Float(x) + 0.5
                    let py: Float = Float(y) + 0.5
                    let w0: Float = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
                    let w1: Float = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
                    let w2: Float = Float(1.0 - Double(w0) - Double(w1))
                    if w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0 {
                        let z: Float = w0 * z0 + w1 * z1 + w2 * z2
                        atomicMin(depth, y &* width &+ x, Int32(z * 1048575.0) &* 512 &+ t)
                    }
                    x = x &+ 1
                }
            }
            y = y &+ 1
        }
    }
    return 0
}

func shade(_ p: Int32, _ depth: MutableList<Int32>, _ tris: MutableList<Float>) -> Float {
    let key: Int32 = depth[Int64(p)]
    if key == 2147483647 {
        return 0.06
    }
    let id: Int32 = key % 512
    let z: Float = Float(key / 512) / 1048575.0
    return tris[Int64(id &* 10 &+ 9)] * Float(1.0 - 0.45 * Double(z))
}

func makeScene(_ tris: MutableList<Float>, _ count: Int64, _ width: Int64, _ height: Int64) -> Int64 {
    var seed: Int64 = 12345
    do {
        var t: Int64 = 0
        while t < count {
            let cx: Float = nextRandom(seed) * Float(width)
            seed = nextSeed(seed)
            let cy: Float = nextRandom(seed) * Float(height)
            seed = nextSeed(seed)
            let size: Float = Float(30.0 + Double(nextRandom(seed) * 110.0))
            seed = nextSeed(seed)
            do {
                var corner: Int64 = 0
                while corner < 3 {
                    tris[t &* 10 &+ corner &* 3] = cx + (nextRandom(seed) - 0.5) * 2.0 * size
                    seed = nextSeed(seed)
                    tris[(t &* 10 &+ corner &* 3) &+ 1] = cy + (nextRandom(seed) - 0.5) * 2.0 * size
                    seed = nextSeed(seed)
                    tris[(t &* 10 &+ corner &* 3) &+ 2] = nextRandom(seed)
                    seed = nextSeed(seed)
                    corner = corner &+ 1
                }
            }
            tris[t &* 10 &+ 9] = Float(0.25 + 0.75 * Double(nextRandom(seed)))
            seed = nextSeed(seed)
            t = t &+ 1
        }
    }
    return 0
}

func nextSeed(_ seed: Int64) -> Int64 {
    return (seed &* 1103515245 &+ 12345) % 2147483648
}

func nextRandom(_ seed: Int64) -> Float {
    return Float(seed / 65536 % 32768) / 32768.0
}
