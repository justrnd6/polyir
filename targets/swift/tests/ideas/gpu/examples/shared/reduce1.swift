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

func atomicAddFloat(_ sums: MutableList<Float>, _ slot: Int32, _ amount: Float) -> Float {
    let old: Float = sums[Int64(slot)]
    sums[Int64(slot)] = old + amount
    return old
}

func groupSize() -> Int32 {
    return 1
}

func localIndex(_ i: Int32) -> Int32 {
    return 0
}

func groupIndex(_ i: Int32) -> Int32 {
    return i
}

func barrier() -> Int32 {
    return 0
}

func blockSum(_ i: Int32, _ src: MutableList<Float>, _ n: Int32, _ tile: MutableList<Float>, _ partial: MutableList<Float>) -> Int32 {
    let lane: Int32 = localIndex(i)
    if i < n {
        tile[Int64(lane)] = src[Int64(i)]
    } else {
        tile[Int64(lane)] = 0.0
    }
    barrier()
    do {
        var s: Int32 = groupSize() / 2
        while s > 0 {
            if lane < s {
                tile[Int64(lane)] = tile[Int64(lane)] + tile[Int64(lane &+ s)]
            }
            barrier()
            s = s / 2
        }
    }
    if lane == 0 {
        partial[Int64(groupIndex(i))] = tile[0]
    }
    return 0
}

func blockMax(_ i: Int32, _ src: MutableList<Float>, _ n: Int32, _ tile: MutableList<Float>, _ partial: MutableList<Float>) -> Int32 {
    let lane: Int32 = localIndex(i)
    if i < n {
        tile[Int64(lane)] = src[Int64(i)]
    } else {
        tile[Int64(lane)] = src[0]
    }
    barrier()
    do {
        var s: Int32 = groupSize() / 2
        while s > 0 {
            if lane < s {
                tile[Int64(lane)] = fmaxf(tile[Int64(lane)], tile[Int64(lane &+ s)])
            }
            barrier()
            s = s / 2
        }
    }
    if lane == 0 {
        partial[Int64(groupIndex(i))] = tile[0]
    }
    return 0
}

func total(_ i: Int32, _ src: MutableList<Float>, _ n: Int32, _ tile: MutableList<Float>, _ sum: MutableList<Float>) -> Int32 {
    let lane: Int32 = localIndex(i)
    if i < n {
        tile[Int64(lane)] = src[Int64(i)]
    } else {
        tile[Int64(lane)] = 0.0
    }
    barrier()
    do {
        var s: Int32 = groupSize() / 2
        while s > 0 {
            if lane < s {
                tile[Int64(lane)] = tile[Int64(lane)] + tile[Int64(lane &+ s)]
            }
            barrier()
            s = s / 2
        }
    }
    if lane == 0 {
        atomicAddFloat(sum, 0, tile[0])
    }
    return 0
}

func groupsFor(_ n: Int64) -> Int64 {
    return (n &+ 255) / 256
}

func launchCount(_ n: Int64) -> Int64 {
    var launches: Int64 = 0
    var left: Int64 = n
    do {
        var k: Int64 = 0
        while k < 10 {
            if left > 1 {
                left = groupsFor(left)
                launches = launches &+ 1
            }
            k = k &+ 1
        }
    }
    return launches
}
