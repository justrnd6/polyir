// The garbage collector of the bytecode VM (docs/memory.md, Parts 1 and 3). Read it top to bottom: this file is the whole collector.
//
// What it is for. A running program keeps making objects: structs, lists, maps, sets, closures, coroutines, and the boxes behind its
// `var`s. They live in the heap (VmValues.hpp), a vector of cells: an object's id is its index there, and a reference to the object is
// that id. Nothing in the program says when an object is no longer needed, so from time to time the collector works it out, and a
// program that keeps a bounded amount of data then uses a bounded amount of memory, however long it runs.
//
// How: MARK AND SWEEP.
//   1. Mark. Start from the ROOTS: the places the program can read a reference from without going through another object. They are
//      the slots and operand stacks of the frames that are running, the globals, and the few values the VM's own C++ code holds while
//      program code runs ("pinned" values, such as a coroutine being resumed). Mark every object a root refers to, then every object a
//      marked object refers to, and so on until nothing new turns up. A marked object is REACHABLE: the program may still use it.
//   2. Sweep. A cell that was not marked holds an object the program can never reach again, since no path leads to it any more. The
//      cell is emptied and its id goes on the heap's free list, where the next allocation finds it.
//
// Why this kind of collector. It is a TRACING collector: it finds the live objects instead of counting the references to each one, so
// an ordinary assignment costs nothing extra, and a cycle (two objects that refer to each other and that nothing else reaches) is
// reclaimed like any other garbage. It is PRECISE: the VM knows which of its values are references (ValueKind::Ref), so it never takes a
// number for one. And it never moves an object: a reference is a cell's index, so moving an object would mean finding and rewriting
// every value that refers to it.
//
// IN SLICES (Part 3), so that no pause is long. One collection (a CYCLE) is spread over many short SLICES, and the program runs between
// them; each slice does at most a budget of work (Memory::slice: values looked at, or cells swept), so the longest pause is set by the
// budget, not by how much data the program keeps. In the words of TRI-COLOUR marking, an object is WHITE (not reached yet; at the end of
// the marking, the white objects are the garbage), GREY (reached, its own values not looked at yet: it is on the worklist) or BLACK
// (reached, every value looked at). A slice takes grey objects, looks at their values (what they refer to turns grey) and so makes them
// black; the marking is over when nothing is grey.
//
// The danger is the program, which runs between slices. If it takes the only reference to a white object out of an object still grey
// (overwriting it) and keeps it only where the marking has already looked (a black object, or a frame's slots, which the marking looked at
// when the cycle began), the object stays white and is freed while still in use. The WRITE BARRIER (VmValues.hpp) prevents it: before a
// store overwrites a value inside an object (a field, an element, a map's value, a box's cell), the value it overwrites turns grey; before
// values are moved or removed all at once (a map's entry removed, a list truncated, a coroutine resumed), all of them do. This is the
// SNAPSHOT-AT-THE-BEGINNING (Yuasa) barrier: every object reachable when the cycle began is marked, and objects made during the cycle are
// made black, so nothing reachable is lost. (Why this one and not Dijkstra's, which greys the value being stored: with Dijkstra's the
// frames and globals, which have no barrier, must be looked at again at the end, and whatever that finds marked before the cycle can end,
// a pause that grows with the data. With the snapshot the roots are looked at once, when the cycle begins, a pause as long as the stack is
// deep; and the cycle ends the moment nothing is grey, inside a slice. The price: what was reachable at the start survives the cycle even
// if the program drops it meanwhile; it goes at the next one.)
//
// The SWEEP is in slices too: a cursor walks the cells, `slice` at a time. An object made while the sweep has not reached its cell yet is
// made marked (the sweep keeps it); one made behind the cursor is made unmarked, ready for the next cycle.
//
// When it runs: only at a SAFE POINT, the moment between two instructions of the dispatch loop (Run.cpp, safePoint). Only then is every
// live reference in a root; halfway through an instruction, one may sit in a C++ local the collector cannot see. So allocation never
// collects. It only counts (Alloc, VmValues.cpp), and a safe point does the collector's work once the count passes `collectAt`. Between
// cycles that is the threshold: twice what survived the last cycle, so a program that keeps a steady amount of data collects at a steady
// rate. During a cycle, every object the program makes earns PACE units of work, and the next slice is due once they add up to a budget;
// should the program outrun the collector (the heap half as big again as when the cycle began), a slice is due at every safe point.
// Memory::incremental false (POLYIR_GC_MODE=stop) does each whole cycle at one safe point instead, as before Part 3.
#include "polyir/Std.hpp"
#include "vm/Machine.hpp"

namespace polyir::vm {

  using Nanoseconds = std::chrono::nanoseconds;
  using std::chrono::duration_cast;

  namespace {

    // The two steps of the work at a safe point, doTheWork and then countThePause, defined below in the order CollectGarbage takes them.
    Outcome doTheWork(Machine& m);

    void countThePause(Pauses& pauses, int64_t nanoseconds);

  }  // namespace

  // ── the collector's work at a safe point ──

  // However much it does, the time it takes is one pause of the program.
  Outcome CollectGarbage(Machine& m) {
    const SteadyClock::time_point started = SteadyClock::now();
    Outcome out = doTheWork(m);
    countThePause(m.memory.pauses, duration_cast<Nanoseconds>(SteadyClock::now() - started).count());

    return out;
  }

  namespace {

    void beginACycle(Machine& m);

    bool mustFinish(const Machine& m);

    Outcome finishTheCycle(Machine& m);

    Outcome oneSlice(Machine& m, int64_t budget);

    void scheduleTheNextWork(Machine& m);

    // A new cycle begins with its roots. Then comes one slice of marking or of sweeping, or the whole cycle at once when the
    // collector stops the world (or must). Last, it sets when the next work is due.
    Outcome doTheWork(Machine& m) {
      m.memory.peakObjects = max(m.memory.peakObjects, m.heap.objects);  // the heap is at its fullest when collecting is due

      if (m.heap.marking.phase == Phase::Idle) {
        beginACycle(m);
      }

      Outcome out = mustFinish(m) ? finishTheCycle(m) : oneSlice(m, m.memory.slice);
      scheduleTheNextWork(m);

      return out;
    }

    // Stop-the-world mode and its stress mode finish every cycle at once; so does an incremental cycle that let the heap pass the ceiling.
    bool mustFinish(const Machine& m) {
      return !m.memory.incremental || m.memory.collectEverySafePoint || m.heap.objects > m.memory.ceiling;
    }

    // ── a cycle ──

    void markTheRoots(Machine& m);

    // The snapshot: from here on, everything reachable now will be marked. The roots are looked at now, in this pause, and never again in
    // this cycle. The barrier keeps what was in the heap, and what the program makes is black.
    void beginACycle(Machine& m) {
      m.memory.collections++;
      m.memory.cycleStart = m.heap.objects;
      m.heap.marking.phase = Phase::Marking;
      m.heap.marking.markedObjects = 0;
      markTheRoots(m);
    }

    // Marks to the end, then sweeps to the end (or stops at the trap that ends the run).
    Outcome finishTheCycle(Machine& m) {
      Outcome out = VoidOutcome();

      while (IsValue(out) && m.heap.marking.phase != Phase::Idle) {
        out = oneSlice(m, INT64_MAX);
      }

      return out;
    }

    Outcome markASlice(Machine& m, int64_t budget);

    void sweepASlice(Machine& m, int64_t budget);

    Outcome oneSlice(Machine& m, int64_t budget) {
      if (m.heap.marking.phase == Phase::Marking) {
        return markASlice(m, budget);
      }

      sweepASlice(m, budget);

      return VoidOutcome();
    }

    int64_t traceSome(Machine& m, int64_t budget);

    Outcome finishMarking(Machine& m);

    // Grey objects are looked at until the budget is spent; once none is left, the marking is over.
    Outcome markASlice(Machine& m, int64_t budget) {
      int64_t done = 0;

      while (done < budget && !m.heap.marking.worklist.empty()) {
        done += traceSome(m, budget - done);
      }

      if (!m.heap.marking.worklist.empty()) {
        return VoidOutcome();
      }

      return finishMarking(m);
    }

    Outcome verifyTheMarks(Machine& m);

    void startTheSweep(Heap& heap);

    void setTheThreshold(Memory& memory, int64_t survivors);

    Outcome checkTheCeiling(const Memory& memory, int64_t survivors);

    // What is marked now survives (what the program made during the cycle among it); the next threshold and the ceiling follow from it.
    Outcome finishMarking(Machine& m) {
      const Outcome verified = m.memory.verify ? verifyTheMarks(m) : VoidOutcome();

      if (!IsValue(verified)) {
        return verified;
      }

      const int64_t survivors = m.heap.marking.markedObjects;
      startTheSweep(m.heap);
      setTheThreshold(m.memory, survivors);

      return checkTheCeiling(m.memory, survivors);
    }

    // ── 1. mark: the roots ──

    void markFrame(Heap& heap, const Frame& frame);

    void markValues(Heap& heap, const Vector<Value>& values);

    // Every running frame, the globals and the pinned values: everything the program (or the VM on its behalf) can reach directly.
    void markTheRoots(Machine& m) {
      for (const Frame* frame : m.runningFrames) {
        markFrame(m.heap, *frame);
      }

      markValues(m.heap, m.globals);
      markValues(m.heap, m.pinned);
    }

    // A frame's locals are in its slots; its temporaries (an expression half evaluated, arguments not yet passed) are on its stack.
    void markFrame(Heap& heap, const Frame& frame) {
      markValues(heap, frame.slots);
      markValues(heap, frame.stack);
    }

    void markValue(Heap& heap, const Value& v);

    void markValues(Heap& heap, const Vector<Value>& values) {
      for (const Value& v : values) {
        markValue(heap, v);
      }
    }

    // An optional, a variant and an either carry their payload in `inner`, outside the heap.
    bool isWrapper(const Value& v) {
      return v.kind == ValueKind::Some || v.kind == ValueKind::Variant || v.kind == ValueKind::Either;
    }

    // The object a value refers to, or -1. A payload may itself be wrapped (`some(some(list))`), so the loop unwraps until it reaches
    // something that is not a wrapper. Scalars and strings hold no reference (a string's text is C++'s to free).
    int64_t referenceIn(const Value& v) {
      const Value* at = &v;

      while (isWrapper(*at) && at->inner != nullptr) {
        at = at->inner.get();
      }

      return at->kind == ValueKind::Ref ? static_cast<int64_t>(at->number) : -1;
    }

    void markObject(Heap& heap, int64_t id);

    void markValue(Heap& heap, const Value& v) {
      const int64_t id = referenceIn(v);

      if (id >= 0) {
        markObject(heap, id);
      }
    }

    // Marking an object only puts it on the worklist (it turns grey); its own values are looked at by the trace, so nothing here recurses.
    void markObject(Heap& heap, int64_t id) {
      Marking& marking = heap.marking;
      const auto cell = static_cast<size_t>(id);

      if (marking.marked[cell] != 0) {
        return;
      }

      marking.marked[cell] = 1;
      marking.markedObjects++;
      marking.worklist.push_back(Grey{id, 0});
    }

    // ── 1. mark: looking at the grey objects ──

    // Where each kind of object keeps its values, in the order the trace looks at them. A box keeps its cell, a closure its
    // captures, and a suspended coroutine its frame's slots, then its stack. A struct, list, set, map or sparse keeps its `items`
    // (fields, elements, members, keys), then its `values` (a map's and a sparse's values).
    size_t valueCount(const HeapObject& object) {
      switch (object.kind) {
        case ObjectKind::Box:
          return 1;
        case ObjectKind::Closure:
          return object.closure.captured.size();
        case ObjectKind::Coroutine:
          return object.coroutine == nullptr ? 0 : object.coroutine->frame.slots.size() + object.coroutine->frame.stack.size();
        case ObjectKind::Free:
          return 0;  // nothing points at a free cell; it is here only for completeness
        default:
          return object.items.size() + object.values.size();
      }
    }

    // The k-th value when `second` is read straight after `first`, as one list.
    const Value& joinedAt(const Vector<Value>& first, const Vector<Value>& second, size_t k) {
      return k < first.size() ? first[k] : second[k - first.size()];
    }

    const Value& valueAt(const HeapObject& object, size_t k) {
      switch (object.kind) {
        case ObjectKind::Box:
          return object.cell;
        case ObjectKind::Closure:
          return object.closure.captured[k];
        case ObjectKind::Coroutine:
          return joinedAt(object.coroutine->frame.slots, object.coroutine->frame.stack, k);
        default:
          return joinedAt(object.items, object.values, k);
      }
    }

    Grey takeGrey(Marking& marking) {
      const Grey grey = marking.worklist.back();
      marking.worklist.pop_back();

      return grey;
    }

    // The most values of one object looked at before the grey objects they pushed are traced, even in a slice with no budget (stopping the
    // world): it keeps the worklist short.
    constexpr size_t CHUNK = 4096;

    // How many of an object's remaining values one step looks at: what is left, but no more than the budget or CHUNK.
    size_t valuesThisTime(int64_t budget, size_t remaining) {
      return min(min(static_cast<size_t>(budget), CHUNK), remaining);
    }

    // Looks at the next values of the grey object on top of the worklist, at most the budget and at most CHUNK of them. Once all
    // are done it is black. A list of a million elements is looked at over many slices, so it cannot make one long pause.
    int64_t traceSome(Machine& m, int64_t budget) {
      const Grey grey = takeGrey(m.heap.marking);
      const HeapObject& object = ObjectAt(m.heap, grey.id);
      const size_t from = min(grey.next, valueCount(object));
      const size_t to = from + valuesThisTime(budget, valueCount(object) - from);

      // An object with values left goes back on the worklist before the chunk is looked at. So the grey objects the chunk pushes
      // lie above it and are traced first: the worklist stays short.
      if (to < valueCount(object)) {
        m.heap.marking.worklist.push_back(Grey{grey.id, to});
      }

      for (size_t k = from; k < to; k++) {
        markValue(m.heap, valueAt(object, k));
      }

      // The work done: the values looked at, plus one for taking the object off the worklist. So an object with no values still
      // uses up budget, and a slice of such objects still ends.
      return static_cast<int64_t>(to - from) + 1;
    }

  }  // namespace

  // ── the write barrier (VmValues.hpp calls these while the collector marks) ──

  // The value a store is about to overwrite turns grey: what was in the heap when the cycle began cannot disappear before it is marked.
  void ShadeValue(Heap& heap, const Value& v) {
    markValue(heap, v);
  }

  // Every value of an object turns grey, before they are moved or removed all at once.
  void ShadeObject(Heap& heap, const HeapObject& object) {
    for (size_t k = 0; k < valueCount(object); k++) {
      markValue(heap, valueAt(object, k));
    }
  }

  namespace {

    // ── 2. sweep ──

    // The sweep covers the cells there are now; cells added later hold objects made after the marking, which the next cycle decides on.
    void startTheSweep(Heap& heap) {
      heap.marking.phase = Phase::Sweeping;
      heap.marking.sweepAt = 0;
      heap.marking.sweepEnd = heap.cells.size();
    }

    void sweepCell(Machine& m, size_t id);

    // The next `budget` cells; once the last is swept, the cycle is over.
    void sweepASlice(Machine& m, int64_t budget) {
      Marking& marking = m.heap.marking;
      const size_t end = marking.sweepAt + static_cast<size_t>(min(budget, static_cast<int64_t>(marking.sweepEnd - marking.sweepAt)));

      for (size_t id = marking.sweepAt; id < end; id++) {
        sweepCell(m, id);
      }

      marking.sweepAt = end;

      if (end == marking.sweepEnd) {
        marking.phase = Phase::Idle;
      }
    }

    void reclaimCell(Machine& m, size_t id);

    // A marked cell survives, and is unmarked for the next cycle; an unmarked one that holds an object is reclaimed.
    void sweepCell(Machine& m, size_t id) {
      uint8_t& mark = m.heap.marking.marked[id];

      if (mark != 0) {
        mark = 0;
      } else if (m.heap.cells[id].kind != ObjectKind::Free) {
        reclaimCell(m, id);
      }
    }

    // The cell is emptied, which hands its lists, text and coroutine back to C++ at once, and its id goes on the free list. The cell
    // itself stays where it is: the ids of the objects after it are references and must not change. Reusing the id later is safe,
    // because the old object was unreachable: nothing still holds its id.
    void reclaimCell(Machine& m, size_t id) {
      HeapObject& cell = m.heap.cells[id];
      cell = HeapObject{};
      cell.kind = ObjectKind::Free;

      m.heap.freeCells.push_back(static_cast<int64_t>(id));
      m.heap.objects--;
      m.memory.reclaimed++;
    }

    // ── 3. the policy: when the next work comes, and when the run must stop ──

    // A heap of fewer objects than this is never worth collecting: 65,536 cells are some 15 MB, and collecting smaller heaps would spend
    // the run sweeping instead of running.
    constexpr int64_t MINIMUM_THRESHOLD = int64_t{1} << 16;

    // The work each object the program makes during a cycle earns: values looked at, or cells swept.
    constexpr int64_t PACE = 32;

    // Twice what survived (and at least the minimum), so the time spent collecting stays in proportion to the work the program does.
    // Never past the ceiling: a heap about to cross it is collected, and checked, first. The stress modes keep the threshold at 0, so
    // every safe point finds the heap past it.
    void setTheThreshold(Memory& memory, int64_t survivors) {
      if (memory.collectEverySafePoint || memory.verify) {
        memory.threshold = 0;
      } else {
        memory.threshold = min(memory.ceiling, max(MINIMUM_THRESHOLD, 2 * survivors));
      }
    }

    // More objects still reachable than the ceiling allows: the program really does hold that much, so collecting again cannot help.
    // The run stops with a trap instead of growing until the operating system ends the process the VM runs in.
    Outcome checkTheCeiling(const Memory& memory, int64_t survivors) {
      if (survivors <= memory.ceiling) {
        return VoidOutcome();
      }

      const String limit = " (limit " + to_string(memory.ceiling) + ")";

      return Trap("out of memory: " + to_string(survivors) + " objects are still reachable" + limit);
    }

    // Whether the program makes objects faster than the cycle goes: the heap half as big again as when the cycle began.
    bool isBehind(const Machine& m) {
      return m.heap.objects > m.memory.cycleStart + m.memory.cycleStart / 2;
    }

    // Between cycles the next work is the next cycle, at the threshold. During one, it is the next slice, once the program has made
    // the objects that earn it. The slice comes at the next safe point instead when the program is outrunning the collector, or in
    // the incremental stress mode.
    void scheduleTheNextWork(Machine& m) {
      Memory& memory = m.memory;

      if (m.heap.marking.phase == Phase::Idle) {
        memory.collectAt = memory.threshold;
      } else if (memory.verify || isBehind(m)) {
        memory.collectAt = -1;
      } else {
        memory.collectAt = m.heap.objects + memory.slice / PACE;
      }
    }

    // ── the incremental stress mode's check ──

    Vector<int64_t> rootObjects(Machine& m);

    int64_t verifyOne(Heap& heap, Vector<uint8_t>& seen, Vector<int64_t>& pending);

    // At the end of an incremental marking, everything the program can reach now must be marked. Either it was reachable when the
    // cycle began (and the barrier kept it), or it was made during the cycle (black). A walk of everything reachable now, made apart
    // from the marking, must find no unmarked object. One it does find is the sign of a missing barrier, and the run stops before
    // it is freed.
    Outcome verifyTheMarks(Machine& m) {
      // The walk has a `seen` of its own: it does not trust the marks. So a black object that a store without a barrier made refer
      // to a white one is walked into too.
      Vector<uint8_t> seen(m.heap.cells.size(), 0);
      Vector<int64_t> pending = rootObjects(m);
      int64_t missed = 0;

      while (!pending.empty()) {
        missed += verifyOne(m.heap, seen, pending);
      }

      if (missed == 0) {
        return VoidOutcome();
      }

      return Trap("gc verify: " + to_string(missed) + " reachable objects were left unmarked by the incremental marking (a write barrier is missing)");
    }

    void addReference(Vector<int64_t>& to, const Value& v) {
      if (referenceIn(v) >= 0) {
        to.push_back(referenceIn(v));
      }
    }

    void addReferences(Vector<int64_t>& to, const Vector<Value>& values) {
      for (const Value& v : values) {
        addReference(to, v);
      }
    }

    Vector<int64_t> rootObjects(Machine& m) {
      Vector<int64_t> roots;

      for (const Frame* frame : m.runningFrames) {
        addReferences(roots, frame->slots);
      }

      for (const Frame* frame : m.runningFrames) {
        addReferences(roots, frame->stack);
      }

      addReferences(roots, m.globals);
      addReferences(roots, m.pinned);

      return roots;
    }

    // One object of the walk: when not seen yet, what it refers to is walked next. The answer is 1 when it was left unmarked.
    int64_t verifyOne(Heap& heap, Vector<uint8_t>& seen, Vector<int64_t>& pending) {
      const auto cell = static_cast<size_t>(pending.back());
      pending.pop_back();

      if (seen[cell] != 0) {
        return 0;
      }

      seen[cell] = 1;

      for (size_t k = 0; k < valueCount(heap.cells[cell]); k++) {
        addReference(pending, valueAt(heap.cells[cell], k));
      }

      return heap.marking.marked[cell] != 0 ? 0 : 1;
    }

    // ── the pauses ──

    // 1000 buckets 1 µs wide below 1 ms, 990 buckets 100 µs wide from 1 ms to 100 ms, then a spare bucket that is never filled
    // (index 1990) and last the bucket for the rest (index 1991, whose top bucketTop gives as 100.2 ms): 1000 + 990 + 1 + 1.
    constexpr size_t BUCKETS = 1992;

    size_t bucketOf(int64_t nanoseconds) {
      const int64_t micros = nanoseconds / 1000;

      if (micros < 1000) {
        return static_cast<size_t>(micros);
      }

      if (micros < 100000) {
        return static_cast<size_t>(1000 + (micros - 1000) / 100);
      }

      return BUCKETS - 1;
    }

    // The longest pause a bucket stands for, in nanoseconds.
    int64_t bucketTop(size_t bucket) {
      if (bucket < 1000) {
        return (static_cast<int64_t>(bucket) + 1) * 1000;
      }

      return (1000 + (static_cast<int64_t>(bucket) - 1000 + 1) * 100) * 1000;
    }

    void countThePause(Pauses& pauses, int64_t nanoseconds) {
      if (pauses.buckets.empty()) {
        pauses.buckets.assign(BUCKETS, 0);
      }

      pauses.count++;
      pauses.total += nanoseconds;
      pauses.longest = max(pauses.longest, nanoseconds);
      pauses.buckets[bucketOf(nanoseconds)]++;
    }

  }  // namespace

  // The top of the bucket the share falls in, never more than the longest pause.
  int64_t PausePercentile(const Pauses& pauses, int64_t share) {
    const int64_t wanted = (pauses.count * share + 99) / 100;
    int64_t seen = 0;

    for (size_t b = 0; b < pauses.buckets.size(); b++) {
      seen += pauses.buckets[b];

      if (seen >= wanted && seen > 0) {
        return min(bucketTop(b), pauses.longest);
      }
    }

    return pauses.longest;
  }

  // ── a new object's colour (VmValues.cpp's Alloc calls this) ──

  namespace {

    // Black while a cycle marks (it holds nothing from before the cycle). Marked too while the sweep has yet to reach its cell, so
    // the sweep keeps it. Otherwise white, for the next cycle to decide.
    bool isBornMarked(const Marking& marking, size_t cell) {
      if (marking.phase == Phase::Marking) {
        return true;
      }

      return marking.phase == Phase::Sweeping && cell >= marking.sweepAt && cell < marking.sweepEnd;
    }

  }  // namespace

  void ColourNewObject(Heap& heap, int64_t id) {
    Marking& marking = heap.marking;
    const auto cell = static_cast<size_t>(id);

    if (cell >= marking.marked.size()) {
      marking.marked.resize(cell + 1, 0);
    }

    marking.marked[cell] = isBornMarked(marking, cell) ? 1 : 0;

    if (marking.phase == Phase::Marking) {
      marking.markedObjects++;
    }
  }

  namespace {

    // The incremental stress mode is the incremental mode with the smallest slice.
    Memory modeOf(const MemorySettings& settings) {
      Memory memory;
      memory.collectEverySafePoint = settings.collectEverySafePoint;
      memory.incremental = settings.incremental || settings.verify;
      memory.verify = settings.verify;
      memory.slice = settings.verify ? 1 : max(int64_t{1}, settings.slice);

      return memory;
    }

  }  // namespace

  // A new machine's policy: nothing has survived yet, so the first collection comes at the minimum threshold (or the ceiling, if lower).
  Memory MemoryPolicy(const MemorySettings& settings) {
    Memory memory = modeOf(settings);
    memory.ceiling = settings.ceiling;
    setTheThreshold(memory, 0);
    memory.collectAt = memory.threshold;

    return memory;
  }

}  // namespace polyir::vm
