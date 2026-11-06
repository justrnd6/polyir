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

// What `throw` throws: the code thrown.
struct Thrown: Error {
    let value: Int64
}

final class Acc: Hashable {
    var total: Int64
    var tag: String
    var items: MutableList<Int64>

    init(_ total: Int64, _ tag: String, _ items: MutableList<Int64>) {
        self.total = total
        self.tag = tag
        self.items = items
    }

    func add(_ x: Int64) -> Int64 {
        self.total = self.total &+ x
        self.items.append(x)
        return self.total
    }

    func later() -> () -> Int64 {
        return { () in self.total &* 2 }
    }

    func describe() -> String {
        return self.tag + ":" + self.tag
    }

    func self_() -> Acc {
        return self
    }

    func copy() -> Acc {
        Acc(total, tag, items)
    }

    static func == (a: Acc, b: Acc) -> Bool {
        [true, a.total == b.total, a.tag == b.tag, a.items == b.items].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(total)
        hasher.combine(tag)
        hasher.combine(items)
    }
}

protocol Animal: AnyObject {
    func legs(_ p0: Int64) -> Int64
    func name() -> String
}

final class Dog: Animal, Hashable {
    let age: Int64

    init(_ age: Int64) {
        self.age = age
    }

    func legs(_ n: Int64) -> Int64 {
        return 4 &* n &+ self.age
    }

    func name() -> String {
        return "dog"
    }

    static func == (a: Dog, b: Dog) -> Bool {
        [true, a.age == b.age].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(age)
    }
}

final class Bird: Animal, Hashable {
    let age: Int64

    init(_ age: Int64) {
        self.age = age
    }

    func legs(_ n: Int64) -> Int64 {
        return 2 &* n
    }

    func name() -> String {
        return "bird"
    }

    static func == (a: Bird, b: Bird) -> Bool {
        [true, a.age == b.age].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(age)
    }
}

func risky(_ x: Int64) throws -> Int64 {
    if x < 0 {
        throw Thrown(value: 7)
    }
    return x
}

func tryReturn(_ x: Int64) -> Int64 {
    do {
        return (try risky(x)) &+ 1
    } catch {
        let code = (error as! Thrown).value
        return code &* 100
    }
    return 0
}

func nested(_ x: Int64) -> Int64 {
    var r: Int64 = 0
    do {
        do {
            r = try risky(x)
        } catch {
            let a = (error as! Thrown).value
            r = a &+ 1
            throw Thrown(value: r)
        }
    } catch {
        let b = (error as! Thrown).value
        r = b &* 10
    }
    return r
}

func makeAdder(_ n: Int64) -> (Int64) -> Int64 {
    return { (x: Int64) in x &+ n }
}

func twice(_ f: @escaping (Int64) -> Int64, _ x: Int64) -> Int64 {
    return f(f(x))
}

func counterPair() -> Int64 {
    var c: Int64 = 0
    let inc: () -> Int64 = { () -> Int64 in
        c = c &+ 1
        return c
    }
    let get: () -> Int64 = { () in c }
    inc()
    inc()
    return get() &* 10 &+ c
}

func main() -> Int64 {
    var score: Int64 = 0
    let a: Acc = Acc(1, "ab", MutableList())
    score = (score &+ a.add(5)) &+ a.add(6)
    let f: () -> Int64 = a.later()
    score = score &+ f()
    score = score &+ a.items.count
    if a.describe() == "ab:ab" {
        score = score &+ 1000
    }
    let s: Acc = a.self_()
    if s === a {
        score = score &+ 2000
    }
    a.tag = "zz"
    if s.tag == "zz" {
        score = score &+ 3000
    }
    let zoo: MutableList<any Animal> = MutableList()
    zoo.append(Dog(3))
    zoo.append(Bird(1))
    do {
        var i: Int64 = 0
        while i < zoo.count {
            score = score &+ zoo[i].legs(2)
            if zoo[i].name() == "bird" {
                score = score &+ 7
            }
            i = i &+ 1
        }
    }
    score = (score &+ tryReturn(3)) &+ tryReturn(0 &- 1)
    score = (score &+ nested(4)) &+ nested(0 &- 2)
    let add3: (Int64) -> Int64 = makeAdder(3)
    score = score &+ twice(add3, 10)
    score = score &+ counterPair()
    return score % 251
}

exit(Int32(truncatingIfNeeded: main()))
