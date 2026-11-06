// A copy of a list, map, set or record: a new one holding the same items.
function copyOf<T>(value: T): T {
  if (Array.isArray(value)) {
    return [...value] as T;
  }
  if (value instanceof Map || value instanceof Set) {
    return new (value.constructor as any)(value);
  }
  return Object.assign(Object.create(Object.getPrototypeOf(value)), value);
}
