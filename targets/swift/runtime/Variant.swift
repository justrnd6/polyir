// A variant: the number of its case and the value it holds. Two compare by their cases and what their values say.
struct Variant: Equatable {
    let tag: Int64
    let value: Any?

    static func == (a: Variant, b: Variant) -> Bool {
        a.tag == b.tag && String(describing: a.value) == String(describing: b.value)
    }
}
