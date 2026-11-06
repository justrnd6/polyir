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

func collisionAndOverwrite() -> Int64 {
    let hm: MutableMap<Int64, Int64> = MutableMap()
    hm[0] = 100
    hm[5] = 500
    hm[1] = 10
    hm[1] = 20
    return ((hm[0] ?? 0) &* 10000 &+ (hm[5] ?? 0) &* 10) &+ (hm[1] ?? 0)
}

func missingKeyIsZero() -> Int64 {
    let hm: MutableMap<Int64, Int64> = MutableMap()
    hm[0] = 7
    return hm[42] ?? 0
}

func sizeCheck() -> Int64 {
    let hm: MutableMap<Int64, Int64> = MutableMap()
    hm[0] = 1
    hm[1] = 2
    hm[1] = 99
    return hm.count &* 100
}

func stringKeyedWriteAndReadBack() -> Int64 {
    let hm: MutableMap<String, Int64> = MutableMap()
    hm["one"] = 1
    hm["two"] = 2
    hm["one"] = 11
    return (hm["one"] ?? 0) &* 100 &+ (hm["two"] ?? 0)
}

func stringKeyMissingIsZero() -> Int64 {
    let hm: MutableMap<String, Int64> = MutableMap()
    hm["present"] = 5
    return hm["absent"] ?? 0
}

func hashmapPtrValueMissIsSafe() -> Int64 {
    let hm: MutableMap<Int64, String> = MutableMap()
    hm[1] = "present"
    let missing: String = hm[99] ?? ""
    return 1
}
