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

func fftStage(_ t: Int32, _ src: MutableList<Float>, _ p: MutableList<Int32>, _ dst: MutableList<Float>) -> Int32 {
    let total: Int32 = p[0]
    let n: Int32 = p[1]
    let s: Int32 = p[2]
    let half: Int32 = total / 2
    let batch: Int32 = t / half
    let local: Int32 = t % half
    let position: Int32 = local / s
    let column: Int32 = local % s
    let base: Int32 = batch &* total &* 2
    let a: Int32 = base &+ Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: column &+ s &* position))
    let b: Int32 = base &+ Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: column &+ s &* (position &+ n / 2)))
    let sumRe: Float = src[Int64(a)] + src[Int64(b)]
    let sumIm: Float = src[Int64(a &+ 1)] + src[Int64(b &+ 1)]
    let diffRe: Float = src[Int64(a)] - src[Int64(b)]
    let diffIm: Float = src[Int64(a &+ 1)] - src[Int64(b &+ 1)]
    let angle: Float = Float((0.0 - 6.2831853) * Double(Float(p[3])) * Double(Float(position)) / Double(Float(n)))
    let twiddleRe: Float = cosf(angle)
    let twiddleIm: Float = sinf(angle)
    let first: Int32 = base &+ Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: column &+ s &* Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: position))))
    let second: Int32 = base &+ Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: column &+ s &* Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: position) &+ 1)))
    dst[Int64(first)] = sumRe
    dst[Int64(first &+ 1)] = sumIm
    dst[Int64(second)] = diffRe * twiddleRe - diffIm * twiddleIm
    dst[Int64(second &+ 1)] = diffRe * twiddleIm + diffIm * twiddleRe
    return 0
}

func fftMagnitude(_ k: Int32, _ data: MutableList<Float>, _ p: MutableList<Int32>) -> Float {
    let total: Int32 = p[0]
    let re: Float = data[2 &* Int64(truncatingIfNeeded: k)]
    let im: Float = data[2 &* Int64(truncatingIfNeeded: k) &+ 1]
    return Float(2.0 * Double(sqrtf(re * re + im * im)) / Double(Float(total)))
}

func fftFrames(_ t: Int32, _ signal: MutableList<Float>, _ p: MutableList<Int32>, _ dst: MutableList<Float>) -> Int32 {
    let total: Int32 = p[0]
    let frame: Int32 = t / total
    let k: Int32 = t % total
    let window: Float = Float(0.5 - 0.5 * Double(cosf(Float(6.2831853 * Double(Float(k)) / Double(Float(total &- 1))))))
    dst[2 &* Int64(truncatingIfNeeded: t)] = signal[Int64(frame &* p[1] &+ k)] * window
    dst[2 &* Int64(truncatingIfNeeded: t) &+ 1] = 0.0
    return 0
}

func spectrogramPixel(_ i: Int32, _ data: MutableList<Float>, _ p: MutableList<Int32>) -> Float {
    let total: Int32 = p[0]
    let frames: Int32 = p[1]
    let rows: Int32 = p[2]
    let x: Int32 = i % frames
    let bin: Int32 = (rows &- 1) &- i / frames
    let at: Int32 = x &* total &* 2 &+ Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: bin))
    let re: Float = data[Int64(at)]
    let im: Float = data[Int64(at &+ 1)]
    let magnitude: Float = Float(4.0 * Double(sqrtf(re * re + im * im)) / Double(Float(total)))
    var level: Float = Float((20.0 * Double(logf(magnitude + 0.0000001)) / 2.3025851 + 70.0) / 70.0)
    if level < 0.0 {
        level = 0.0
    }
    if level > 1.0 {
        level = 1.0
    }
    return level
}

func complexMultiply(_ k: Int32, _ a: MutableList<Float>, _ b: MutableList<Float>, _ out: MutableList<Float>) -> Int32 {
    out[2 &* Int64(truncatingIfNeeded: k)] = a[2 &* Int64(truncatingIfNeeded: k)] * b[2 &* Int64(truncatingIfNeeded: k)] - a[2 &* Int64(truncatingIfNeeded: k) &+ 1] * b[2 &* Int64(truncatingIfNeeded: k) &+ 1]
    out[2 &* Int64(truncatingIfNeeded: k) &+ 1] = a[2 &* Int64(truncatingIfNeeded: k)] * b[2 &* Int64(truncatingIfNeeded: k) &+ 1] + a[2 &* Int64(truncatingIfNeeded: k) &+ 1] * b[2 &* Int64(truncatingIfNeeded: k)]
    return 0
}

func realScaled(_ i: Int32, _ data: MutableList<Float>, _ p: MutableList<Float>) -> Float {
    return data[2 &* Int64(truncatingIfNeeded: i)] / p[0]
}
