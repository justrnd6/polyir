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

func histTest() -> Int64 {
    let hist: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    hist[0] = 10
    hist[1] = 20
    hist[2] = 30
    hist[3] = 40
    return hist[1] &+ hist[2]
}

func lenCheck() -> Int64 {
    let hist: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    return hist.count
}

func sumFixed(_ arr: MutableList<Int64>) -> Int64 {
    return ((arr[0] &+ arr[1]) &+ arr[2]) &+ arr[3]
}

func lenOnParam(_ arr: MutableList<Int64>) -> Int64 {
    return arr.count
}

func callWithLocal() -> Int64 {
    let hist: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    hist[0] = 1
    hist[1] = 2
    hist[2] = 3
    hist[3] = 4
    return sumFixed(hist) &* 100 &+ lenOnParam(hist)
}
