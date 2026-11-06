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

func pairSum(_ i: Int32, _ src: MutableList<Float>, _ n: Int32) -> Float {
    let a: Int32 = Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: i))
    var total: Float = src[Int64(a)]
    if a &+ 1 < n {
        total = total + src[Int64(a &+ 1)]
    }
    return total
}

func pairSumInt(_ i: Int32, _ src: MutableList<Int32>, _ n: Int32) -> Int32 {
    let a: Int32 = Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: i))
    var total: Int32 = src[Int64(a)]
    if a &+ 1 < n {
        total = total &+ src[Int64(a &+ 1)]
    }
    return total
}

func pairMax(_ i: Int32, _ src: MutableList<Float>, _ n: Int32) -> Float {
    let a: Int32 = Int32(truncatingIfNeeded: 2 &* Int64(truncatingIfNeeded: i))
    var best: Float = src[Int64(a)]
    if a &+ 1 < n {
        if src[Int64(a &+ 1)] > best {
            best = src[Int64(a &+ 1)]
        }
    }
    return best
}

func passCount(_ n: Int64) -> Int64 {
    var passes: Int64 = 0
    var left: Int64 = n
    do {
        var k: Int64 = 0
        while k < 40 {
            if left > 1 {
                left = (left &+ 1) / 2
                passes = passes &+ 1
            }
            k = k &+ 1
        }
    }
    return passes
}

func cpuSum(_ values: MutableList<Float>, _ n: Int64) -> Float {
    var total: Float = 0.0
    do {
        var i: Int64 = 0
        while i < n {
            total = total + values[i]
            i = i &+ 1
        }
    }
    return total
}

func cpuMax(_ values: MutableList<Float>, _ n: Int64) -> Float {
    var best: Float = values[0]
    do {
        var i: Int64 = 1
        while i < n {
            if values[i] > best {
                best = values[i]
            }
            i = i &+ 1
        }
    }
    return best
}

func fillTestData(_ values: MutableList<Float>, _ n: Int64) -> Int64 {
    do {
        var i: Int64 = 0
        while i < n {
            values[i] = Float(i % 7)
            i = i &+ 1
        }
    }
    values[n / 3] = 1000.0
    return 0
}
