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

func transform(_ p: [Float], _ m: MutableList<Float>) -> [Float] {
    return [m[0] * p[Int(0)] + m[1] * p[Int(1)] + m[2] * p[Int(2)] + m[3] * p[Int(3)], m[4] * p[Int(0)] + m[5] * p[Int(1)] + m[6] * p[Int(2)] + m[7] * p[Int(3)], m[8] * p[Int(0)] + m[9] * p[Int(1)] + m[10] * p[Int(2)] + m[11] * p[Int(3)], m[12] * p[Int(0)] + m[13] * p[Int(1)] + m[14] * p[Int(2)] + m[15] * p[Int(3)]]
}

func identity(_ m: MutableList<Float>) -> Int64 {
    do {
        var k: Int64 = 0
        while k < 16 {
            m[k] = 0.0
            k = k &+ 1
        }
    }
    m[0] = 1.0
    m[5] = 1.0
    m[10] = 1.0
    m[15] = 1.0
    return 0
}

func multiply4(_ a: MutableList<Float>, _ b: MutableList<Float>, _ out: MutableList<Float>) -> Int64 {
    do {
        var row: Int64 = 0
        while row < 4 {
            do {
                var col: Int64 = 0
                while col < 4 {
                    var sum: Float = 0.0
                    do {
                        var k: Int64 = 0
                        while k < 4 {
                            sum = sum + a[row &* 4 &+ k] * b[k &* 4 &+ col]
                            k = k &+ 1
                        }
                    }
                    out[row &* 4 &+ col] = sum
                    col = col &+ 1
                }
            }
            row = row &+ 1
        }
    }
    return 0
}
