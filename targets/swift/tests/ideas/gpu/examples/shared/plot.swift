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

func drawStems(_ pic: MutableList<Float>, _ w: Int64, _ h: Int64, _ values: MutableList<Float>, _ n: Int64, _ low: Float, _ high: Float) -> Int64 {
    do {
        var k: Int64 = 0
        while k < w &* h {
            pic[k] = 0.96
            k = k &+ 1
        }
    }
    let zero: Int64 = Int64(high / (high - low) * Float(h))
    do {
        var x: Int64 = 0
        while x < w {
            pic[zero &* w &+ x] = 0.55
            x = x &+ 1
        }
    }
    do {
        var j: Int64 = 0
        while j < n {
            let column: Int64 = Int64((Float(j) + 0.5) * Float(w) / Float(n))
            var top: Int64 = Int64((high - values[j]) / (high - low) * Float(h &- 1))
            if top < 0 {
                top = 0
            }
            if top > h &- 1 {
                top = h &- 1
            }
            let from: Int64 = minOf(top, zero)
            let to: Int64 = maxOf(top, zero)
            do {
                var y: Int64 = from
                while y <= to {
                    pic[y &* w &+ column] = 0.1
                    y = y &+ 1
                }
            }
            do {
                var dy: Int64 = 0 &- 1
                while dy <= 1 {
                    do {
                        var dx: Int64 = 0 &- 1
                        while dx <= 1 {
                            let yy: Int64 = top &+ dy
                            let xx: Int64 = column &+ dx
                            if yy >= 0 && yy < h && xx >= 0 && xx < w {
                                pic[yy &* w &+ xx] = 0.1
                            }
                            dx = dx &+ 1
                        }
                    }
                    dy = dy &+ 1
                }
            }
            j = j &+ 1
        }
    }
    return 0
}

func minOf(_ a: Int64, _ b: Int64) -> Int64 {
    if a < b {
        return a
    }
    return b
}

func maxOf(_ a: Int64, _ b: Int64) -> Int64 {
    if a > b {
        return a
    }
    return b
}

func drawWave(_ pic: MutableList<Float>, _ w: Int64, _ h: Int64, _ values: MutableList<Float>, _ n: Int64, _ low: Float, _ high: Float) -> Int64 {
    do {
        var k: Int64 = 0
        while k < w &* h {
            pic[k] = 0.96
            k = k &+ 1
        }
    }
    let zero: Int64 = Int64(high / (high - low) * Float(h))
    do {
        var x: Int64 = 0
        while x < w {
            pic[zero &* w &+ x] = 0.55
            x = x &+ 1
        }
    }
    do {
        var x: Int64 = 0
        while x < w {
            let from: Int64 = x &* n / w
            var upto: Int64 = (x &+ 1) &* n / w
            if upto <= from {
                upto = from &+ 1
            }
            var lowest: Float = values[from]
            var highest: Float = values[from]
            do {
                var j: Int64 = from
                while j < upto {
                    if values[j] < lowest {
                        lowest = values[j]
                    }
                    if values[j] > highest {
                        highest = values[j]
                    }
                    j = j &+ 1
                }
            }
            var top: Int64 = Int64((high - highest) / (high - low) * Float(h &- 1))
            var bottom: Int64 = Int64((high - lowest) / (high - low) * Float(h &- 1))
            if top < 0 {
                top = 0
            }
            if bottom > h &- 1 {
                bottom = h &- 1
            }
            do {
                var y: Int64 = top
                while y <= bottom {
                    pic[y &* w &+ x] = 0.1
                    y = y &+ 1
                }
            }
            x = x &+ 1
        }
    }
    return 0
}

func drawCurve(_ pic: MutableList<Float>, _ w: Int64, _ h: Int64, _ values: MutableList<Float>, _ n: Int64, _ low: Float, _ high: Float) -> Int64 {
    do {
        var k: Int64 = 0
        while k < w &* h {
            pic[k] = 0.96
            k = k &+ 1
        }
    }
    let zero: Int64 = Int64(high / (high - low) * Float(h))
    do {
        var x: Int64 = 0
        while x < w {
            pic[zero &* w &+ x] = 0.55
            x = x &+ 1
        }
    }
    var previous: Int64 = curveRow(values[0], h, low, high)
    do {
        var x: Int64 = 0
        while x < w {
            let row: Int64 = curveRow(values[x &* n / w], h, low, high)
            do {
                var y: Int64 = minOf(row, previous)
                while y <= maxOf(row, previous) {
                    pic[y &* w &+ x] = 0.1
                    y = y &+ 1
                }
            }
            previous = row
            x = x &+ 1
        }
    }
    return 0
}

func curveRow(_ value: Float, _ h: Int64, _ low: Float, _ high: Float) -> Int64 {
    var row: Int64 = Int64((high - value) / (high - low) * Float(h &- 1))
    if row < 0 {
        row = 0
    }
    if row > h &- 1 {
        row = h &- 1
    }
    return row
}
