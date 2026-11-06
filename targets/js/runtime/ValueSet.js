// A Set of records, lists or sets compared by what they hold: the first one added stands for all values equal to it.
class ValueSet extends Set {
  constructor(items) {
    super();
    this.byKey = new Map();
    for (const item of items ?? []) {
      this.add(item);
    }
  }

  add(item) {
    const key = valueKey(item);
    if (!this.byKey.has(key)) {
      this.byKey.set(key, item);
      super.add(item);
    }
    return this;
  }

  has(item) {
    return this.byKey.has(valueKey(item));
  }

  delete(item) {
    const key = valueKey(item);
    const held = this.byKey.get(key);
    return this.byKey.delete(key) && super.delete(held);
  }
}
