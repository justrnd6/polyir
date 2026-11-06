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

func queens(_ t: Int32, _ prefixes: MutableList<Int32>, _ p: MutableList<Int32>, _ stack: MutableList<Int32>) -> Int32 {
    let n: Int32 = p[0]
    let depth: Int32 = p[1]
    let full: Int32 = Int32(truncatingIfNeeded: 1 << Int64(truncatingIfNeeded: n) &- 1)
    let base: Int32 = t &* (n &+ 1) &* 4
    stack[Int64(base)] = prefixes[Int64(t &* 3)]
    stack[Int64(base &+ 1)] = prefixes[Int64(t &* 3 &+ 1)]
    stack[Int64(base &+ 2)] = prefixes[Int64(t &* 3 &+ 2)]
    stack[Int64(base &+ 3)] = full & ~(prefixes[Int64(t &* 3)] | prefixes[Int64(t &* 3 &+ 1)] | prefixes[Int64(t &* 3 &+ 2)])
    var level: Int32 = 0
    var count: Int32 = 0
    do {
        var step: Int64 = 0
        while level >= 0 {
            let at: Int32 = base &+ level &* 4
            let free: Int32 = stack[Int64(at &+ 3)]
            if free == 0 {
                level = level &- 1
            } else {
                let bit: Int32 = free & Int32(truncatingIfNeeded: 0 &- Int64(truncatingIfNeeded: free))
                stack[Int64(at &+ 3)] = free ^ bit
                if (depth &+ level) &+ 1 == n {
                    count = count &+ 1
                } else {
                    let nextCols: Int32 = stack[Int64(at)] | bit
                    let nextLeft: Int32 = (stack[Int64(at &+ 1)] | bit) << 1 & full
                    let nextRight: Int32 = (stack[Int64(at &+ 2)] | bit) >> 1
                    level = level &+ 1
                    let below: Int32 = base &+ level &* 4
                    stack[Int64(below)] = nextCols
                    stack[Int64(below &+ 1)] = nextLeft
                    stack[Int64(below &+ 2)] = nextRight
                    stack[Int64(below &+ 3)] = full & ~(nextCols | nextLeft | nextRight)
                }
            }
            step = step &+ 1
        }
    }
    return count
}

func listPrefixes(_ n: Int64, _ depth: Int64, _ out: MutableList<Int32>) -> Int64 {
    var total: Int64 = 1
    var r: Int64 = 0
    while r < depth {
        total = total &* n
        r = r &+ 1
    }
    let full: Int32 = Int32(truncatingIfNeeded: 1 << n &- 1)
    var found: Int64 = 0
    do {
        var code: Int64 = 0
        while code < total {
            var left: Int32 = 0
            var right: Int32 = 0
            var cols: Int32 = 0
            var ok: Int64 = 1
            var rest: Int64 = code
            do {
                var r: Int64 = 0
                while r < depth {
                    let column: Int64 = rest % n
                    rest = rest / n
                    let bit: Int32 = Int32(truncatingIfNeeded: 1 << column)
                    if bit & (cols | left | right) != 0 {
                        ok = 0
                    }
                    cols = cols | bit
                    left = (left | bit) << 1 & full
                    right = (right | bit) >> 1
                    r = r &+ 1
                }
            }
            if ok == 1 {
                out[found &* 3] = cols
                out[found &* 3 &+ 1] = left
                out[found &* 3 &+ 2] = right
                found = found &+ 1
            }
            code = code &+ 1
        }
    }
    return found
}
