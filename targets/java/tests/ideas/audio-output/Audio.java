final class Audio {
    static long audioStreamAt(Fn4<Long, float[], Long, Long, Long> fill, long state, long channels, long rate, float seconds, float volume) {
        return macos.audioStreamRun(StreamFile.streamNew(fill, state, channels, rate, seconds, volume));
    }

    static long audioStream(Fn4<Long, float[], Long, Long, Long> fill, long state, long channels, long rate, float seconds) {
        return audioStreamAt(fill, state, channels, rate, seconds, 1.0f);
    }

    static long soundPlay(SoundFile.Sound s) {
        return macos.audioStreamRun(StreamFile.soundStream(s, 1.0f));
    }

    static long audioPlay(float[] samples, long count, long rate) {
        return soundPlay(new SoundFile.Sound(samples, count, rate, 1L));
    }

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
}
