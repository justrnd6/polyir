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

func fftStagesGpu(_ device: Int64, _ stage: Int64, _ first: Int64, _ second: Int64, _ total: Int64, _ batch: Int64, _ direction: Int64) -> Int64 {
    var source: Int64 = first
    var target: Int64 = second
    let scratch: Int64 = metalEmptyBuffer(device, batch &* (total / 2) &* 4)
    var n: Int64 = total
    var s: Int64 = 1
    do {
        var pass: Int64 = 0
        while pass < 30 {
            if n > 1 {
                let settings: MutableList<Int32> = MutableList(Array(repeating: 0, count: 4))
                settings[0] = Int32(truncatingIfNeeded: total)
                settings[1] = Int32(truncatingIfNeeded: n)
                settings[2] = Int32(truncatingIfNeeded: s)
                settings[3] = Int32(truncatingIfNeeded: direction)
                if metalDispatch(device, stage, batch &* (total / 2), source, metalBuffer(device, Int64(settings), 16), target, scratch) == 0 {
                    return 0
                }
                let swap: Int64 = source
                source = target
                target = swap
                n = n / 2
                s = s &* 2
            }
            pass = pass &+ 1
        }
    }
    return source
}
