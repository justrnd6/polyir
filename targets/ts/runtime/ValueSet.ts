// A Set of records, lists or sets compared by what they hold: the first one added stands for all values equal to it.
class ValueSet<T = any> extends Set<T> {
  private byKey = new Map<string, T>();

  constructor(items?: Iterable<T>) {
    super();
    for (const item of items ?? []) {
      this.add(item);
    }
  }

  add(item: T): this {
    const key = valueKey(item);
    if (!this.byKey.has(key)) {
      this.byKey.set(key, item);
      super.add(item);
    }
    return this;
  }

  has(item: T): boolean {
    return this.byKey.has(valueKey(item));
  }

  delete(item: T): boolean {
    const key = valueKey(item);
    const held = this.byKey.get(key) as T;
    return this.byKey.delete(key) && super.delete(held);
  }
}
