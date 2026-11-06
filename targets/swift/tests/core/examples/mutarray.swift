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

// A push used as a value gives the list; a list shortened; a list, set or map with one more; two lists joined.
fileprivate func pushed<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    items.append(item)
    return items
}

fileprivate func truncate<T>(_ items: MutableList<T>, _ count: Int64) -> MutableList<T> {
    if Int(count) < items.items.count {
        items.items.removeLast(items.items.count - max(Int(count), 0))
    }
    return items
}

fileprivate func withItem<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    MutableList(items.items + [item])
}

fileprivate func withItem<T>(_ items: MutableSet<T>, _ item: T) -> MutableSet<T> {
    MutableSet(items.items.union([item]))
}

fileprivate func withEntry<K, V>(_ table: MutableMap<K, V>, _ key: K, _ value: V) -> MutableMap<K, V> {
    let out = MutableMap(table.items)
    out[key] = value
    return out
}

fileprivate func concat<T>(_ a: MutableList<T>, _ b: MutableList<T>) -> MutableList<T> {
    MutableList(a.items + b.items)
}

func sumPushed(_ a: Int64, _ b: Int64, _ c: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList()
    arr.append(a)
    arr.append(b)
    arr.append(c)
    arr.append((a &+ b) &+ c)
    return ((arr[0] &+ arr[1]) &+ arr[2]) &+ arr[3]
}

func lenCheck(_ a: Int64, _ b: Int64, _ c: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList()
    arr.append(a)
    arr.append(b)
    arr.append(c)
    return arr.count
}

func sumSquaresUpTo(_ n: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        while i < n {
            arr.append(i &* i)
            i = i &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < arr.count {
            total = total &+ arr[i]
            i = i &+ 1
        }
    }
    return total
}

func sumEvensBelow(_ n: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        while i < n {
            if i % 2 == 0 {
                arr.append(i)
            }
            i = i &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < arr.count {
            total = total &+ arr[i]
            i = i &+ 1
        }
    }
    return total
}

func fillSquares(_ arr: MutableList<Int64>, _ count: Int64) -> Int64 {
    do {
        var i: Int64 = 0
        while i < count {
            arr.append(i &* i)
            i = i &+ 1
        }
    }
    return count
}

func squaresThroughAHelper(_ count: Int64) -> Int64 {
    let arr: MutableList<Int64> = MutableList()
    fillSquares(arr, count)
    return arr.count &* 1000 &+ arr[count &- 1]
}

func pushGivesTheArrayBack() -> Int64 {
    var arr: MutableList<Int64> = MutableList()
    arr = pushed(arr, 10)
    arr = pushed(arr, 20)
    return arr[0] &+ arr[1]
}

func sumEvensBelowExplicit(_ n: Int64) -> Int64 {
    var label: Int64 = 0
    var i: Int64 = 0
    var arr: MutableList<Int64> = MutableList()
    machine: while true {
        switch label {
        case 0:
            let empty: MutableList<Int64> = MutableList()
            i = 0
            arr = empty
            label = 1
            continue machine
        case 1:
            if i < n {
                label = 2
                continue machine
            } else {
                label = 5
                continue machine
            }
        case 2:
            if i % 2 == 0 {
                label = 3
                continue machine
            } else {
                label = 4
                continue machine
            }
        case 3:
            var next_i: Int64 = i &+ 1
            var next_arr: MutableList<Int64> = pushed(arr, i)
            i = next_i
            arr = next_arr
            label = 1
            continue machine
        case 4:
            i = i &+ 1
            label = 1
            continue machine
        case 5:
            var i: Int64 = 0
            var total: Int64 = 0
            while i < arr.count {
                i = i &+ 1
                total = total &+ arr[i]
            }
            return total
        default:
            fatalError("no block \(label)")
        }
    }
}
