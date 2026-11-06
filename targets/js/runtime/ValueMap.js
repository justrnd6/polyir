// A Map whose keys are records, lists or sets compared by what they hold: the first key set stands for all keys equal to it.
class ValueMap extends Map {
  constructor(entries) {
    super();
    this.byKey = new Map();
    for (const [key, value] of entries ?? []) {
      this.set(key, value);
    }
  }

  set(key, value) {
    const text = valueKey(key);
    if (!this.byKey.has(text)) {
      this.byKey.set(text, key);
    }
    return super.set(this.byKey.get(text), value);
  }

  get(key) {
    return super.get(this.byKey.get(valueKey(key)));
  }

  has(key) {
    return this.byKey.has(valueKey(key));
  }

  delete(key) {
    const text = valueKey(key);
    const held = this.byKey.get(text);
    return this.byKey.delete(text) && super.delete(held);
  }
}
