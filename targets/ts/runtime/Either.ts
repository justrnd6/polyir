// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> {
  private constructor(readonly isOk: boolean, private readonly held: T | E) {}

  static ok<T, E>(value: T): Either<T, E> {
    return new Either<T, E>(true, value);
  }

  static err<T, E>(value: E): Either<T, E> {
    return new Either<T, E>(false, value);
  }

  get okValue(): T {
    return this.held as T;
  }

  get errValue(): E {
    return this.held as E;
  }
}
