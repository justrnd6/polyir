// The functions a closure can be, by how many arguments they take: `Fn1<A, R>` gives an R, `Act1<A>` gives nothing.
interface Fn0<R> {
    R call();
}

interface Fn1<A, R> {
    R call(A a);
}

interface Fn2<A, B, R> {
    R call(A a, B b);
}

interface Fn3<A, B, C, R> {
    R call(A a, B b, C c);
}

interface Fn4<A, B, C, D, R> {
    R call(A a, B b, C c, D d);
}

interface Act0 {
    void call();
}

interface Act1<A> {
    void call(A a);
}

interface Act2<A, B> {
    void call(A a, B b);
}

interface Act3<A, B, C> {
    void call(A a, B b, C c);
}

interface Act4<A, B, C, D> {
    void call(A a, B b, C c, D d);
}
