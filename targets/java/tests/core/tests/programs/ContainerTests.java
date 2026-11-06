import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;

final class ContainerTests {
    record Bag(ArrayList<Long> items, long total) {
    }

    record Pt(long x, long y) {
    }

    static long pushThree(ArrayList<Long> xs) {
        xs.add(3L);
        return (long) xs.size();
    }

    static long pushMany(ArrayList<Long> xs, long count) {
        for (long i = 0L; i < count; i = i + 1L) {
            xs.add(i * 2L);
        }
        return (long) xs.size();
    }

    static double checkAPushInsideAFunctionIsSeenByTheCaller() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(1L);
        pushThree(xs);
        return Check.offInt((long) xs.size(), 2L) + Check.offInt(xs.get(1), 3L);
    }

    static double checkManyPushesInsideAFunctionLeaveTheCallerValid() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(7L);
        pushMany(xs, 100L);
        return Check.offInt((long) xs.size(), 101L) + Check.offInt(xs.get(0), 7L) + Check.offInt(xs.get(1), 0L) + Check.offInt(xs.get(100), 198L);
    }

    static double checkTwoNamesForOneDarrayShareIt() {
        final ArrayList<Long> a = new ArrayList<>();
        final ArrayList<Long> b = a;
        a.add(5L);
        b.add(6L);
        return Check.offInt((long) a.size(), 2L) + Check.offInt((long) b.size(), 2L) + Check.offInt(a.get(1), 6L) + Check.offInt(b.get(0), 5L);
    }

    static double checkAnElementWrittenInAFunctionIsSeenByTheCaller() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(1L);
        xs.add(2L);
        overwriteFirst(xs, 40L);
        return Check.offInt(xs.get(0), 40L) + Check.offInt(xs.get(1), 2L);
    }

    static long overwriteFirst(ArrayList<Long> xs, long value) {
        xs.set(0, value);
        return 0L;
    }

    static ArrayList<Long> makeFilled(long count) {
        final ArrayList<Long> xs = new ArrayList<>();
        for (long i = 0L; i < count; i = i + 1L) {
            xs.add(i + 1L);
        }
        return xs;
    }

    static double checkAReturnedDarrayIsTheOneTheFunctionBuilt() {
        final ArrayList<Long> xs = makeFilled(4L);
        xs.add(50L);
        return Check.offInt((long) xs.size(), 5L) + Check.offInt(xs.get(3), 4L) + Check.offInt(xs.get(4), 50L);
    }

    static double checkAStructFieldIsTheSameDarray() {
        final ArrayList<Long> xs = new ArrayList<>();
        final Bag bag = new Bag(xs, 0L);
        xs.add(8L);
        bag.items.add(9L);
        return Check.offInt((long) bag.items.size(), 2L) + Check.offInt((long) xs.size(), 2L) + Check.offInt(bag.items.get(1), 9L);
    }

    static double checkAMethodStylePushOnAFieldWorks() {
        final Bag bag = new Bag(new ArrayList<>(), 0L);
        bag.items.add(4L);
        bag.items.add(5L);
        return Check.offInt((long) bag.items.size(), 2L) + Check.offInt(bag.items.get(1), 5L);
    }

    static double checkConcatenationMakesANewDarray() {
        final ArrayList<Long> a = new ArrayList<>();
        final ArrayList<Long> b = new ArrayList<>();
        a.add(1L);
        b.add(2L);
        final ArrayList<Long> c = concat(a, b);
        a.add(99L);
        return Check.offInt((long) c.size(), 2L) + Check.offInt(c.get(0), 1L) + Check.offInt(c.get(1), 2L) + Check.offInt((long) a.size(), 2L);
    }

    static double checkAClosureSharesTheDarrayItCaptures() {
        final ArrayList<Long> xs = new ArrayList<>();
        final Fn1<Long, Long> add = (v) -> {
            xs.add(v);
            return (long) xs.size();
        };
        add.call(10L);
        add.call(20L);
        return Check.offInt((long) xs.size(), 2L) + Check.offInt(xs.get(1), 20L);
    }

    static double checkPushGivesBackTheSameDarray() {
        ArrayList<Long> xs = new ArrayList<>();
        xs = pushed(xs, 5L);
        xs = pushed(xs, 6L);
        return Check.offInt((long) xs.size(), 2L) + Check.offInt(xs.get(1), 6L);
    }

    static double checkReassigningAVarSwitchesDarrays() {
        ArrayList<Long> xs = new ArrayList<>();
        final ArrayList<Long> other = new ArrayList<>();
        other.add(1L);
        other.add(2L);
        other.add(3L);
        xs = other;
        xs.add(4L);
        return Check.offInt((long) other.size(), 4L) + Check.offInt((long) xs.size(), 4L);
    }

    static double checkAListOfListsSharesItsInnerLists() {
        final ArrayList<ArrayList<Long>> outer = new ArrayList<>();
        final ArrayList<Long> a = new ArrayList<>();
        a.add(1L);
        a.add(2L);
        outer.add(a);
        final ArrayList<Long> b = new ArrayList<>();
        b.add(10L);
        outer.add(b);
        a.add(3L);
        final ArrayList<Long> first = outer.get(0);
        final ArrayList<Long> second = outer.get(1);
        return Check.offInt((long) outer.size(), 2L) + Check.offInt((long) first.size(), 3L) + Check.offInt(first.get(2), 3L) + Check.offInt(second.get(0), 10L);
    }

    static double checkADarrayOfStructsKeepsTheStructs() {
        final ArrayList<Pt> ps = new ArrayList<>();
        ps.add(new Pt(1L, 2L));
        ps.add(new Pt(3L, 4L));
        final Pt p = ps.get(1);
        return Check.offInt((long) ps.size(), 2L) + Check.offInt(p.x, 3L) + Check.offInt(p.y, 4L);
    }

    static double checkAThousandPushesKeepEveryElement() {
        final ArrayList<Long> xs = new ArrayList<>();
        for (long i = 0L; i < 1000L; i = i + 1L) {
            xs.add(i);
        }
        long total = 0L;
        for (long i = 0L; i < (long) xs.size(); i = i + 1L) {
            total = total + xs.get((int) i);
        }
        return Check.offInt((long) xs.size(), 1000L) + Check.offInt(total, 499500L);
    }

    static double checkAListInsideAnEitherIsTheSameList() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(4L);
        final Either<ArrayList<Long>, Long> r = Either.<ArrayList<Long>, Long>ok(xs);
        xs.add(5L);
        final ArrayList<Long> back = r.okValue();
        return Check.offInt((long) back.size(), 2L) + Check.offInt(back.get(1), 5L);
    }

    static long fillSparse(Sparse<Long> sp) {
        sp.set(5L, 1L, 1L);
        sp.set(7L, 2L, 3L);
        return 0L;
    }

    static double checkASparseWrittenInAFunctionIsSeenByTheCaller() {
        final Sparse<Long> sp = new Sparse<>(0L);
        fillSparse(sp);
        return Check.offInt(sp.get(1L, 1L), 5L) + Check.offInt(sp.get(2L, 3L), 7L) + Check.offInt(sp.get(9L, 9L), 0L) + Check.offInt((long) sp.size(), 2L);
    }

    static double checkLastWriteWinsForASparse() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(1L, 3L, 3L);
        sp.set(2L, 3L, 3L);
        sp.set(3L, 3L, 3L);
        return Check.offInt(sp.get(3L, 3L), 3L);
    }

    static double checkOneDimensionalSparseSharesToo() {
        final Sparse<Long> sp = new Sparse<>(0L);
        final Sparse<Long> alias = sp;
        alias.set(100L, 0L, 0L);
        sp.set(200L, 7L, 0L);
        return Check.offInt(sp.get(0L, 0L), 100L) + Check.offInt(alias.get(7L, 0L), 200L);
    }

    static long fillMap(HashMap<Long, Long> hm) {
        hm.put(1L, 10L);
        hm.put(2L, 20L);
        hm.put(1L, 11L);
        return 0L;
    }

    static double checkAHashmapWrittenInAFunctionIsSeenByTheCaller() {
        final HashMap<Long, Long> hm = new HashMap<>();
        fillMap(hm);
        return Check.offInt(hm.getOrDefault(1L, 0L), 11L) + Check.offInt(hm.getOrDefault(2L, 0L), 20L) + Check.offInt(hm.getOrDefault(3L, 0L), 0L) + Check.offInt((long) hm.size(), 2L);
    }

    static double checkTwoNamesForOneHashmapShareIt() {
        final HashMap<Long, Long> a = new HashMap<>();
        final HashMap<Long, Long> b = a;
        a.put(5L, 50L);
        b.put(6L, 60L);
        return Check.offInt((long) a.size(), 2L) + Check.offInt(b.getOrDefault(5L, 0L), 50L) + Check.offInt(a.getOrDefault(6L, 0L), 60L);
    }

    static double checkAStringKeyedHashmapIsShared() {
        final HashMap<String, Long> hm = new HashMap<>();
        addWord(hm, "one", 1L);
        addWord(hm, "two", 2L);
        return Check.offInt(hm.getOrDefault("one", 0L), 1L) + Check.offInt(hm.getOrDefault("two", 0L), 2L) + Check.offInt((long) hm.size(), 2L);
    }

    static long addWord(HashMap<String, Long> hm, String word, long value) {
        hm.put(word, value);
        return 0L;
    }

    static double checkAHashmapOfListsKeepsTheLists() {
        final HashMap<Long, ArrayList<Long>> hm = new HashMap<>();
        final ArrayList<Long> l = new ArrayList<>();
        hm.put(7L, l);
        l.add(5L);
        l.add(6L);
        final ArrayList<Long> got = hm.getOrDefault(7L, new ArrayList<>());
        return Check.offInt((long) got.size(), 2L) + Check.offInt(got.get(1), 6L);
    }

    static long fillGrid(Grid<Long> g) {
        for (long r = 0L; r < 2L; r = r + 1L) {
            for (long c = 0L; c < 3L; c = c + 1L) {
                g.set(r * 10L + c, r, c);
            }
        }
        return 0L;
    }

    static double checkAGridFilledInAFunctionIsSeenByTheCaller() {
        final Grid<Long> g = new Grid<>(new int[] {2, 3}, 0L);
        fillGrid(g);
        return Check.offInt(g.get(1L, 2L), 12L) + Check.offInt(g.get(0L, 1L), 1L);
    }

    static double checkTwoNamesForOneGridShareIt() {
        final Grid<Long> a = new Grid<>(new int[] {2, 3}, 0L);
        final Grid<Long> b = a;
        a.set(9L, 1L, 1L);
        return Check.offInt(b.get(1L, 1L), 9L);
    }

    static double checkAGridStartsZeroed() {
        final Grid<Long> t = new Grid<>(new int[] {2, 2, 2}, 0L);
        t.set(5L, 1L, 1L, 1L);
        return Check.offInt(t.get(0L, 1L, 0L), 0L) + Check.offInt(t.get(1L, 1L, 1L), 5L);
    }

    static double checkDarrayCopyIsIndependent() {
        final ArrayList<Long> a = new ArrayList<>();
        a.add(1L);
        a.add(2L);
        final ArrayList<Long> b = new ArrayList<>(a);
        b.add(3L);
        a.add(9L);
        return Check.offInt((long) a.size(), 3L) + Check.offInt((long) b.size(), 3L) + Check.offInt(b.get(2), 3L) + Check.offInt(a.get(2), 9L) + Check.offInt(b.get(0), 1L);
    }

    static double checkGridAndTensorCopyAreIndependent() {
        final Grid<Long> g = new Grid<>(new int[] {2, 2}, 0L);
        g.set(5L, 1L, 1L);
        final Grid<Long> h = g.copy();
        g.set(6L, 1L, 1L);
        final Grid<Long> t = new Grid<>(new int[] {2, 2, 2}, 0L);
        t.set(7L, 1L, 0L, 1L);
        final Grid<Long> u = t.copy();
        t.set(8L, 1L, 0L, 1L);
        return Check.offInt(h.get(1L, 1L), 5L) + Check.offInt(g.get(1L, 1L), 6L) + Check.offInt(u.get(1L, 0L, 1L), 7L);
    }

    static double checkSparseCopyIsIndependent() {
        final Sparse<Long> s = new Sparse<>(0L);
        s.set(7L, 1L, 2L);
        final Sparse<Long> c = s.copy();
        s.set(8L, 3L, 4L);
        c.set(9L, 5L, 6L);
        return Check.offInt((long) s.size(), 2L) + Check.offInt((long) c.size(), 2L) + Check.offInt(c.get(1L, 2L), 7L);
    }

    static double checkHashmapCopyIsIndependent() {
        final HashMap<Long, Long> m = new HashMap<>();
        m.put(1L, 10L);
        final HashMap<Long, Long> c = new HashMap<>(m);
        m.put(2L, 20L);
        c.put(3L, 30L);
        return Check.offInt((long) m.size(), 2L) + Check.offInt((long) c.size(), 2L) + Check.offInt(c.getOrDefault(1L, 0L), 10L) + Check.offInt(c.getOrDefault(3L, 0L), 30L);
    }

    static double checkCopyOfAStringKeyedHashmap() {
        final HashMap<String, Long> m = new HashMap<>();
        m.put("a", 1L);
        final HashMap<String, Long> c = new HashMap<>(m);
        m.put("b", 2L);
        return Check.offInt(c.getOrDefault("a", 0L), 1L) + Check.offInt((long) c.size(), 1L);
    }

    static ArrayList<Long> makeBag(long n) {
        final ArrayList<Long> d = new ArrayList<>();
        d.add(n);
        d.add(n + 1L);
        return d;
    }

    static long sumGrid(Grid<Long> g) {
        return g.get(0L, 0L) + g.get(1L, 1L);
    }

    static long holdsOnto(ArrayList<ArrayList<Long>> store, ArrayList<Long> d) {
        store.add(d);
        return 0L;
    }

    static double checkAReturnedContainerSurvives() {
        final ArrayList<Long> a = makeBag(10L);
        final ArrayList<Long> b = makeBag(20L);
        return Check.offInt(a.get(0) + a.get(1) + b.get(0) + b.get(1), 62L);
    }

    static double checkAContainerLentToACalleeIsStillUsableAfterwards() {
        long total = 0L;
        for (long i = 0L; i < 50L; i = i + 1L) {
            final Grid<Long> g = new Grid<>(new int[] {2, 2}, 0L);
            g.set(i, 0L, 0L);
            g.set(1L, 1L, 1L);
            total = total + sumGrid(g) + g.get(0L, 0L);
        }
        return Check.offInt(total, 2L * 1225L + 50L + 0L);
    }

    static double checkAContainerGivenToACalleeThatKeepsItSurvives() {
        final ArrayList<ArrayList<Long>> store = new ArrayList<>();
        for (long i = 0L; i < 3L; i = i + 1L) {
            final ArrayList<Long> d = new ArrayList<>();
            d.add(i * 10L);
            holdsOnto(store, d);
        }
        return Check.offInt(store.get(0).get(0) + store.get(1).get(0) + store.get(2).get(0), 30L);
    }

    static double checkAContainerReturnedFromInsideAnIfSurvives() {
        final ArrayList<Long> keep = new ArrayList<>();
        keep.add(4L);
        final ArrayList<Long> other = new ArrayList<>();
        other.add(5L);
        long total = 0L;
        for (long i = 0L; i < 20L; i = i + 1L) {
            final ArrayList<Long> scratch = new ArrayList<>();
            scratch.add(i);
            if (i == 7L) {
                total = total + scratch.get(0);
            }
        }
        return Check.offInt(total + keep.get(0) + other.get(0), 16L);
    }

    static long earlyExit(long n) {
        final ArrayList<Long> scratch = new ArrayList<>();
        scratch.add(n);
        if (n > 5L) {
            return scratch.get(0) * 2L;
        }
        scratch.add(1L);
        return (long) scratch.size() + scratch.get(0);
    }

    static double checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue() {
        return Check.offInt(earlyExit(9L) * 100L + earlyExit(2L), 18L * 100L + 4L);
    }

    static String greeting(String name) {
        final String hello = "hello, ";
        final String full = hello + name + "!";
        return full;
    }

    static long shout(String s) {
        if (Objects.equals(s, "hi!")) {
            return 1L;
        }
        return 0L;
    }

    static Fn0<Long> counterFrom(long start) {
        final long[] n = {start};
        final Fn0<Long> next = () -> {
            n[0] = n[0] + 1L;
            return n[0];
        };
        return next;
    }

    static long applyTwice(Fn0<Long> f) {
        f.call();
        return f.call();
    }

    static double checkAReturnedStringSurvives() {
        final String a = greeting("ann");
        final String b = greeting("bo");
        if (!Objects.equals(a, "hello, ann!")) {
            return 1.0;
        }
        if (!Objects.equals(b, "hello, bo!")) {
            return 1.0;
        }
        return 0.0;
    }

    static double checkAStringLentToACalleeIsUsableAfterwards() {
        long hits = 0L;
        for (long i = 0L; i < 20L; i = i + 1L) {
            final String s = "h" + "i" + "!";
            hits = hits + shout(s);
            if (Objects.equals(s, "hi!")) {
                hits = hits + 1L;
            }
        }
        return Check.offInt(hits, 40L);
    }

    static double checkAStringUsedAsAHashmapKeyIsKept() {
        final HashMap<String, Long> m = new HashMap<>();
        for (long i = 0L; i < 3L; i = i + 1L) {
            final String key = "k" + "ey";
            m.put(key, i);
        }
        return Check.offInt(m.getOrDefault("key", 0L), 2L) + Check.offInt((long) m.size(), 1L);
    }

    static double checkAClosureThatIsReturnedKeepsItsBox() {
        final Fn0<Long> c = counterFrom(10L);
        c.call();
        return Check.offInt(c.call(), 12L);
    }

    static double checkAClosureGivenToACalleeKeepsWorking() {
        final long[] n = {0L};
        final Fn0<Long> tick = () -> {
            n[0] = n[0] + 5L;
            return n[0];
        };
        return Check.offInt(applyTwice(tick), 10L) + Check.offInt(n[0], 10L);
    }

    static double checkAClosureOnlyCalledLocallyInALoop() {
        long total = 0L;
        for (long i = 0L; i < 50L; i = i + 1L) {
            final long[] count = {i};
            final Fn0<Long> bump = () -> {
                count[0] = count[0] + 1L;
                return count[0];
            };
            bump.call();
            total = total + bump.call() - i;
        }
        return Check.offInt(total, 100L);
    }

    static double checkTwoClosuresShareOneCapturedName() {
        final long[] n = {0L};
        final Fn0<Long> inc = () -> {
            n[0] = n[0] + 1L;
            return n[0];
        };
        final Fn0<Long> read = () -> n[0] * 10L;
        inc.call();
        inc.call();
        return Check.offInt(read.call(), 20L);
    }

    static final class Pocket {
        ArrayList<Long> items;

        Pocket(ArrayList<Long> items) {
            this.items = items;
        }

        Pocket copy() {
            return new Pocket(items);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Pocket that && Arrays.equals(new Object[] {items}, new Object[] {that.items});
        }

        @Override
        public int hashCode() {
            return Objects.hash(items);
        }
    }

    static ArrayList<Long> madeHere() {
        final ArrayList<Long> d = new ArrayList<>();
        d.add(1L);
        return d;
    }

    static ArrayList<Long> madeThroughAnother() {
        final ArrayList<Long> d = madeHere();
        d.add(2L);
        return d;
    }

    static ArrayList<Long> madeInPlace() {
        return new ArrayList<>();
    }

    static ArrayList<Long> sameAsArgument(ArrayList<Long> d) {
        return d;
    }

    static ArrayList<Long> elementOf(ArrayList<ArrayList<Long>> xs) {
        return xs.get(0);
    }

    static ArrayList<Long> fieldOf(Pocket p) {
        return p.items;
    }

    static ArrayList<Long> keptAndReturned(ArrayList<ArrayList<Long>> store) {
        final ArrayList<Long> d = new ArrayList<>();
        store.add(d);
        return d;
    }

    static ArrayList<Long> sometimesTheArgument(long flag, ArrayList<Long> d) {
        if (flag == 0L) {
            return d;
        }
        return new ArrayList<>();
    }

    static double checkAFreshResultIsUsableAndReleased() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final ArrayList<Long> a = madeHere();
            final ArrayList<Long> b = madeThroughAnother();
            final ArrayList<Long> c = madeInPlace();
            c.add(i);
            total = total + (long) a.size() + (long) b.size() + (long) c.size();
        }
        return Check.offInt(total, 100L * 4L);
    }

    static double checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner() {
        final ArrayList<Long> src = new ArrayList<>();
        src.add(7L);
        final ArrayList<Long> same = sameAsArgument(src);
        same.add(8L);
        final ArrayList<ArrayList<Long>> outer = new ArrayList<>();
        outer.add(src);
        final ArrayList<Long> inner = elementOf(outer);
        final Pocket pocket = new Pocket(src);
        final ArrayList<Long> field = fieldOf(pocket);
        final ArrayList<Long> maybe = sometimesTheArgument(0L, src);
        return Check.offInt((long) src.size(), 2L) + Check.offInt((long) inner.size(), 2L) + Check.offInt((long) field.size(), 2L) + Check.offInt((long) maybe.size(), 2L);
    }

    static double checkAnObjectKeptByTheCalleeSurvivesItsCaller() {
        final ArrayList<ArrayList<Long>> store = new ArrayList<>();
        long total = 0L;
        for (long i = 0L; i < 5L; i = i + 1L) {
            final ArrayList<Long> d = keptAndReturned(store);
            d.add(i);
        }
        for (long j = 0L; j < 5L; j = j + 1L) {
            total = total + store.get((int) j).get(0);
        }
        return Check.offInt(total, 10L);
    }

    static ArrayList<Long> handedOn() {
        final ArrayList<Long> a = madeHere();
        return a;
    }

    static double checkAResultHandedOnAgainIsStillFresh() {
        final ArrayList<Long> a = handedOn();
        return Check.offInt(a.get(0), 1L);
    }

    static double checkADiscardedFreshResultIsHarmless() {
        madeHere();
        madeThroughAnother();
        return 0.0;
    }

    static Pocket madeStruct(long n) {
        final Pocket p = new Pocket(new ArrayList<>());
        p.items.add(n);
        return p;
    }

    static String makeLabel(String a) {
        final String s = a + "-" + a;
        return s;
    }

    static double checkAStructAndAStringHandedOverByACall() {
        long total = 0L;
        for (long i = 0L; i < 50L; i = i + 1L) {
            final Pocket p = madeStruct(i);
            final String s = makeLabel("x");
            if (Objects.equals(s, "x-x")) {
                total = total + p.items.get(0) + 1L;
            }
        }
        return Check.offInt(total, 1225L + 50L);
    }

    static final class Shelf {
        ArrayList<Long> books;
        String tags;

        Shelf(ArrayList<Long> books, String tags) {
            this.books = books;
            this.tags = tags;
        }

        Shelf copy() {
            return new Shelf(books, tags);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Shelf that && Arrays.equals(new Object[] {books, tags}, new Object[] {that.books, that.tags});
        }

        @Override
        public int hashCode() {
            return Objects.hash(books, tags);
        }
    }

    static final class Cabinet {
        ArrayList<Long> drawer;

        Cabinet(ArrayList<Long> drawer) {
            this.drawer = drawer;
        }

        Cabinet copy() {
            return new Cabinet(drawer);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Cabinet that && Arrays.equals(new Object[] {drawer}, new Object[] {that.drawer});
        }

        @Override
        public int hashCode() {
            return Objects.hash(drawer);
        }
    }

    static final class Crate {
        ArrayList<Long> items;

        Crate(ArrayList<Long> items) {
            this.items = items;
        }

        Crate copy() {
            return new Crate(items);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Crate that && Arrays.equals(new Object[] {items}, new Object[] {that.items});
        }

        @Override
        public int hashCode() {
            return Objects.hash(items);
        }
    }

    static Shelf stockedShelf(long n) {
        final Shelf s = new Shelf(new ArrayList<>(), "a" + "b");
        s.books.add(n);
        s.books.add(n + 1L);
        return s;
    }

    static ArrayList<Long> openDrawer(Cabinet c) {
        return c.drawer;
    }

    static double checkAnOwnedFieldIsUsableThenReleasedWithItsStruct() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final Shelf s = stockedShelf(i);
            total = total + (long) s.books.size() + s.books.get(0) - i;
            if (Objects.equals(s.tags, "ab")) {
                total = total + 1L;
            }
        }
        return Check.offInt(total, 100L * 3L);
    }

    static double checkAReplacedFieldValueIsReleasedAndTheNewOneWorks() {
        final Shelf s = stockedShelf(5L);
        s.books = new ArrayList<>();
        s.books.add(40L);
        s.tags = "x" + "y";
        if (!Objects.equals(s.tags, "xy")) {
            return 1.0;
        }
        return Check.offInt((long) s.books.size(), 1L) + Check.offInt(s.books.get(0), 40L);
    }

    static double checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct() {
        final Cabinet c = new Cabinet(new ArrayList<>());
        c.drawer.add(9L);
        final ArrayList<Long> drawer = openDrawer(c);
        drawer.add(10L);
        return Check.offInt((long) c.drawer.size(), 2L) + Check.offInt(drawer.get(1), 10L);
    }

    static double checkACopiedStructSharesNothingItMustNotFree() {
        final Crate a = new Crate(new ArrayList<>());
        a.items.add(3L);
        final Crate b = a.copy();
        b.items.add(4L);
        return Check.offInt((long) a.items.size(), 2L) + Check.offInt((long) b.items.size(), 2L);
    }

    static double checkAListOfListsReleasesItsElements() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final ArrayList<ArrayList<Long>> outer = new ArrayList<>();
            for (long j = 0L; j < 3L; j = j + 1L) {
                outer.add(new ArrayList<>());
                outer.get((int) j).add(i + j);
            }
            total = total + (long) outer.size() + (long) outer.get(2).size() + outer.get(1).get(0) - i;
        }
        return Check.offInt(total, 100L * (3L + 1L + 1L));
    }

    static ArrayList<Long> firstOf(ArrayList<ArrayList<Long>> xs) {
        final ArrayList<Long> first = xs.get(0);
        return first;
    }

    static double checkAnElementReadOutOfAListIsNotReleasedWithIt() {
        final ArrayList<ArrayList<Long>> store = new ArrayList<>();
        store.add(new ArrayList<>());
        store.get(0).add(6L);
        final ArrayList<Long> got = firstOf(store);
        got.add(7L);
        return Check.offInt((long) store.get(0).size(), 2L) + Check.offInt(got.get(1), 7L);
    }

    static double checkAMapPastItsStartingCapacityKeepsEveryEntry() {
        final HashMap<Long, Long> m = new HashMap<>();
        for (long i = 0L; i < 1000L; i = i + 1L) {
            m.put(i, i * 3L);
        }
        long wrong = 0L;
        for (long i = 0L; i < 1000L; i = i + 1L) {
            if (m.getOrDefault(i, 0L) != i * 3L) {
                wrong = wrong + 1L;
            }
        }
        return Check.offInt(wrong, 0L) + Check.offInt((long) m.size(), 1000L);
    }

    static double checkAGrownMapIsStillTheSameMapForItsAliases() {
        final HashMap<Long, Long> m = new HashMap<>();
        final HashMap<Long, Long> alias = m;
        for (long i = 0L; i < 20L; i = i + 1L) {
            alias.put(i, i);
        }
        return Check.offInt((long) m.size(), 20L) + Check.offInt(m.getOrDefault(19L, 0L), 19L);
    }

    static double checkStringKeysSurviveGrowing() {
        final HashMap<String, Long> m = new HashMap<>();
        m.put("a", 1L);
        m.put("b", 2L);
        m.put("c", 3L);
        m.put("d", 4L);
        m.put("a", 10L);
        return Check.offInt(m.getOrDefault("a", 0L), 10L) + Check.offInt(m.getOrDefault("b", 0L), 2L) + Check.offInt(m.getOrDefault("c", 0L), 3L) + Check.offInt(m.getOrDefault("d", 0L), 4L) + Check.offInt((long) m.size(), 4L);
    }

    record GrowKey(long a, long b) {
    }

    static double checkStructKeysSurviveGrowing() {
        final HashMap<GrowKey, Long> m = new HashMap<>();
        for (long i = 0L; i < 50L; i = i + 1L) {
            m.put(new GrowKey(i, i + 1L), i);
        }
        long wrong = 0L;
        for (long i = 0L; i < 50L; i = i + 1L) {
            if (m.getOrDefault(new GrowKey(i, i + 1L), 0L) != i) {
                wrong = wrong + 1L;
            }
        }
        return Check.offInt(wrong, 0L) + Check.offInt((long) m.size(), 50L);
    }

    static double checkASetPastItsStartingCapacityKeepsEveryMember() {
        final HashSet<Long> seen = new HashSet<>();
        for (long i = 0L; i < 100L; i = i + 1L) {
            seen.add(i % 40L);
        }
        long absent = 0L;
        if (seen.contains(39L) == false) {
            absent = absent + 1L;
        }
        if (seen.contains(40L)) {
            absent = absent + 1L;
        }
        return Check.offInt((long) seen.size(), 40L) + Check.offInt(absent, 0L);
    }

    static long readAt(ArrayList<Long> xs, long i) {
        return xs.get((int) i);
    }

    static long caughtCode(ArrayList<Long> xs, long i) {
        long code = 0L;
        try {
            code = readAt(xs, i);
        } catch (Thrown thrown) {
            final long c = thrown.value;
            code = c;
        }
        return code;
    }

    static double checkAnIndexPastTheEndOfAListThrowsMinusOne() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(5L);
        return Check.offInt(caughtCode(xs, 1L), 0L - 1L) + Check.offInt(caughtCode(xs, 100000L), 0L - 1L);
    }

    static double checkANegativeIndexThrowsMinusOne() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(5L);
        return Check.offInt(caughtCode(xs, 0L - 1L), 0L - 1L);
    }

    static double checkAnIndexInsideTheListStillReads() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(5L);
        xs.add(6L);
        return Check.offInt(caughtCode(xs, 0L), 5L) + Check.offInt(caughtCode(xs, 1L), 6L);
    }

    static double checkAnEmptyListCannotBeRead() {
        final ArrayList<Long> xs = new ArrayList<>();
        return Check.offInt(caughtCode(xs, 0L), 0L - 1L);
    }

    static long divideBy(long a, long b) {
        long out = 0L;
        try {
            out = a / b;
        } catch (Thrown thrown) {
            final long c = thrown.value;
            out = c;
        }
        return out;
    }

    static double checkDividingByZeroThrowsMinusFour() {
        return Check.offInt(divideBy(10L, 0L), 0L - 4L) + Check.offInt(divideBy(10L, 2L), 5L);
    }

    static double checkAFixedArrayAndAGridAreCheckedPerDimension() {
        long code = 0L;
        final Grid<Long> g = new Grid<>(new int[] {2, 2}, 0L);
        try {
            g.set(1L, 0L, 5L);
        } catch (Thrown thrown) {
            final long c = thrown.value;
            code = c;
        }
        return Check.offInt(code, 0L - 1L);
    }

    record OptPoint(long x, long y) {
    }

    static Long optionalFind(ArrayList<Long> xs, long want) {
        for (long i = 0L; i < (long) xs.size(); i = i + 1L) {
            if (xs.get((int) i) == want) {
                return i;
            }
        }
        return null;
    }

    static OptPoint optionalPoint(long ok) {
        if (ok == 1L) {
            return new OptPoint(7L, 8L);
        }
        return null;
    }

    static String optionalName(long ok) {
        if (ok == 1L) {
            return "ann";
        }
        return null;
    }

    static long unwrapCode(Long o) {
        long code = 0L;
        try {
            code = present(o);
        } catch (Thrown thrown) {
            final long c = thrown.value;
            code = c;
        }
        return code;
    }

    static double checkAnOptionalHoldsAValueOrNone() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(10L);
        xs.add(20L);
        long seen = 0L;
        if (optionalFind(xs, 20L) != null) {
            seen = seen + 1L;
        }
        if (optionalFind(xs, 99L) == null) {
            seen = seen + 10L;
        }
        return Check.offInt(seen, 11L) + Check.offInt(present(optionalFind(xs, 20L)), 1L);
    }

    static double checkAnOptionalOfAStructAndOfAStringWork() {
        final OptPoint p = optionalPoint(1L);
        long seen = 0L;
        if (p != null) {
            seen = seen + present(p).x + present(p).y;
        }
        if (optionalPoint(0L) == null) {
            seen = seen + 100L;
        }
        if (optionalName(1L) != null) {
            seen = seen + 1000L;
        }
        return Check.offInt(seen, 1115L);
    }

    static double checkUnwrappingNoneThrowsMinusThree() {
        return Check.offInt(unwrapCode(null), 0L - 3L) + Check.offInt(unwrapCode(42L), 42L);
    }

    // Two lists joined into a new one.
    static <T> ArrayList<T> concat(ArrayList<T> a, ArrayList<T> b) {
        final ArrayList<T> out = new ArrayList<>(a);
        out.addAll(b);
        return out;
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

    // A push used as a value gives the list, as in polyir (Java's add gives whether it changed the list).
    static <T> ArrayList<T> pushed(ArrayList<T> items, T item) {
        items.add(item);
        return items;
    }

    // An `either`: an ok value or an error, and which one it holds.
    static final class Either<T, E> {
        final boolean isOk;
        private final Object held;

        private Either(boolean isOk, Object held) {
            this.isOk = isOk;
            this.held = held;
        }

        static <T, E> Either<T, E> ok(T value) {
            return new Either<>(true, value);
        }

        static <T, E> Either<T, E> err(E value) {
            return new Either<>(false, value);
        }

        @SuppressWarnings("unchecked")
        T okValue() {
            return (T) held;
        }

        @SuppressWarnings("unchecked")
        E errValue() {
            return (E) held;
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Either<?, ?> that && isOk == that.isOk && Objects.equals(held, that.held);
        }

        @Override
        public int hashCode() {
            return Objects.hash(isOk, held);
        }
    }

    // A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
    // written reads as the element's zero. `set(value, row, col)` writes one.
    static final class Sparse<T> {
        private final ArrayList<Long> rows = new ArrayList<>();
        private final ArrayList<Long> cols = new ArrayList<>();
        private final ArrayList<T> values = new ArrayList<>();
        private final T zero;

        Sparse(T zero) {
            this.zero = zero;
        }

        long size() {
            return values.size();
        }

        T get(long row, long col) {
            T found = zero;
            for (int i = 0; i < values.size(); i++) {
                if (rows.get(i) == row && cols.get(i) == col) {
                    found = values.get(i);
                }
            }
            return found;
        }

        void set(T value, long row, long col) {
            rows.add(row);
            cols.add(col);
            values.add(value);
        }

        Sparse<T> copy() {
            final Sparse<T> out = new Sparse<>(zero);
            out.rows.addAll(rows);
            out.cols.addAll(cols);
            out.values.addAll(values);
            return out;
        }
    }

    // A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
    // writes one.
    static final class Grid<T> {
        final int[] dims;
        private final Object[] cells;

        Grid(int[] dims, T zero) {
            this.dims = dims;
            int count = 1;
            for (final int size : dims) {
                count *= size;
            }
            cells = new Object[count];
            Arrays.fill(cells, zero);
        }

        private int place(long[] indices) {
            int at = 0;
            for (int i = 0; i < dims.length; i++) {
                at = at * dims[i] + (int) indices[i];
            }
            return at;
        }

        @SuppressWarnings("unchecked")
        T get(long... indices) {
            return (T) cells[place(indices)];
        }

        void set(T value, long... indices) {
            cells[place(indices)] = value;
        }

        Grid<T> copy() {
            final Grid<T> out = new Grid<>(dims, null);
            System.arraycopy(cells, 0, out.cells, 0, cells.length);
            return out;
        }
    }

    // What `throw` throws: the code thrown.
    static final class Thrown extends RuntimeException {
        final long value;

        Thrown(long value) {
            super("thrown " + value);
            this.value = value;
        }
    }

    // Stops the program with a message, as polyir's `fail` does.
    static <T> T fail(String message) {
        System.err.println("trap: " + message);
        System.exit(1);
        return null;
    }

    // The value an optional holds; an empty one stops the program.
    static <T> T present(T value) {
        if (value == null) {
            fail("unwrap of an empty optional");
        }
        return value;
    }
}
