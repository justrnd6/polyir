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

// Stops the program with a message, as polyir's `fail` does.
fileprivate func fail(_ message: String) -> Never {
    FileHandle.standardError.write(("trap: " + message + "\n").data(using: .utf8)!)
    exit(1)
}

final class Shape: Hashable {
    let side: Int64
    let name: String
    let tags: MutableList<String>

    init(_ side: Int64, _ name: String, _ tags: MutableList<String>) {
        self.side = side
        self.name = name
        self.tags = tags
    }

    func size() -> Size {
        return Size(self.side &* self.side)
    }

    static func square(_ side: Int64) -> Shape {
        return builder().side(side).name("square").build()
    }

    static func builder() -> Builder {
        return Builder(nil, "shape", MutableList())
    }

    final class Size: Hashable {
        let area: Int64

        init(_ area: Int64) {
            self.area = area
        }

        static func == (a: Size, b: Size) -> Bool {
            [true, a.area == b.area].allSatisfy { $0 }
        }

        func hash(into hasher: inout Hasher) {
            hasher.combine(area)
        }
    }

    final class Builder: Hashable {
        var __side: Int64?
        var __name: String
        var __tags: MutableList<String>

        init(_ __side: Int64?, _ __name: String, _ __tags: MutableList<String>) {
            self.__side = __side
            self.__name = __name
            self.__tags = __tags
        }

        func side(_ value: Int64) -> Builder {
            self.__side = value
            return self
        }

        func name(_ value: String) -> Builder {
            self.__name = value
            return self
        }

        func tags(_ value: MutableList<String>) -> Builder {
            self.__tags = value
            return self
        }

        func build() -> Shape {
            if self.__side == nil {
                fail("Shape.build: side is not set")
            }
            return Shape(self.__side!, self.__name, self.__tags)
        }

        func copy() -> Builder {
            Builder(__side, __name, __tags)
        }

        static func == (a: Builder, b: Builder) -> Bool {
            [true, a.__side == b.__side, a.__name == b.__name, a.__tags == b.__tags].allSatisfy { $0 }
        }

        func hash(into hasher: inout Hasher) {
            hasher.combine(__side)
            hasher.combine(__name)
            hasher.combine(__tags)
        }
    }

    static func == (a: Shape, b: Shape) -> Bool {
        [true, a.side == b.side, a.name == b.name, a.tags == b.tags].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(side)
        hasher.combine(name)
        hasher.combine(tags)
    }
}

func tagged(_ b: Shape.Builder) -> Shape.Builder {
    let tags: MutableList<String> = MutableList(["red", "big"])
    return b.tags(tags)
}

func main() -> Void {
    let first: Shape = tagged(Shape.builder()).side(2).build()
    let second: Shape = Shape.square(3)
    let size: Shape.Size = second.size()
    print("\(first.name) \(first.side) \(first.size().area) [\(first.tags[0]), \(first.tags[1])] \(second.name) \(second.side) \(size.area)")
}

main()
