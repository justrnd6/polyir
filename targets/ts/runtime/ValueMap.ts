// A Map whose keys are records, lists or sets compared by what they hold: the first key set stands for all keys equal to it.
class ValueMap<K = any, V = any> extends Map<K, V> {
  private byKey = new Map<string, K>();

  constructor(entries?: Iterable<[K, V]>) {
    super();
    for (const [key, value] of entries ?? []) {
      this.set(key, value);
    }
  }

  set(key: K, value: V): this {
    const text = valueKey(key);
    if (!this.byKey.has(text)) {
      this.byKey.set(text, key);
    }
    return super.set(this.byKey.get(text) as K, value);
  }

  get(key: K): V | undefined {
    return super.get(this.byKey.get(valueKey(key)) as K);
  }

  has(key: K): boolean {
    return this.byKey.has(valueKey(key));
  }

  delete(key: K): boolean {
    const text = valueKey(key);
    const held = this.byKey.get(text) as K;
    return this.byKey.delete(text) && super.delete(held);
  }
}
