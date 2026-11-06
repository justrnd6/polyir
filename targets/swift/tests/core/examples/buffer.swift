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

func fillToCapacity() -> Int64 {
    let arr: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    arr[0] = 10
    arr[1] = 20
    arr[2] = 30
    arr[3] = 40
    return ((arr[0] &+ arr[1]) &+ arr[2]) &+ arr[3]
}

func capacityCheck() -> Int64 {
    let arr: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    arr[0] = 1
    arr[1] = 2
    return arr.count
}

func letBoundIndexedFill() -> Int64 {
    let arr: MutableList<Int64> = MutableList(Array(repeating: 0, count: 3))
    arr[0] = 5
    arr[1] = 6
    arr[2] = 7
    return (arr[0] &+ arr[1]) &+ arr[2]
}

func sumViaLoop(_ n: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList(Array(repeating: 0, count: 10))
    do {
        var i: Int64 = 0
        while i < n {
            arr[i] = i
            i = i &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < n {
            total = total &+ arr[i]
            i = i &+ 1
        }
    }
    return total
}

func stringElements() -> Int64 {
    let names: MutableList<String> = MutableList(Array(repeating: "", count: 2))
    names[0] = "first"
    names[1] = "second"
    if names[0] != "first" {
        return 0
    }
    if names[1] != "second" {
        return 0
    }
    return 1
}

func sumParam(_ arr: MutableList<Int64>) -> Int64 {
    return ((arr[0] &+ arr[1]) &+ arr[2]) &+ arr.count
}

func viaParam() -> Int64 {
    let arr: MutableList<Int64> = MutableList(Array(repeating: 0, count: 3))
    arr[0] = 100
    arr[1] = 200
    arr[2] = 300
    return sumParam(arr)
}
