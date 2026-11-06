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

func quietTone() -> Sound {
    let s: Sound = soundNew(0.1, 1000)
    s.tone(0, 125.0, 0.5, 0.0, 0.1)
    return s
}

func checkNewSoundIsMono() -> Double {
    return offInt(soundNew(1.0, 8000).channels, 1)
}

func checkNewSoundLength() -> Double {
    return offInt(soundNew(2.5, 8000).count, 20000)
}

func checkSeconds() -> Double {
    return off(soundNew(2.5, 8000).seconds(), 2.5)
}

func checkTonePeak() -> Double {
    return off(quietTone().peak(), 0.5)
}

func checkSquareTonePeak() -> Double {
    let s: Sound = soundNew(0.1, 1000)
    s.tone(1, 125.0, 0.4, 0.0, 0.1)
    return off(s.peak(), 0.4)
}

func checkScale() -> Double {
    let s: Sound = quietTone()
    s.scale(0.5)
    return off(s.peak(), 0.25)
}

func checkMixAdds() -> Double {
    let a: Sound = quietTone()
    a.add(quietTone(), 0.0, 1.0)
    return off(a.peak(), 1.0)
}

func checkNormalize() -> Double {
    let s: Sound = soundNew(0.1, 1000)
    s.tone(0, 125.0, 0.3, 0.0, 0.1)
    s.normalize(0.9)
    return off(s.peak(), 0.9)
}

func checkDelayedMixKeepsLength() -> Double {
    let a: Sound = soundNew(1.0, 1000)
    a.add(quietTone(), 0.5, 1.0)
    return offInt(a.count, 1000)
}

func checkDelayedMixIsSilentBefore() -> Double {
    let a: Sound = soundNew(1.0, 1000)
    a.add(quietTone(), 0.5, 1.0)
    return off(a.samples[499], 0.0)
}

func checkDelayedMixStartsLater() -> Double {
    let a: Sound = soundNew(1.0, 1000)
    a.add(quietTone(), 0.5, 1.0)
    return off(a.samples[525], 0.35355339)
}

func checkToneFadesIn() -> Double {
    let s: Sound = soundNew(0.5, 8000)
    s.tone(0, 440.0, 1.0, 0.0, 0.5)
    return off(s.samples[0], 0.0)
}

func checkNoiseAmplitude() -> Double {
    let s: Sound = soundNew(0.1, 8000)
    s.noise(0.25, 7)
    let loudest: Float = s.peak()
    if loudest > 0.25 {
        return 1.0
    }
    if loudest < 0.2 {
        return 1.0
    }
    return 0.0
}

func filterGain(_ kind: Int64, _ hz: Float, _ cutoff: Float) -> Float {
    let s: Sound = soundNew(1.0, 8000)
    let data: MutableList<Float> = s.samples
    do {
        var i: Int64 = 0
        while i < 8000 {
            data[i] = sine(hz * Float(i) / 8000.0)
            i = i &+ 1
        }
    }
    if kind == 0 {
        s.lowPass(cutoff)
    } else {
        s.highPass(cutoff)
    }
    var loudest: Float = 0.0
    do {
        var i: Int64 = 6000
        while i < 8000 {
            var level: Float = data[i]
            if level < 0.0 {
                level = Float(0.0 - Double(level))
            }
            if level > loudest {
                loudest = level
            }
            i = i &+ 1
        }
    }
    return loudest
}

func checkLowPassCutsHighTone() -> Double {
    return off(filterGain(0, 1000.0, 500.0), 0.3987)
}

func checkLowPassKeepsLowTone() -> Double {
    return off(filterGain(0, 200.0, 500.0), 0.9031)
}

func checkHighPassKeepsHighTone() -> Double {
    return off(filterGain(1, 1000.0, 500.0), 0.7225)
}

func checkHighPassCutsLowTone() -> Double {
    return off(filterGain(1, 200.0, 500.0), 0.3613)
}

func checkEchoFirstRepeat() -> Double {
    let s: Sound = soundNew(0.5, 1000)
    s.samples[0] = 1.0
    s.echo(0.1, 0.5)
    return off(s.samples[100], 0.5)
}

func checkEchoSecondRepeat() -> Double {
    let s: Sound = soundNew(0.5, 1000)
    s.samples[0] = 1.0
    s.echo(0.1, 0.5)
    return off(s.samples[200], 0.25)
}

func convolved() -> Sound {
    let a: Sound = soundNew(0.002, 1000)
    let r: Sound = soundNew(0.003, 1000)
    a.samples[0] = 1.0
    a.samples[1] = 2.0
    r.samples[0] = 1.0
    r.samples[2] = 3.0
    return a.convolve(r)
}

func checkConvolutionLength() -> Double {
    return offInt(convolved().count, 4)
}

func checkConvolutionValues() -> Double {
    let y: MutableList<Float> = convolved().samples
    return off(y[0], 1.0) + off(y[1], 2.0) + off(y[2], 3.0) + off(y[3], 6.0)
}

func reverbOfImpulse() -> Sound {
    let s: Sound = soundNew(1.0, 10000)
    s.samples[0] = 1.0
    s.reverb(1.0)
    return s
}

func checkReverbStart() -> Double {
    return off(reverbOfImpulse().samples[0], 0.20525)
}

func checkReverbFirstEcho() -> Double {
    return off(reverbOfImpulse().samples[297], 0.03924)
}
