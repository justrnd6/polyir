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

var visits: Int64 = 0

final class Account: Hashable {
    var balance: Int64
    let owner: String

    init(_ balance: Int64, _ owner: String) {
        self.balance = balance
        self.owner = owner
    }

    func deposit(_ amount: Int64) -> Int64 {
        self.balance = self.balance &+ amount
        return self.balance
    }

    func depositTwice(_ amount: Int64) -> Int64 {
        self.deposit(amount)
        return self.deposit(amount)
    }

    func copy() -> Account {
        Account(balance, owner)
    }

    static func == (a: Account, b: Account) -> Bool {
        [true, a.balance == b.balance, a.owner == b.owner].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(balance)
        hasher.combine(owner)
    }
}

final class Vec2: Hashable {
    let x: Double
    let y: Double

    init(_ x: Double, _ y: Double) {
        self.x = x
        self.y = y
    }

    func length2() -> Double {
        return self.x * self.x + self.y * self.y
    }

    func scaled(_ k: Double) -> Vec2 {
        return Vec2(self.x * k, self.y * k)
    }

    static func == (a: Vec2, b: Vec2) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

func fib(_ n: Int64) -> Int64 {
    return n < 2 ? n : fib(n &- 1) &+ fib(n &- 2)
}

func classify(_ n: Int64) -> Int64 {
    return n < 0 ? 1 : n == 0 ? 2 : n < 10 ? 3 : 4
}

func bump() -> Int64 {
    visits = visits &+ 1
    return visits
}

func greeting(_ name: String) -> String {
    return "hello, " + name
}

func narrow(_ a: Int32, _ b: Int32) -> Int32 {
    return a &* b &+ 7
}

func apply(_ f: @escaping (Int64) -> Int64, _ x: Int64) -> Int64 {
    return f(x) &+ f(x &+ 1)
}

func main() -> Int64 {
    var score: Int64 = 0
    score = score &+ fib(10)
    let acct: Account = Account(100, "ann")
    score = score &+ acct.depositTwice(5)
    let v: Vec2 = Vec2(3.0, 4.0)
    let w: Vec2 = v.scaled(2.0)
    score = score &+ Int64(w.length2())
    if greeting("bo") == "hello, bo" {
        score = score &+ 1
    }
    score = score &+ Int64(truncatingIfNeeded: narrow(6, 7))
    let seen: MutableSet<Int64> = MutableSet()
    do {
        var i: Int64 = 0
        while i < 20 {
            seen.items.insert(i % 7)
            i = i &+ 1
        }
    }
    score = score &+ seen.count
    let nums: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        var j: Int64 = 10
        while i < 5 {
            nums.append(i &* j)
            i = i &+ 1
            j = j &- 1
        }
    }
    nums[0] = 100
    score = ((score &+ nums[0]) &+ nums[4]) &+ nums.count
    let ages: MutableMap<String, Int64> = MutableMap()
    ages["ann"] = 31
    if ages["ann"] != nil {
        score = score &+ (ages["ann"] ?? 0)
    }
    if (ages["zed"] != nil) == false {
        score = score &+ 1
    }
    let factor: Int64 = 3
    score = score &+ apply({ (x: Int64) in x &* factor }, 4)
    let b: Box<Double> = Box(2.5)
    score = score &+ Int64(b.value * 4.0)
    score = (((score &+ classify(0 &- 5)) &+ classify(0)) &+ classify(7)) &+ classify(70)
    score = (score &+ bump()) &+ bump()
    return score
}

final class Box<T: Hashable>: Hashable {
    let value: T

    init(_ value: T) {
        self.value = value
    }

    static func == (a: Box, b: Box) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}

exit(Int32(truncatingIfNeeded: main()))
