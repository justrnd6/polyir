/* The garbage collector of a natively built polyir program (docs/memory.md, Parts 2 and 3). Read it top to bottom: this file is the whole
 * collector. It is the native twin of the VM's collector (runtime/src/vm/Collector.cpp) and works the same way; what differs is how it finds
 * things.
 *
 * How it gets into a program. gen_collector_ir.sh compiles this file to LLVM IR, and the LLVM compiler (src/emit/llvm) writes that IR into
 * every module that allocates, so a program needs nothing extra at link time. The compiled code calls the functions marked "(the program calls
 * this)"; everything else is the collector's own.
 *
 * Names. The functions and globals the compiled code uses are the collector's C ABI and spelled like one: gc_poll, gc_allocate, gc_top. The
 * collector's own helpers and constants are `static` and plain camelCase (markWord, sweepAPage, kClassBytes). Their IR lands in the same
 * module as the program's own functions, which keep the names the program gives them, so filter_ir.py spells every one of them
 * `@.gc.<name>` there (`@.gc.markWord`): a dot cannot appear in a polyir name, so no program function or global can meet one. Functions are
 * declared just before the first one that calls them. The file follows a collection: a safe point, a cycle, marking, sweeping and the
 * policy, then the pieces those use (the sets of addresses, the pages, the objects, allocating, the shadow stack, the pauses and starting).
 *
 * What it is for. A running program keeps making OBJECTS: structs, lists and their element blocks, maps and their tables, closures and their
 * environments, the cells behind captured variables, optionals, eithers and variants, interface values, coroutines and their frames, and the
 * strings it builds. Nothing in the program says when one is no longer needed, so from time to time the collector works it out and gives the
 * memory back, and a program that keeps a bounded amount of data then uses a bounded amount of memory, however long it runs.
 *
 * How: MARK AND SWEEP, as on the VM.
 *   1. Mark. Start from the ROOTS, the places the program can read a reference from without going through another object: the frame record
 *      of every running function (below) and the globals. Mark every object a root refers to, then every object a marked object refers to,
 *      and so on until nothing new turns up. A marked object is REACHABLE: the program may still use it.
 *   2. Sweep. An object that was not marked can never be reached again: its memory is given back, to be used for the next objects.
 *
 * IN SLICES, so that no pause is long (Part 3). A game draws a frame every 16 ms; a collector that marks a million objects at once stops it
 * for tens of milliseconds. So by default the collector is INCREMENTAL: one COLLECTION (a CYCLE) is spread over many short SLICES, and the
 * program runs between them. Each slice does at most a budget of work (POLYIR_GC_SLICE words looked at), so the longest pause is set by the
 * budget, not by how much data the program keeps. The words used to explain it are the TRI-COLOUR ones:
 *   - WHITE: not reached yet. When marking ends, the white objects are the garbage.
 *   - GREY: reached (its mark bit is set), but its own words not looked at yet: it is on the WORKLIST.
 *   - BLACK: reached, and every word of it looked at.
 * Marking is "take a grey object, look at its words (each object they point at turns grey), and it is black"; it is over when nothing is grey.
 *
 * The danger: the program runs between slices and moves references around. If it takes the only reference to a white object W out of an
 * object that is still grey (overwriting it) and keeps W only where marking has already looked (a black object, or its own frame record,
 * which marking looked at when the cycle began), W stays white and is freed while the program still uses it. A WRITE BARRIER stops that:
 * the compiled code calls gc_write_barrier before every store of a reference into an object. This collector's barrier is the
 * SNAPSHOT-AT-THE-BEGINNING (Yuasa) kind: while marking runs, it greys the reference the store is about to OVERWRITE. So no reference that
 * was in the heap when the cycle began can disappear before marking has seen it, and every object reachable at the start of the cycle (the
 * snapshot) is marked. Objects made during the cycle are made black (marked at once), so nothing new can be lost either. Why this kind and
 * not the other common one (Dijkstra's, which greys the reference being STORED): with Dijkstra's barrier, stores into the frame records and
 * the globals (which have no barrier) can hide an object, so the roots must be looked at again at the end of marking, and whatever that
 * finds must be marked before the cycle can end: an end-of-cycle pause that grows with the data the program keeps. With the snapshot
 * barrier the roots are looked at once, at the start of the cycle (a pause as long as the stack is deep, and the globals), and the cycle
 * ends the moment the worklist is empty, inside a slice. The price is that everything reachable at the start survives the cycle, even what
 * the program drops during it (it goes at the next cycle).
 *
 * The SWEEP is in slices too: a cursor walks the pages (and then the large objects), a few at a time. An object made while the sweep has
 * not reached its page yet is made marked, so the sweep keeps it; one made in a page already swept is made unmarked, ready for the next cycle.
 * Pacing: every byte the program allocates during a cycle earns the collector one word of work, and a slice runs at a safe point once that
 * reaches the budget. Eight words looked at per word allocated finish the marking of a heap at its threshold (twice what survived the last
 * cycle, so at most half of it reachable) before the program has allocated a sixteenth as much again: little floating garbage (what was
 * made during the marking, black, and survives the cycle), so the heap peaks about where a stop-the-world one does. Should the program
 * allocate faster than that (more than half again what the heap held when the cycle began), a slice runs at every safe point until the
 * cycle is over; each one is still within the budget.
 * POLYIR_GC_MODE=stop makes every collection one pause instead (the whole cycle at once, stop-the-world), as before Part 3.
 *
 * Where objects live: PAGES. The collector has its own allocator. Almost every object is small (a struct, a list header, an optional: 16 to
 * 48 bytes), so small objects (up to GC_LARGEST_SMALL bytes) are cut from PAGES of GC_PAGE_BYTES (64 KiB), and every page holds objects of
 * ONE SIZE (its SIZE CLASS: 16, 32, 48, ... 2048 bytes; a request is rounded up to the next class) and ONE LAYOUT (below). A page starts with
 * a HEADER (a GcPage): its size class, its layout, and two bitmaps with one bit per slot, ALLOCATED (the slot holds an object) and MARKED (the
 * current collection reached it). The slots follow the header, one after the other. Pages are aligned to their size, so the page that holds
 * an address is the address with its low 16 bits cleared, and where an object starts follows from its size class. So:
 *   - allocating is finding a clear bit in the page's allocated bitmap and setting it: no hashing, no header per object;
 *   - a word is a reference exactly when its page is one of the collector's (the PAGE SET, a small hash set of page addresses), it falls on
 *     the start of a slot, and that slot's allocated bit is set;
 *   - sweeping a page is, for every 64 slots, `allocated = marked` and `marked = 0`: what was not marked is free again, with no free() per
 *     object. A page left with no objects is kept as a SPARE for the next page any size class needs, and given back to the C library once it
 *     has sat unused through a whole round between two collections.
 * The few LARGE objects (a long list's elements, a big map's tables) are malloc blocks with a 32-byte header in front (their size, layout,
 * mark bit and the links of the LARGE LIST, newest first, that the sweep walks), and their addresses are in a hash set of their own (the LARGE
 * SET).
 *
 * Compiled code keeps references in machine words, and a word could also be a number, the address of a string constant, memory a C library
 * made, or an address inside an object, so the collector follows a word only when it is EXACTLY the start of an object it made. Anything else
 * (a number, a constant, host memory, a raw `ptr<T>`, an address into the middle of an object) is left alone: never followed, never freed.
 *
 * The LAYOUT of an object says whether to look inside it: GC_DATA, no references (a string, a block of numbers), or GC_REFERENCES, any of its
 * 8-byte words may be one (a struct with a reference field, a block of references, a map's tables, a coroutine's frame). The compiler picks it
 * from the type at each allocation. A GC_REFERENCES object starts zeroed, so the words the program has not written yet (a list's spare
 * capacity, the rest of a slot past what was asked for) keep nothing alive.
 *
 * Where the roots are: a SHADOW STACK. A compiled function that works with references keeps them in a FRAME RECORD on its own stack (a GcFrame):
 * a link to its caller's record, a count, and one slot per reference-holding variable and temporary. It links the record into the chain on
 * entry (gc_push_frame) and unlinks it on every way out (gc_pop_frame), so `gc_top` and the `caller` links reach every running function's
 * slots. The compiler stores each reference a function works with into a slot as soon as it is made, so at a safe point every live reference
 * is in a record, in a global, or inside an object those reach. (A coroutine's record lives in its frame: linked while the coroutine runs,
 * unlinked while it is suspended, when the frame is reached through the coroutine object like any other memory. A coroutine overwrites its
 * frame without barriers, so when one resumes during marking its whole frame is looked at first: see gc_link_frame.)
 *
 * When it runs: only at a SAFE POINT, a call of gc_poll, which the compiler puts at the start of every function that allocates and at the head
 * of every loop in one. Allocation never collects (gc_allocate only takes a slot), so the compiler's own sequences of allocations (a list
 * header, then its element block) never meet a collection halfway. A cycle begins once the objects hold more than GC_MINIMUM_THRESHOLD bytes;
 * after each, the next begins at twice what survived: a program that keeps a steady amount of data then collects at a steady rate.
 *
 * Only a program's own `main` starts the collector (gc_start, from the entry point the compiler writes). A library built from polyir and
 * called by a host never collects: the host may hold references the collector cannot see. Before gc_start, or without it, objects are plain
 * malloc blocks. The collector is single-threaded, as polyir programs are.
 *
 * The environment can change the policy: POLYIR_GC_MODE=stop|incremental, POLYIR_GC_SLICE=WORDS (the budget of one slice), POLYIR_GC_STRESS=1
 * (a whole stop-the-world collection at every safe point, so a missing root shows up at once as a wrong answer), POLYIR_GC_STRESS=incremental
 * (a slice of one word at every safe point, a new cycle as soon as one ends, and at the end of every marking a check that nothing reachable
 * was left unmarked: a missing write barrier stops the program with a message), POLYIR_GC_CEILING=BYTES sets the ceiling, and
 * POLYIR_GC_REPORT=1 prints what the collector did (with its pauses) when the program ends. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── what the compiled program shares with the collector ── */

/* One running function's frame record. `slots` holds `count` references; they start at zero and the compiled code keeps them current. */
typedef struct GcFrame {
  struct GcFrame* caller;
  int64_t count;
  void* slots[];
} GcFrame;

/* The layouts (the last argument of every allocation). */
enum { GC_DATA = 0, GC_REFERENCES = 1 };

/* The innermost running function's frame record. Like every name the compiled code uses, it keeps the C ABI's gc_ spelling. */
GcFrame* gc_top = 0;  // style-ignore-line: variable_case

/* Set while a cycle is marking: the write barrier reads it before every store of a reference, so it is a global of its own (one load). */
int32_t gc_marking = 0;  // style-ignore-line: variable_case

/* ── what the collector keeps ── */

/* A page: 64 KiB, aligned to 64 KiB, so `address & ~(GC_PAGE_BYTES - 1)` is the page of any address inside it. */
#define GC_PAGE_BYTES ((uintptr_t)64 << 10)

/* The bitmaps have a bit for every slot the smallest size class (16 bytes) could fit in a page: 4,096 bits, 64 words of 64. */
#define GC_BITMAP_WORDS (GC_PAGE_BYTES / 16 / 64)

/* The header at the start of every page. Every slot of the page is `slotBytes` long and the objects in it are laid out as `layout`. Bit i of
 * `allocated` is set while slot i holds an object; bit i of `marked` is set once the current cycle has reached it (grey or black: which of the
 * two is the worklist's business). `swept` is the cycle whose sweep last passed the page: while it is behind the collector's `cycle` the page
 * is still to be swept, and its marks still tell the live objects from the garbage. */
typedef struct GcPage {
  struct GcPage* next;  /* the next page of the same size class and layout (or the next spare) */
  uint32_t slotBytes;
  uint32_t slotCount;
  uint32_t layout;
  uint32_t used;  /* the slots that hold an object */
  uint32_t words;  /* the bitmap words that cover slotCount slots */
  uint32_t cursor;  /* no bitmap word before this one has a free slot: where the next allocation starts looking */
  uint32_t swept;
  uint32_t unused;
  uint64_t allocated[GC_BITMAP_WORDS];
  uint64_t marked[GC_BITMAP_WORDS];
} GcPage;

/* The first slot starts after the header, at a multiple of 16 bytes, so every object is 16-byte aligned as malloc's blocks are (LLVM's vector
 * loads assume it). With 16-byte slots a page holds 4,029 of them; the header is 1.6% of the page. */
#define GC_FIRST_SLOT ((sizeof(GcPage) + 15) / 16 * 16)

/* Objects bigger than this are large: a page would hold too few of them for the rounding to its size class to be worth it. */
#define GC_LARGEST_SMALL 2048

/* The size classes. Multiples of 16 (the alignment), close together where most objects are and about a quarter apart above 128 bytes, so a
 * request wastes less than a quarter of its slot. */
#define GC_CLASS_COUNT 24
static const uint32_t kClassBytes[GC_CLASS_COUNT] = {16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256,
                                                        320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048};

/* The pages of one size class and one layout, in a list: allocation takes slots from `current` and moves along the list as pages fill. */
typedef struct GcClass {
  GcPage* first;
  GcPage* last;
  GcPage* current;
  uint32_t slotBytes;
  uint32_t layout;
} GcClass;

/* In front of every LARGE object. 32 bytes, so the object stays 16-byte aligned as malloc's blocks are. `newer` and `older` link the large
 * list, newest first. */
typedef struct GcHeader {
  struct GcHeader* newer;
  struct GcHeader* older;
  uint64_t bytes;
  uint32_t layout;
  uint32_t marked;
} GcHeader;

/* A set of addresses (the pages, the large objects): open addressing with linear probing, 0 in an empty entry, never more than three quarters
 * full. */
typedef struct GcSet {
  uintptr_t* entries;
  uint64_t capacity;  /* a power of two */
  uint64_t count;
} GcSet;

/* A grey object: reached, its own words looked at up to `next` (a long list is looked at over several slices). `entry` is the object's
 * address, plus GC_IN_A_PAGE when the object is in a page (its size is then its page's size class; without the tag it is a large object,
 * whose size is in its header), so no look-up finds its size again. */
typedef struct GcGrey {
  uintptr_t entry;
  uint64_t next;
} GcGrey;

/* The grey objects, explicit, so a long chain cannot overflow the C stack. */
typedef struct GcWorklist {
  GcGrey* items;
  uint64_t count;
  uint64_t capacity;
} GcWorklist;

/* How long the pauses were: their number, the longest, their sum, and how many fell in each bucket of GC_BUCKETS (1 µs wide below 1 ms, 100 µs
 * wide up to 100 ms, then one bucket for the rest), from which the report reads the median and the 99th percentile. */
#define GC_BUCKETS 1992
typedef struct GcPauses {
  uint64_t count;
  uint64_t longest;  /* nanoseconds */
  uint64_t total;
  uint32_t buckets[GC_BUCKETS];
} GcPauses;

/* Where the sweep is: the class it is in (0 to 2 * GC_CLASS_COUNT - 1, by layout then size class; 2 * GC_CLASS_COUNT means the large objects)
 * and the last page of that class it kept (0 before the first), or the next large object to look at. */
typedef struct GcSweep {
  int32_t klass;
  GcPage* kept;
  GcHeader* large;
} GcSweep;

/* What a cycle is doing. */
enum { GC_IDLE = 0, GC_MARKING = 1, GC_SWEEPING = 2 };

typedef struct GcCollector {
  int32_t running;  /* gc_start was called: objects come from pages and the large set, and safe points may collect */
  int32_t phase;  /* GC_IDLE, GC_MARKING or GC_SWEEPING */
  int32_t incremental;  /* POLYIR_GC_MODE: a cycle in slices, or all at once */
  int32_t stress;  /* POLYIR_GC_STRESS=1: a whole collection at every safe point */
  int32_t verify;  /* POLYIR_GC_STRESS=incremental: the marks are checked at the end of every marking */
  int32_t report;  /* POLYIR_GC_REPORT: say what was done when the program ends */
  uint32_t cycle;  /* the cycles whose marking is over: a page whose `swept` is behind this is still to be swept */
  GcClass classes[2][GC_CLASS_COUNT];  /* by layout, then size class */
  uint8_t classOf[GC_LARGEST_SMALL / 16 + 1];  /* the size class of a request of up to 16 * k bytes, by k */
  GcSet pages;  /* every page the collector holds, spares included */
  GcPage* spares;  /* empty pages, ready for any size class */
  uint64_t spareCount;
  uint64_t spareLow;  /* the fewest spares there were since the last collection */
  GcSet large;  /* the large objects */
  GcHeader* newestLarge;  /* the start of the large list */
  uint64_t bytes;  /* what the objects take up (the garbage the sweep has not reached included): their slots, and the large ones' blocks */
  uint64_t objects;  /* how many there are */
  uintptr_t lowest, highest;  /* every page and large object lies in [lowest, highest]: a quick no for most numbers */
  uint64_t collectAt;  /* a safe point does the collector's work once `bytes` is past this */
  uint64_t threshold;  /* between cycles, where the next one begins (collectAt is then this) */
  uint64_t ceiling;  /* a cycle that leaves more bytes reachable than this stops the program */
  void*** globals;  /* the addresses of the globals that hold references */
  int64_t globalCount;
  GcWorklist worklist;
  uint64_t slice;  /* the budget of one slice, in words looked at */
  uint64_t cycleStart;  /* `bytes` when the cycle began */
  uint64_t survivors;  /* the bytes the sweep has kept so far: at its end, what survived the cycle */
  int32_t releasedWhileMarking;  /* the cycle's marking saw an object given back (a worklist entry may no longer be an object) */
  GcSweep sweep;
  GcPauses pauses;
  uint64_t collections, reclaimedObjects, reclaimedBytes, peakBytes;  /* what POLYIR_GC_REPORT prints */
} GcCollector;

/* Everything the collector keeps. Its name starts gc_ so filter_ir.py makes it a shared (linkonce_odr) global: modules built apart and linked
 * together then share one collector. */
GcCollector gc_state = {0};  // style-ignore-line: variable_case

/* A heap smaller than this is never worth collecting: collecting smaller ones would spend the run sweeping instead of running. */
#define GC_MINIMUM_THRESHOLD ((uint64_t)8 << 20)

/* The default ceiling, 4 GiB still reachable: far beyond what a polyir program keeps, still short of what a laptop has. */
#define GC_DEFAULT_CEILING ((uint64_t)4 << 30)

/* The default budget of one slice: 10,000 words looked at, some 20 to 60 µs on a laptop. */
#define GC_DEFAULT_SLICE 10000

/* The most words of one object looked at before the grey objects they pushed are traced, even in a slice with no budget (stopping the
 * world): it keeps the worklist short. */
#define GC_CHUNK_WORDS 4096

/* The work one allocated byte earns while a cycle runs: one word looked at (eight per word allocated; see "Pacing" above). */
#define GC_PACE_WORDS_PER_BYTE 1

#define GC_FIRST_CAPACITY 1024

/* The tag of a worklist entry for an object in a page (objects are 16-byte aligned, so the low bit of their address is free). */
#define GC_IN_A_PAGE 1

/* ── a safe point ── */

/* The steps of a safe point. */
static uint64_t nowInNanoseconds(void);

static void doTheWork(void);

static void countThePause(uint64_t nanoseconds);

/* A safe point (the program calls this): when there is collecting to do, it is done here, and how long it took is one pause. Whether there
 * is, is one comparison: `collectAt` is where the next work is due (see scheduleTheNextWork). */
void gc_poll(void) {
  if (!gc_state.running || gc_state.bytes <= gc_state.collectAt) {
    return;
  }

  const uint64_t started = nowInNanoseconds();
  doTheWork();
  countThePause(nowInNanoseconds() - started);
}

static void noteThePeak(void);

static void beginACycle(void);

static int mustFinish(void);

static void finishTheCycle(void);

static void markASlice(uint64_t budget);

static void sweepASlice(uint64_t budget);

static void scheduleTheNextWork(void);

/* A new cycle begins with its roots. Then comes one slice of marking or of sweeping. When the collector stops the world (or must: see
 * mustFinish), the whole cycle runs at once instead. Last, it works out when the next work is due. */
static void doTheWork(void) {
  GcCollector* gc = &gc_state;
  noteThePeak();  /* the heap is at its fullest when collecting is due */

  if (gc->phase == GC_IDLE) {
    beginACycle();
  }

  if (mustFinish()) {
    finishTheCycle();
  } else if (gc->phase == GC_MARKING) {
    markASlice(gc->slice);
  } else {
    sweepASlice(gc->slice);
  }

  scheduleTheNextWork();
}

/* The most the objects have held at once, for the report. */
static void noteThePeak(void) {
  if (gc_state.bytes > gc_state.peakBytes) {
    gc_state.peakBytes = gc_state.bytes;
  }
}

/* Stop-the-world mode and its stress mode finish every cycle at once; so does an incremental cycle that let the objects pass the ceiling. */
static int mustFinish(void) {
  const GcCollector* gc = &gc_state;

  return !gc->incremental || gc->stress || gc->bytes > gc->ceiling;
}

/* ── a cycle ── */

static void markTheRoots(void);

/* The snapshot: from here on, everything reachable now will be marked. The roots are looked at now, in this pause, and never again in this
 * cycle. The barrier keeps what was in the heap, and what the program makes is black. */
static void beginACycle(void) {
  GcCollector* gc = &gc_state;
  gc->collections++;
  gc->phase = GC_MARKING;
  gc_marking = 1;

  gc->cycleStart = gc->bytes;
  gc->releasedWhileMarking = 0;
  markTheRoots();
}

/* Marks to the end, then sweeps to the end. */
static void finishTheCycle(void) {
  GcCollector* gc = &gc_state;

  while (gc->phase == GC_MARKING) {
    markASlice(UINT64_MAX);
  }

  while (gc->phase == GC_SWEEPING) {
    sweepASlice(UINT64_MAX);
  }
}

static uint64_t traceSome(uint64_t budget);

static void finishMarking(void);

/* Grey objects are looked at until the budget is spent; once none is left, the marking is over. */
static void markASlice(uint64_t budget) {
  GcWorklist* work = &gc_state.worklist;
  uint64_t done = 0;

  while (done < budget && work->count > 0) {
    done += traceSome(budget - done);
  }

  if (work->count == 0) {
    finishMarking();
  }
}

static void verifyTheMarks(void);

static void setTheThreshold(uint64_t survivors);

static void checkTheCeiling(uint64_t survivors);

static void startTheSweep(void);

/* What is marked now survives (what the program made during the cycle among it). The sweep starts: every page is now behind the
 * collector's cycle, so still to be swept. */
static void finishMarking(void) {
  GcCollector* gc = &gc_state;

  if (gc->verify) {
    verifyTheMarks();
  }

  gc_marking = 0;
  gc->phase = GC_SWEEPING;
  gc->cycle++;
  startTheSweep();
}

/* ── 1. mark: the roots ── */

static void markWord(uintptr_t word);

/* Every running function's record, then every global that holds a reference. */
static void markTheRoots(void) {
  for (GcFrame* frame = gc_top; frame != 0; frame = frame->caller) {
    for (int64_t i = 0; i < frame->count; i++) {
      markWord((uintptr_t)frame->slots[i]);
    }
  }

  for (int64_t i = 0; i < gc_state.globalCount; i++) {
    markWord((uintptr_t)*gc_state.globals[i]);
  }
}

static int mightBeObject(uintptr_t word);

static GcPage* pageOf(uintptr_t word);

static int64_t slotIndex(const GcPage* page, uintptr_t word);

static void markSlot(GcPage* page, int64_t slot, uintptr_t object);

static int isLarge(uintptr_t word);

static void markLarge(uintptr_t object);

/* A word that is the start of an object not yet marked marks it (it turns grey). That is an object in a page when the word falls on an
 * allocated slot of one. Else it is a large object, when the word is in the large set. */
static void markWord(uintptr_t word) {
  if (!mightBeObject(word)) {
    return;
  }

  GcPage* page = pageOf(word);

  if (page != 0) {
    markSlot(page, slotIndex(page, word), word);
  } else if (isLarge(word)) {
    markLarge(word);
  }
}

static int isBitSet(const uint64_t* bitmap, uint64_t index) {
  return (bitmap[index / 64] >> (index % 64)) & 1;
}

static void setBit(uint64_t* bitmap, uint64_t index) {
  bitmap[index / 64] |= (uint64_t)1 << (index % 64);
}

static void push(GcWorklist* list, uintptr_t entry);

static void pushFrom(GcWorklist* list, uintptr_t entry, uint64_t next);

/* Marking only sets the bit and, when the object may hold references, puts it on the worklist (grey). Its own words are looked at by the
 * trace, so nothing here recurses. A GC_DATA object has nothing to look at: it is black at once. */
static void markSlot(GcPage* page, int64_t slot, uintptr_t object) {
  if (slot < 0 || isBitSet(page->marked, (uint64_t)slot)) {
    return;
  }

  setBit(page->marked, (uint64_t)slot);

  if (page->layout == GC_REFERENCES) {
    push(&gc_state.worklist, object | GC_IN_A_PAGE);
  }
}

static GcHeader* headerOf(uintptr_t object) {
  return (GcHeader*)(object - sizeof(GcHeader));
}

static void markLarge(uintptr_t object) {
  GcHeader* header = headerOf(object);

  if (header->marked) {
    return;
  }

  header->marked = 1;

  if (header->layout == GC_REFERENCES) {
    push(&gc_state.worklist, object);
  }
}

/* ── 1. mark: looking at the grey objects ── */

static int isStillAnObject(uintptr_t entry);

static uint64_t traceAChunk(GcGrey grey, uint64_t budget);

static uint64_t chunkEnd(uint64_t from, uint64_t total, uint64_t budget);

static void markWords(const uintptr_t* words, uint64_t from, uint64_t to);

static uintptr_t objectAddress(uintptr_t entry);

static uint64_t wordsOf(uintptr_t entry);

/* Looks at the next words of the grey object on top of the worklist: at most the budget, and at most GC_CHUNK_WORDS of them. A list of a
 * million elements is looked at over many slices, so it cannot make one long pause. Once all its words are done, the object is black. The
 * answer is the work done: the words, and one for taking the object. */
static uint64_t traceSome(uint64_t budget) {
  GcWorklist* work = &gc_state.worklist;
  const GcGrey grey = work->items[--work->count];

  /* Between two slices the program may have given an object on the worklist back (a list's old elements after it grew, a map's old
   * tables). The barrier then already looked at its words, so after that an entry that is no longer an object is skipped. One whose slot
   * holds a new object since is looked at anyway, which does no harm. */
  if (gc_state.releasedWhileMarking && !isStillAnObject(grey.entry)) {
    return 1;
  }

  return traceAChunk(grey, budget);
}

/* One chunk of a grey object: its words from `grey.next` on, as many as chunkEnd allows. When words are left, the object goes back on the
 * worklist before the chunk is looked at. The grey objects the chunk pushes then lie above it and are traced first, so the worklist stays
 * short. A list of a million elements does not put all of them on it at once. The answer is the work done. */
static uint64_t traceAChunk(GcGrey grey, uint64_t budget) {
  const uint64_t total = wordsOf(grey.entry);
  const uint64_t from = grey.next < total ? grey.next : total;
  const uint64_t to = chunkEnd(from, total, budget);

  if (to < total) {
    pushFrom(&gc_state.worklist, grey.entry, to);
  }

  markWords((const uintptr_t*)objectAddress(grey.entry), from, to);

  return to - from + 1;
}

/* Where a chunk that starts at word `from` ends: at most the budget and at most GC_CHUNK_WORDS words on, and never past the object's end. */
static uint64_t chunkEnd(uint64_t from, uint64_t total, uint64_t budget) {
  const uint64_t most = budget < GC_CHUNK_WORDS ? budget : GC_CHUNK_WORDS;

  return from + (total - from < most ? total - from : most);
}

/* Marks what each of the words [from, to) refers to. */
static void markWords(const uintptr_t* words, uint64_t from, uint64_t to) {
  for (uint64_t i = from; i < to; i++) {
    markWord(words[i]);
  }
}

/* A worklist entry without its GC_IN_A_PAGE tag: the object's address. */
static uintptr_t objectAddress(uintptr_t entry) {
  return entry & ~(uintptr_t)GC_IN_A_PAGE;
}

static int isStillAnObject(uintptr_t entry) {
  const uintptr_t object = objectAddress(entry);

  if (!(entry & GC_IN_A_PAGE)) {
    return isLarge(object);
  }

  return slotIndex((const GcPage*)(object & ~(GC_PAGE_BYTES - 1)), object) >= 0;
}

/* How many 8-byte words of a worklist entry's object to look at: its slot, or what the large object's header says. */
static uint64_t wordsOf(uintptr_t entry) {
  if (entry & GC_IN_A_PAGE) {
    return ((const GcPage*)(entry & ~(GC_PAGE_BYTES - 1)))->slotBytes / sizeof(uintptr_t);
  }

  return headerOf(entry)->bytes / sizeof(uintptr_t);
}

/* ── the write barrier (the program calls this) ── */

/* The compiled code calls this with a word's address before it stores a reference into that word of an object. The word may be in a field,
 * an element, a map's table, a captured variable's cell or a coroutine's yield slot. While marking runs, the reference about to be
 * overwritten turns grey. So whatever was in the heap when the cycle began is marked before it can disappear (snapshot at the beginning).
 * The rest of the time it is one load and a branch. */
void gc_write_barrier(void** word) {
  if (gc_marking) {
    markWord((uintptr_t)*word);
  }
}

/* The same for every word of an object at once. It runs before the collector itself gives back or moves an object whose words the program
 * still needs (gc_resize, gc_release). It also runs when a coroutine resumes (gc_link_frame). */
static void shadeWords(const void* object, uint64_t bytes) {
  markWords((const uintptr_t*)object, 0, bytes / sizeof(uintptr_t));
}

/* ── is this word an object? ── */

/* Objects are 16-byte aligned and lie between the lowest and the highest address the collector holds: most numbers fail here, before any
 * look-up. */
static int mightBeObject(uintptr_t word) {
  const GcCollector* gc = &gc_state;

  return word != 0 && (word & 15) == 0 && word >= gc->lowest && word <= gc->highest;
}

static int setHas(const GcSet* set, uintptr_t address);

/* The page that holds the word, when it is one of the collector's, or 0. The page is the word with its low 16 bits cleared, looked up in
 * the page set. */
static GcPage* pageOf(uintptr_t word) {
  const uintptr_t page = word & ~(GC_PAGE_BYTES - 1);

  return setHas(&gc_state.pages, page) ? (GcPage*)page : 0;
}

/* The slot an object starts at, or -1. The word must fall exactly on the start of a slot, and the slot must hold an object. An address
 * into the middle of an object, or into the header, is not a reference. */
static int64_t slotIndex(const GcPage* page, uintptr_t word) {
  const uintptr_t first = (uintptr_t)page + GC_FIRST_SLOT;

  if (word < first || (word - first) % page->slotBytes != 0) {
    return -1;
  }

  const uint64_t slot = (word - first) / page->slotBytes;

  return slot < page->slotCount && isBitSet(page->allocated, slot) ? (int64_t)slot : -1;
}

static int isLarge(uintptr_t word) {
  return setHas(&gc_state.large, word);
}

/* The page of an object in a page (the word starts an allocated slot of it). 0 for anything else: a large object, or memory the collector
 * did not make. */
static GcPage* pageOfObject(uintptr_t word) {
  GcPage* page = mightBeObject(word) ? pageOf(word) : 0;

  return page != 0 && slotIndex(page, word) >= 0 ? page : 0;
}

static int isLargeObject(uintptr_t word) {
  return mightBeObject(word) && isLarge(word);
}

/* ── 2. sweep ── */

#define GC_LARGE_TURN (2 * GC_CLASS_COUNT)

/* The sweep starts at the first class's first page, with nothing kept yet. */
static void startTheSweep(void) {
  GcSweep* sweep = &gc_state.sweep;
  gc_state.survivors = 0;
  sweep->klass = 0;
  sweep->kept = 0;
  sweep->large = gc_state.newestLarge;
}

static uint64_t sweepAPage(void);

static uint64_t sweepALarge(void);

static void finishSweeping(void);

/* Pages, then large objects, until the budget is spent or everything is swept. */
static void sweepASlice(uint64_t budget) {
  GcSweep* sweep = &gc_state.sweep;
  uint64_t done = 0;

  while (done < budget && sweep->klass < GC_LARGE_TURN) {
    done += sweepAPage();
  }

  while (done < budget && sweep->large != 0) {
    done += sweepALarge();
  }

  if (sweep->klass == GC_LARGE_TURN && sweep->large == 0) {
    finishSweeping();
  }
}

static GcClass* sweepClass(void) {
  const int32_t k = gc_state.sweep.klass;

  return &gc_state.classes[k / GC_CLASS_COUNT][k % GC_CLASS_COUNT];
}

static uint64_t sweepAndKeepOrDrop(GcClass* klass, GcPage* page);

static uint32_t sweepThePage(GcPage* page);

static uint32_t sweepABitmapWord(GcPage* page, uint32_t bitmapWord);

static void countReclaimed(uint64_t objects, uint64_t bytes);

static void dropPage(GcClass* klass, GcPage* page);

static void nextClass(GcClass* klass);

/* The next page of the class being swept: freed of its garbage, kept if anything is left in it, a spare if not. A page the sweep has already
 * passed (one the class took after the marking ended) ends the class: every page after it is newer still. The answer is the work done. */
static uint64_t sweepAPage(void) {
  GcClass* klass = sweepClass();
  GcPage* page = gc_state.sweep.kept != 0 ? gc_state.sweep.kept->next : klass->first;

  if (page == 0 || page->swept == gc_state.cycle) {
    nextClass(klass);
    return 1;
  }

  return sweepAndKeepOrDrop(klass, page);
}

/* Sweeps the page; what is left in it survived, and a page left empty becomes a spare. The answer is the work done. */
static uint64_t sweepAndKeepOrDrop(GcClass* klass, GcPage* page) {
  const uint32_t used = sweepThePage(page);
  gc_state.survivors += (uint64_t)used * page->slotBytes;

  if (used == 0) {
    dropPage(klass, page);
  } else {
    gc_state.sweep.kept = page;
  }

  return page->words + 1;
}

/* Allocation starts again from the first page, so the slots just freed are used before any new page is taken. */
static void nextClass(GcClass* klass) {
  klass->current = klass->first;
  gc_state.sweep.klass++;
  gc_state.sweep.kept = 0;
}

/* In each word of a page's bitmaps, the objects not marked are freed by making the marked bits the allocated ones. The marks are cleared
 * for the next cycle. The answer is how many objects the page still holds. */
static uint32_t sweepThePage(GcPage* page) {
  uint32_t used = 0;

  for (uint32_t bitmapWord = 0; bitmapWord < page->words; bitmapWord++) {
    used += sweepABitmapWord(page, bitmapWord);
  }

  page->used = used;
  page->cursor = 0;
  page->swept = gc_state.cycle;

  return used;
}

/* The 64 slots of one bitmap word: `allocated = marked` frees every slot that was not marked, and `marked = 0` makes the rest white for the
 * next cycle. The answer is how many of the 64 still hold an object. */
static uint32_t sweepABitmapWord(GcPage* page, uint32_t bitmapWord) {
  const uint64_t freed = (uint64_t)__builtin_popcountll(page->allocated[bitmapWord] & ~page->marked[bitmapWord]);
  countReclaimed(freed, freed * page->slotBytes);
  page->allocated[bitmapWord] = page->marked[bitmapWord];
  page->marked[bitmapWord] = 0;

  return (uint32_t)__builtin_popcountll(page->allocated[bitmapWord]);
}

/* The sweep gave `objects` taking `bytes` back: they leave the heap's totals and join the report's. */
static void countReclaimed(uint64_t objects, uint64_t bytes) {
  GcCollector* gc = &gc_state;
  gc->reclaimedObjects += objects;
  gc->reclaimedBytes += bytes;
  gc->objects -= objects;
  gc->bytes -= bytes;
}

static void keepAsSpare(GcPage* page);

/* An empty page leaves its class (the class's current and last page move back to the page before it when it was one of them). */
static void dropPage(GcClass* klass, GcPage* page) {
  GcPage* before = gc_state.sweep.kept;

  if (before != 0) {
    before->next = page->next;
  } else {
    klass->first = page->next;
  }

  if (klass->last == page) {
    klass->last = before;
  }

  if (klass->current == page) {
    klass->current = before;
  }

  keepAsSpare(page);
}

/* An empty page has every allocated bit clear, so no word can be taken for an object in it while it waits. It stays in the page set. */
static void keepAsSpare(GcPage* page) {
  page->next = gc_state.spares;
  gc_state.spares = page;
  gc_state.spareCount++;
}

static void releaseLarge(uintptr_t object);

static void keepLarge(GcHeader* header);

static void reclaimLarge(GcHeader* header);

/* The next large object, from the newest to the oldest: unmarked, it is freed; marked, it is unmarked for the next cycle. Objects made since
 * the sweep began are newer than where it started, so it never meets them (they are made unmarked: see newLarge). */
static uint64_t sweepALarge(void) {
  GcHeader* header = gc_state.sweep.large;
  gc_state.sweep.large = header->older;

  if (header->marked) {
    keepLarge(header);
  } else {
    reclaimLarge(header);
  }

  return 1;
}

/* A marked large object survives, white again for the next cycle. */
static void keepLarge(GcHeader* header) {
  header->marked = 0;
  gc_state.survivors += sizeof(GcHeader) + header->bytes;
}

/* An unmarked one is garbage: counted for the report, then given back. */
static void reclaimLarge(GcHeader* header) {
  gc_state.reclaimedObjects++;
  gc_state.reclaimedBytes += sizeof(GcHeader) + header->bytes;
  releaseLarge((uintptr_t)(header + 1));
}

static void giveBackTheSpares(void);

/* What the sweep kept survived the cycle; the next threshold and the ceiling follow from it. */
static void finishSweeping(void) {
  giveBackTheSpares();
  setTheThreshold(gc_state.survivors);
  checkTheCeiling(gc_state.survivors);
  gc_state.phase = GC_IDLE;
}

/* ── 3. the policy: when the next cycle comes, what is given back, and when the program must stop ── */

/* Twice what survived (and at least the minimum), never past the ceiling; the stress modes keep it at 0, so every safe point collects. */
static void setTheThreshold(uint64_t survivors) {
  GcCollector* gc = &gc_state;
  uint64_t next = 2 * survivors > GC_MINIMUM_THRESHOLD ? 2 * survivors : GC_MINIMUM_THRESHOLD;

  if (next > gc->ceiling) {
    next = gc->ceiling;
  }

  gc->threshold = gc->stress || gc->verify ? 0 : next;
}

/* Between cycles the next work is the next cycle, at the threshold. During one, it is the next slice. That comes once the program has
 * allocated the bytes that earn it (a budget's worth of words, at GC_PACE_WORDS_PER_BYTE). It comes at the next safe point instead when the
 * program is outrunning the collector, or in the incremental stress mode. Outrunning means the heap is half as big again as when the cycle
 * began. */
static void scheduleTheNextWork(void) {
  GcCollector* gc = &gc_state;
  const int behind = gc->bytes > gc->cycleStart + gc->cycleStart / 2;

  if (gc->phase == GC_IDLE) {
    gc->collectAt = gc->threshold;
  } else if (gc->verify || behind) {
    gc->collectAt = 0;
  } else {
    gc->collectAt = gc->bytes + gc->slice / GC_PACE_WORDS_PER_BYTE;
  }
}

static void setRemove(GcSet* set, uintptr_t address);

static void freeASpare(void);

static GcPage* popSpare(void);

/* A spare that sat unused through the whole round since the last collection is not needed at this rate of allocation. It goes back to the
 * C library. Their number is `spareLow`, the fewest spares there were at any time in the round. A program that makes garbage at a steady
 * rate reuses the same pages round after round. One whose data shrinks gives its memory back one round later. */
static void giveBackTheSpares(void) {
  GcCollector* gc = &gc_state;

  for (uint64_t idle = gc->spareLow; idle > 0 && gc->spares != 0; idle--) {
    freeASpare();
  }

  gc->spareLow = gc->spareCount;
}

/* The first spare leaves the page set and goes back to the C library. */
static void freeASpare(void) {
  GcPage* page = popSpare();
  setRemove(&gc_state.pages, (uintptr_t)page);
  free(page);
}

/* Takes the first spare off the spare list. */
static GcPage* popSpare(void) {
  GcPage* spare = gc_state.spares;
  gc_state.spares = spare->next;
  gc_state.spareCount--;

  return spare;
}

static void stopTheProgram(const char* reason);

/* More bytes still reachable than the ceiling allows: the program really does hold that much, so collecting again cannot help. It stops with
 * a clear message instead of growing until the operating system ends it. */
static void checkTheCeiling(uint64_t survivors) {
  if (survivors <= gc_state.ceiling) {
    return;
  }

  char message[160];
  snprintf(message, sizeof message, "out of memory: %llu bytes are still reachable (limit %llu)\n", (unsigned long long)survivors,
           (unsigned long long)gc_state.ceiling);
  stopTheProgram(message);
}

/* What was printed so far is flushed and the reason goes to standard error, then the program stops (as the compiled code's traps do). */
static void stopTheProgram(const char* reason) {
  fflush(0);
  write(2, reason, strlen(reason));
  __builtin_trap();
}

/* ── the incremental stress mode's check ── */

/* What the check needs to know of an object: its size (0: not an object), its layout and its mark. */
typedef struct GcObject {
  uint64_t bytes;
  uint32_t layout;
  uint32_t marked;
} GcObject;

static GcSet emptySet(uint64_t capacity);

static void setAdd(GcSet* set, uintptr_t address);

static void addTheRoots(GcWorklist* pending);

static uint64_t countTheUnmarked(GcWorklist* pending);

static uint64_t verifyWord(GcSet* seen, GcWorklist* pending, uintptr_t word);

static void addItsWords(GcWorklist* pending, const uintptr_t* words, uint64_t bytes);

static GcObject objectAt(uintptr_t word);

static void reportMissed(uint64_t missed);

/* At the end of an incremental marking, everything the program can reach now must be marked. It was reachable when the cycle began (and the
 * barrier kept it), or it was made during the cycle (black). So a walk of everything reachable now, from the roots, made apart from the
 * marking, must find no unmarked object. One it does find was missed, the sign of a missing barrier, and the program stops before that
 * object could be freed. */
static void verifyTheMarks(void) {
  GcWorklist pending = {0, 0, 0};
  addTheRoots(&pending);
  const uint64_t missed = countTheUnmarked(&pending);
  free(pending.items);

  if (missed > 0) {
    reportMissed(missed);
  }
}

/* The walk's starting points: the same roots markTheRoots marks, put on the walk's own list instead. */
static void addTheRoots(GcWorklist* pending) {
  for (GcFrame* frame = gc_top; frame != 0; frame = frame->caller) {
    for (int64_t i = 0; i < frame->count; i++) {
      push(pending, (uintptr_t)frame->slots[i]);
    }
  }

  for (int64_t i = 0; i < gc_state.globalCount; i++) {
    push(pending, (uintptr_t)*gc_state.globals[i]);
  }
}

/* Walks everything reachable from the words on `pending`, each object once, and counts the objects it finds unmarked. The walk keeps a set
 * of the objects it has seen and does not trust the marks. A black object that a store without a barrier made point at a white one is
 * walked into too. */
static uint64_t countTheUnmarked(GcWorklist* pending) {
  GcSet seen = emptySet(GC_FIRST_CAPACITY);
  uint64_t missed = 0;

  while (pending->count > 0) {
    missed += verifyWord(&seen, pending, pending->items[--pending->count].entry);
  }

  free(seen.entries);

  return missed;
}

/* One word of the walk: when it is an object not seen yet, its words are walked next. The answer is 1 when it is an object left unmarked. */
static uint64_t verifyWord(GcSet* seen, GcWorklist* pending, uintptr_t word) {
  const GcObject object = objectAt(word);

  if (object.bytes == 0 || setHas(seen, word)) {
    return 0;
  }

  setAdd(seen, word);

  if (object.layout == GC_REFERENCES) {
    addItsWords(pending, (const uintptr_t*)word, object.bytes);
  }

  return object.marked ? 0 : 1;
}

/* Every word of an object that could be a reference goes on the walk's list. */
static void addItsWords(GcWorklist* pending, const uintptr_t* words, uint64_t bytes) {
  for (uint64_t i = 0; i < bytes / sizeof(uintptr_t); i++) {
    if (mightBeObject(words[i])) {
      push(pending, words[i]);
    }
  }
}

/* What the collector knows of the object a word starts (bytes 0 when it is none of its objects). */
static GcObject objectAt(uintptr_t word) {
  GcObject none = {0, 0, 0};

  if (!mightBeObject(word)) {
    return none;
  }

  GcPage* page = pageOf(word);
  const int64_t slot = page != 0 ? slotIndex(page, word) : -1;

  if (slot >= 0) {
    return (GcObject){page->slotBytes, page->layout, (uint32_t)isBitSet(page->marked, (uint64_t)slot)};
  }

  if (page != 0 || !isLarge(word)) {
    return none;
  }

  return (GcObject){headerOf(word)->bytes, headerOf(word)->layout, headerOf(word)->marked};
}

static void reportMissed(uint64_t missed) {
  char message[200];
  snprintf(message, sizeof message, "gc verify: %llu reachable objects were left unmarked by the incremental marking (a write barrier is missing)\n",
           (unsigned long long)missed);
  stopTheProgram(message);
}

/* ── the sets of addresses ── */

static void* allocatedOrStop(void* block) {
  if (block == 0) {
    stopTheProgram("out of memory: the system has no more to give\n");
  }

  return block;
}

/* Addresses are 16-byte aligned, so their low four bits carry nothing; the multiply spreads the rest over the set. */
static uint64_t homeOf(const GcSet* set, uintptr_t address) {
  return (((uint64_t)address >> 4) * 0x9E3779B97F4A7C15ull >> 20) & (set->capacity - 1);
}

static uint64_t nextEntry(const GcSet* set, uint64_t at) {
  return (at + 1) & (set->capacity - 1);
}

/* Where `address` is in the set, or where it would go. */
static uint64_t slotOf(const GcSet* set, uintptr_t address) {
  uint64_t at = homeOf(set, address);

  while (set->entries[at] != 0 && set->entries[at] != address) {
    at = nextEntry(set, at);
  }

  return at;
}

static int setHas(const GcSet* set, uintptr_t address) {
  return set->capacity != 0 && set->entries[slotOf(set, address)] == address;
}

static GcSet emptySet(uint64_t capacity) {
  GcSet set = {allocatedOrStop(calloc(capacity, sizeof(uintptr_t))), capacity, 0};

  return set;
}

static void place(GcSet* set, uintptr_t address) {
  set->entries[slotOf(set, address)] = address;
  set->count++;
}

/* Twice the room, every address placed again. */
static void growSet(GcSet* set) {
  GcSet bigger = emptySet(set->capacity * 2);

  for (uint64_t i = 0; i < set->capacity; i++) {
    if (set->entries[i] != 0) {
      place(&bigger, set->entries[i]);
    }
  }

  free(set->entries);
  *set = bigger;
}

static void setAdd(GcSet* set, uintptr_t address) {
  if (set->capacity == 0) {
    *set = emptySet(GC_FIRST_CAPACITY);
  }

  if (4 * (set->count + 1) > 3 * set->capacity) {
    growSet(set);
  }

  place(set, address);
}

static uint64_t closeTheHole(GcSet* set, uint64_t hole);

static int staysPut(const GcSet* set, uint64_t hole, uint64_t at);

/* Takes the address out and moves the ones after it in its cluster back over the hole, so no search stops early (no tombstones). */
static void setRemove(GcSet* set, uintptr_t address) {
  const uint64_t hole = closeTheHole(set, slotOf(set, address));
  set->entries[hole] = 0;
  set->count--;
}

/* Walks the cluster after the hole: each entry that may move fills the hole, and its old place is the hole from then on. The answer is the
 * last hole, the entry to clear. */
static uint64_t closeTheHole(GcSet* set, uint64_t hole) {
  for (uint64_t at = nextEntry(set, hole); set->entries[at] != 0; at = nextEntry(set, at)) {
    if (staysPut(set, hole, at)) {
      continue;
    }

    set->entries[hole] = set->entries[at];
    hole = at;
  }

  return hole;
}

/* The entry at `at` stays when its home lies cyclically after the hole and at or before where it is. A search for it starts past the hole,
 * so it never needs to cross it. Otherwise it must move into the hole, or a search for it would stop at the empty entry. */
static int staysPut(const GcSet* set, uint64_t hole, uint64_t at) {
  const uint64_t home = homeOf(set, set->entries[at]);

  return hole <= at ? (home > hole && home <= at) : (home > hole || home <= at);
}

/* The range test of mightBeObject covers [first, last]. It only ever widens: it is a quick no, not an exact answer. */
static void noteRange(uintptr_t first, uintptr_t last) {
  GcCollector* gc = &gc_state;

  if (gc->lowest == 0 || first < gc->lowest) {
    gc->lowest = first;
  }

  if (last > gc->highest) {
    gc->highest = last;
  }
}

/* ── pages ── */

static GcPage* takeASpare(void);

static GcPage* newPage(void);

static void setUpPage(GcPage* page, const GcClass* klass);

/* A spare if there is one, else a new page from the C library (aligned to its size, so its addresses find it by masking). */
static GcPage* takePage(void) {
  if (gc_state.spares != 0) {
    return takeASpare();
  }

  return newPage();
}

/* A spare taken for use; `spareLow` remembers the fewest there were, for giveBackTheSpares. */
static GcPage* takeASpare(void) {
  GcPage* spare = popSpare();

  if (gc_state.spareCount < gc_state.spareLow) {
    gc_state.spareLow = gc_state.spareCount;
  }

  return spare;
}

/* 64 KiB from the C library, aligned to 64 KiB, and entered in the page set. */
static GcPage* newPage(void) {
  void* memory = 0;

  if (posix_memalign(&memory, GC_PAGE_BYTES, GC_PAGE_BYTES) != 0) {
    stopTheProgram("out of memory: the system has no more to give\n");
  }

  setAdd(&gc_state.pages, (uintptr_t)memory);
  noteRange((uintptr_t)memory, (uintptr_t)memory + GC_PAGE_BYTES - 1);

  return memory;
}

/* A page for the class, all its slots free, at the end of the class's list. */
static GcPage* appendPage(GcClass* klass) {
  GcPage* page = takePage();
  setUpPage(page, klass);

  if (klass->last != 0) {
    klass->last->next = page;
  } else {
    klass->first = page;
  }

  klass->last = page;

  return page;
}

/* A cleared header cut for the class's slot size and layout. It has nothing to sweep: it counts as swept for this cycle. */
static void setUpPage(GcPage* page, const GcClass* klass) {
  memset(page, 0, sizeof(GcPage));
  page->slotBytes = klass->slotBytes;
  page->slotCount = (uint32_t)((GC_PAGE_BYTES - GC_FIRST_SLOT) / klass->slotBytes);
  page->words = (page->slotCount + 63) / 64;

  page->layout = klass->layout;
  page->swept = gc_state.cycle;
}

static void* slotAddress(const GcPage* page, uint64_t slot) {
  return (char*)page + GC_FIRST_SLOT + slot * page->slotBytes;
}

/* What colour a new object takes. While a cycle marks, it is black: marked, and nothing to look at (it holds nothing from before the cycle).
 * While the sweep has not reached its page, it is marked too, so the sweep keeps it. Otherwise it is white, for the next cycle to decide. */
static int bornMarked(const GcPage* page) {
  return gc_marking || page->swept != gc_state.cycle;
}

static void colourSlot(GcPage* page, uint64_t slot) {
  if (bornMarked(page)) {
    setBit(page->marked, slot);
  }
}

static int64_t firstFreeSlot(GcPage* page);

static int64_t freeSlotAtCursor(const GcPage* page, uint64_t open);

static void* claimSlot(GcPage* page, uint64_t slot);

/* The first free slot from the cursor on, now allocated. 0 when the page is full (its cursor then at the end, so the next look is quick). */
static void* takeSlot(GcPage* page) {
  const int64_t slot = firstFreeSlot(page);

  if (slot >= 0) {
    return claimSlot(page, (uint64_t)slot);
  }

  page->cursor = page->words;

  return 0;
}

/* Moves the cursor to the first bitmap word with a clear bit, and answers that bit's slot; -1 when there is none. */
static int64_t firstFreeSlot(GcPage* page) {
  for (; page->cursor < page->words; page->cursor++) {
    const uint64_t open = ~page->allocated[page->cursor];

    if (open != 0) {
      return freeSlotAtCursor(page, open);
    }
  }

  return -1;
}

/* The slot of the lowest set bit of `open` (the cursor word's clear bits). The last word can have bits past the page's last slot: such a
 * bit is no slot, so the page is full (-1). */
static int64_t freeSlotAtCursor(const GcPage* page, uint64_t open) {
  const uint64_t slot = (uint64_t)page->cursor * 64 + (uint64_t)__builtin_ctzll(open);

  return slot < page->slotCount ? (int64_t)slot : -1;
}

/* The slot now holds an object, coloured as bornMarked says. */
static void* claimSlot(GcPage* page, uint64_t slot) {
  setBit(page->allocated, slot);
  page->used++;
  colourSlot(page, slot);

  return slotAddress(page, slot);
}

/* The class's next page with a free slot after the current one, or a new page when every page is full. */
static GcPage* pageWithRoom(GcClass* klass) {
  GcPage* page = klass->current != 0 ? klass->current->next : klass->first;

  while (page != 0 && page->used == page->slotCount) {
    page = page->next;
  }

  if (page == 0) {
    page = appendPage(klass);
  }

  klass->current = page;

  return page;
}

/* A slot goes back: its allocated and marked bits are cleared (what it held no longer survives). The cursor moves back to it, so the next
 * allocation of the class may take it again. */
static void releaseSlot(GcPage* page, int64_t slot) {
  GcCollector* gc = &gc_state;
  page->allocated[slot / 64] &= ~((uint64_t)1 << (slot % 64));
  page->marked[slot / 64] &= ~((uint64_t)1 << (slot % 64));
  page->used--;

  if ((uint32_t)(slot / 64) < page->cursor) {
    page->cursor = (uint32_t)(slot / 64);
  }

  gc->bytes -= page->slotBytes;
  gc->objects--;
}

/* ── objects ── */

static GcClass* classFor(uint64_t bytes, int64_t layout);

static void countNewObject(uint64_t bytes);

static GcHeader* newLargeBlock(uint64_t bytes, int64_t layout);

static void addToTheLargeSet(uintptr_t object, uint64_t bytes);

/* An object of the size class `bytes` rounds up to, from the class's pages. */
static void* newSmall(uint64_t bytes, int64_t layout) {
  GcClass* klass = classFor(bytes, layout);
  void* object = klass->current != 0 ? takeSlot(klass->current) : 0;

  while (object == 0) {
    object = takeSlot(pageWithRoom(klass));
  }

  if (layout == GC_REFERENCES) {
    memset(object, 0, klass->slotBytes);
  }

  countNewObject(klass->slotBytes);

  return object;
}

/* The pages for a request of `bytes`: by layout, then the size class it rounds up to. */
static GcClass* classFor(uint64_t bytes, int64_t layout) {
  return &gc_state.classes[layout == GC_REFERENCES][gc_state.classOf[(bytes + 15) / 16]];
}

/* A new object of `bytes` joins the heap's totals. */
static void countNewObject(uint64_t bytes) {
  gc_state.bytes += bytes;
  gc_state.objects++;
}

/* The large list, newest first: a new object goes in front, and one that goes is linked around. */
static void linkLarge(GcHeader* header) {
  GcCollector* gc = &gc_state;
  header->newer = 0;
  header->older = gc->newestLarge;

  if (gc->newestLarge != 0) {
    gc->newestLarge->newer = header;
  }

  gc->newestLarge = header;
}

static void unlinkLarge(GcHeader* header) {
  GcCollector* gc = &gc_state;

  if (header->newer != 0) {
    header->newer->older = header->older;
  } else {
    gc->newestLarge = header->older;
  }

  if (header->older != 0) {
    header->older->newer = header->newer;
  }

  if (gc->sweep.large == header) {
    gc->sweep.large = header->older;
  }
}

/* A large object: a malloc block with a header in front (calloc for a GC_REFERENCES one, which must start zeroed). Its address goes in the
 * large set, and it becomes the newest of the large list. It is born black while a cycle marks. During a sweep it is born unmarked, as the
 * sweep never reaches the objects newer than where it began. */
static void* newLarge(uint64_t bytes, int64_t layout) {
  GcHeader* header = newLargeBlock(bytes, layout);
  linkLarge(header);
  addToTheLargeSet((uintptr_t)(header + 1), bytes);
  countNewObject(sizeof(GcHeader) + bytes);

  return header + 1;
}

/* The malloc block, header filled in. */
static GcHeader* newLargeBlock(uint64_t bytes, int64_t layout) {
  GcHeader* header = allocatedOrStop(layout == GC_REFERENCES ? calloc(1, sizeof(GcHeader) + bytes) : malloc(sizeof(GcHeader) + bytes));
  header->bytes = bytes;
  header->layout = (uint32_t)layout;
  header->marked = (uint32_t)gc_marking;

  return header;
}

/* The large set holds the object's address, and the quick range test covers its words. */
static void addToTheLargeSet(uintptr_t object, uint64_t bytes) {
  setAdd(&gc_state.large, object);
  noteRange(object, object + bytes);
}

/* An object of `bytes` (at least 1), zeroed when its layout is GC_REFERENCES. */
static void* newObject(uint64_t bytes, int64_t layout) {
  return bytes <= GC_LARGEST_SMALL ? newSmall(bytes, layout) : newLarge(bytes, layout);
}

/* A large object leaves the large set and the large list, and goes back to the C library. */
static void releaseLarge(uintptr_t object) {
  GcCollector* gc = &gc_state;
  GcHeader* header = headerOf(object);
  setRemove(&gc->large, object);
  unlinkLarge(header);

  gc->bytes -= sizeof(GcHeader) + header->bytes;
  gc->objects--;
  free(header);
}

/* ── allocating (the program calls these) ── */

/* A block of `bytes` whose words are laid out as `layout` says. */
void* gc_allocate(int64_t bytes, int64_t layout) {
  const size_t size = bytes > 0 ? (size_t)bytes : 1;

  if (!gc_state.running) {
    return allocatedOrStop(malloc(size));
  }

  return newObject(size, layout);
}

/* `count` zeroed elements of `size` bytes each. */
void* gc_allocate_zeroed(int64_t count, int64_t size, int64_t layout) {
  const size_t bytes = count > 0 && size > 0 ? (size_t)(count * size) : 1;

  if (!gc_state.running) {
    return allocatedOrStop(calloc(1, bytes));
  }

  void* object = newObject(bytes, layout);

  if (layout != GC_REFERENCES) {
    memset(object, 0, bytes);
  }

  return object;
}

/* While marking runs, an object the collector moves or gives back on the program's behalf has its words shaded first. The program keeps
 * using them (copied into the new block, which is black), and the barrier never saw them go. */
static void shadeBeforeLeaving(uintptr_t object, uint64_t bytes, uint32_t layout) {
  if (!gc_marking) {
    return;
  }

  gc_state.releasedWhileMarking = 1;

  if (layout == GC_REFERENCES) {
    shadeWords((const void*)object, bytes);
  }
}

/* A slot is resized in place while the new size still fits it. Otherwise the object moves to a new one (of its layout), and the slot is
 * freed, as realloc frees the old block. */
static void* resizeSmall(GcPage* page, int64_t slot, void* block, uint64_t bytes) {
  if (bytes <= page->slotBytes) {
    return block;
  }

  shadeBeforeLeaving((uintptr_t)block, page->slotBytes, page->layout);
  void* moved = newObject(bytes, page->layout);
  memcpy(moved, block, page->slotBytes);
  releaseSlot(page, slot);

  return moved;
}

static void* reallocLarge(uintptr_t object, uint64_t bytes);

static GcHeader* reallocLargeBlock(uintptr_t object, uint64_t bytes);

static void updateTheMovedHeader(GcHeader* header, GcHeader old, uint64_t bytes);

/* A large object that stays large is realloc'd under its header; one that shrinks to a small size moves to a slot. */
static void* resizeLarge(uintptr_t object, uint64_t bytes) {
  const GcHeader* header = headerOf(object);
  shadeBeforeLeaving(object, header->bytes, header->layout);

  if (bytes > GC_LARGEST_SMALL) {
    return reallocLarge(object, bytes);
  }

  void* moved = newObject(bytes, header->layout);
  memcpy(moved, (void*)object, bytes);
  releaseLarge(object);

  return moved;
}

/* The header moved (realloc copied it, links and mark included). Its neighbours in the large list, and the sweep's cursor when it was there,
 * now point at the new place. The object keeps its place in the list, so a sweep under way still meets it exactly once. */
static void relinkLarge(GcHeader* header, uintptr_t old) {
  GcCollector* gc = &gc_state;

  if (header->newer != 0) {
    header->newer->older = header;
  } else {
    gc->newestLarge = header;
  }

  if (header->older != 0) {
    header->older->newer = header;
  }

  if ((uintptr_t)gc->sweep.large == old) {
    gc->sweep.large = header;
  }
}

/* realloc may move the block: it leaves the set under its old address and comes back under the new one. The words it gains are zeroed when
 * it may hold references. While marking runs, it is black from now on (its words were shaded). */
static void* reallocLarge(uintptr_t object, uint64_t bytes) {
  const GcHeader old = *headerOf(object);
  setRemove(&gc_state.large, object);

  GcHeader* header = reallocLargeBlock(object, bytes);
  updateTheMovedHeader(header, old, bytes);
  addToTheLargeSet((uintptr_t)(header + 1), bytes);
  gc_state.bytes += bytes - old.bytes;

  return header + 1;
}

/* realloc, and the large list relinked to wherever the block now is. */
static GcHeader* reallocLargeBlock(uintptr_t object, uint64_t bytes) {
  GcHeader* header = allocatedOrStop(realloc(headerOf(object), sizeof(GcHeader) + bytes));
  relinkLarge(header, (uintptr_t)headerOf(object));

  return header;
}

/* The header after realloc: the new size, the words it gained zeroed when it may hold references, and black while marking runs. */
static void updateTheMovedHeader(GcHeader* header, GcHeader old, uint64_t bytes) {
  if (old.layout == GC_REFERENCES && bytes > old.bytes) {
    memset((char*)(header + 1) + old.bytes, 0, bytes - old.bytes);
  }

  header->bytes = bytes;
  header->marked = gc_marking ? 1 : old.marked;
}

/* A list's elements when it grows: like realloc, and the object keeps its layout. A block the collector did not make (host memory, or made
 * before gc_start) is realloc'd. */
void* gc_resize(void* block, int64_t bytes) {
  const size_t size = bytes > 0 ? (size_t)bytes : 1;
  const uintptr_t object = (uintptr_t)block;

  if (!gc_state.running) {
    return allocatedOrStop(realloc(block, size));
  }

  GcPage* page = pageOfObject(object);

  if (page != 0) {
    return resizeSmall(page, slotIndex(page, object), block, size);
  }

  if (isLargeObject(object)) {
    return resizeLarge(object, size);
  }

  return allocatedOrStop(realloc(block, size));
}

static void giveBackSmall(GcPage* page, uintptr_t object);

static void giveBackLarge(uintptr_t object);

/* A block the compiled code is done with and knows nothing else refers to (a map's old tables after it grew): freed at once. Its words were
 * copied into the new tables, so while marking runs they are shaded first. */
void gc_release(void* block) {
  const uintptr_t object = (uintptr_t)block;
  GcPage* page = gc_state.running ? pageOfObject(object) : 0;

  if (page != 0) {
    giveBackSmall(page, object);
  } else if (gc_state.running && isLargeObject(object)) {
    giveBackLarge(object);
  } else {
    free(block);
  }
}

static void giveBackSmall(GcPage* page, uintptr_t object) {
  shadeBeforeLeaving(object, page->slotBytes, page->layout);
  releaseSlot(page, slotIndex(page, object));
}

static void giveBackLarge(uintptr_t object) {
  shadeBeforeLeaving(object, headerOf(object)->bytes, headerOf(object)->layout);
  releaseLarge(object);
}

/* The program's own `Free`. Once the collector runs it is only a hint: the object goes at the first collection that finds it unreachable. So
 * a program that frees something it still uses cannot corrupt memory. In a program that never started the collector, it is a real free(). */
void gc_free_hint(void* block) {
  if (!gc_state.running) {
    free(block);
  }
}

static void makeRoom(GcWorklist* list) {
  if (list->count < list->capacity) {
    return;
  }

  list->capacity = list->capacity == 0 ? 256 : list->capacity * 2;
  list->items = allocatedOrStop(realloc(list->items, list->capacity * sizeof(GcGrey)));
}

/* A grey object, to be looked at from its word `next` on. */
static void pushFrom(GcWorklist* list, uintptr_t entry, uint64_t next) {
  makeRoom(list);
  list->items[list->count++] = (GcGrey){entry, next};
}

static void push(GcWorklist* list, uintptr_t entry) {
  pushFrom(list, entry, 0);
}

/* ── the shadow stack (the program calls these) ── */

/* On entry to a function that keeps references: its record, empty and linked. */
void gc_push_frame(GcFrame* frame, int64_t count) {
  frame->count = count;
  memset(frame->slots, 0, (size_t)count * sizeof(void*));
  frame->caller = gc_top;
  gc_top = frame;
}

/* On every way out of it. */
void gc_pop_frame(GcFrame* frame) {
  gc_top = frame->caller;
}

static void shadeObject(uintptr_t object);

/* A coroutine's record (filled in once when the coroutine is made) goes in when it resumes and out when it suspends. `coroutineFrame` is the
 * coroutine's frame, the object the record lives in. A resumed coroutine writes its frame with no barrier, as a function writes its stack.
 * The frame holds its record's slots and what LLVM keeps there across a suspend. So when it resumes while marking runs, every word of its
 * frame is shaded first. That is what the barrier would have done for each of those stores. */
void gc_link_frame(GcFrame* frame, void* coroutineFrame) {
  if (gc_marking) {
    shadeObject((uintptr_t)coroutineFrame);
  }

  frame->caller = gc_top;
  gc_top = frame;
}

void gc_unlink_frame(GcFrame* frame) {
  gc_top = frame->caller;
}

/* Every word of an object the collector made (nothing for anything else). */
static void shadeObject(uintptr_t object) {
  if (!gc_state.running || !mightBeObject(object)) {
    return;
  }

  GcPage* page = pageOf(object);

  if (page != 0 && slotIndex(page, object) >= 0) {
    shadeWords((const void*)object, page->slotBytes);
  } else if (page == 0 && isLarge(object)) {
    shadeWords((const void*)object, headerOf(object)->bytes);
  }
}

/* ── the pauses ── */

/* The time in nanoseconds (timespec_get is plain C11, the same on every system the collector's IR runs on). */
static uint64_t nowInNanoseconds(void) {
  struct timespec now;
  timespec_get(&now, TIME_UTC);

  return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

/* The bucket of a pause: 1 µs wide below 1 ms, 100 µs wide below 100 ms, then the last. */
static uint32_t bucketOf(uint64_t nanoseconds) {
  const uint64_t micros = nanoseconds / 1000;

  if (micros < 1000) {
    return (uint32_t)micros;
  }

  if (micros < 100000) {
    return (uint32_t)(1000 + (micros - 1000) / 100);
  }

  return GC_BUCKETS - 1;
}

/* The longest pause a bucket stands for, in nanoseconds. */
static uint64_t bucketTop(uint32_t bucket) {
  if (bucket < 1000) {
    return ((uint64_t)bucket + 1) * 1000;
  }

  return (1000 + ((uint64_t)bucket - 1000 + 1) * 100) * 1000;
}

static void countThePause(uint64_t nanoseconds) {
  GcPauses* pauses = &gc_state.pauses;
  pauses->count++;
  pauses->total += nanoseconds;

  if (nanoseconds > pauses->longest) {
    pauses->longest = nanoseconds;
  }

  pauses->buckets[bucketOf(nanoseconds)]++;
}

static int64_t bucketReaching(const GcPauses* pauses, uint64_t wanted);

/* The pause that `share` (out of 100) of all pauses are no longer than. It is the top of the bucket it falls in, never more than the longest. */
static uint64_t percentile(uint64_t share) {
  const GcPauses* pauses = &gc_state.pauses;
  const int64_t bucket = bucketReaching(pauses, (pauses->count * share + 99) / 100);

  if (bucket < 0) {
    return pauses->longest;
  }

  const uint64_t top = bucketTop((uint32_t)bucket);

  return top < pauses->longest ? top : pauses->longest;
}

/* The first bucket by which `wanted` pauses (and at least one) have been counted, from the shortest up; -1 when there is none. */
static int64_t bucketReaching(const GcPauses* pauses, uint64_t wanted) {
  uint64_t seen = 0;

  for (uint32_t bucket = 0; bucket < GC_BUCKETS; bucket++) {
    seen += pauses->buckets[bucket];

    if (seen >= wanted && seen > 0) {
      return bucket;
    }
  }

  return -1;
}

/* ── starting (the program's entry point calls this) ── */

static void printTheReport(void);

static void setUpTheClassesOf(int layout);

static void fillTheClassTable(void);

static uint64_t sliceSetting(int verify);

static void scheduleTheFirstCycle(void);

static void describeTheHeap(char* out, size_t size);

static void describeThePauses(char* out, size_t size);

static uint64_t numberFrom(const char* name, uint64_t otherwise) {
  const char* text = getenv(name);

  return text != 0 && *text != 0 ? strtoull(text, 0, 10) : otherwise;
}

static int settingIs(const char* name, const char* value) {
  const char* text = getenv(name);

  return text != 0 && strcmp(text, value) == 0;
}

/* The size classes, for each layout, and which class a request of up to 16 * k bytes takes. */
static void setUpTheClasses(void) {
  for (int layout = 0; layout < 2; layout++) {
    setUpTheClassesOf(layout);
  }

  fillTheClassTable();
}

static void setUpTheClassesOf(int layout) {
  for (int sizeClass = 0; sizeClass < GC_CLASS_COUNT; sizeClass++) {
    gc_state.classes[layout][sizeClass].slotBytes = kClassBytes[sizeClass];
    gc_state.classes[layout][sizeClass].layout = (uint32_t)layout;
  }
}

/* `classOf[k]` is the first size class a request of 16 * k bytes fits in, so an allocation finds its class with one look-up. */
static void fillTheClassTable(void) {
  int sizeClass = 0;

  for (uint32_t k = 0; k <= GC_LARGEST_SMALL / 16; k++) {
    while (kClassBytes[sizeClass] < 16 * k) {
      sizeClass++;
    }

    gc_state.classOf[k] = (uint8_t)sizeClass;
  }
}

/* The switches of the environment. The incremental stress mode is incremental with the smallest slice and the marks checked. */
static void readTheSwitches(void) {
  GcCollector* gc = &gc_state;
  gc->verify = settingIs("POLYIR_GC_STRESS", "incremental");
  gc->stress = !gc->verify && numberFrom("POLYIR_GC_STRESS", 0) != 0;
  gc->incremental = gc->verify || !settingIs("POLYIR_GC_MODE", "stop");

  gc->slice = sliceSetting(gc->verify);
  gc->report = numberFrom("POLYIR_GC_REPORT", 0) != 0;
  gc->ceiling = numberFrom("POLYIR_GC_CEILING", GC_DEFAULT_CEILING);
}

/* POLYIR_GC_SLICE, or one word when the marks are checked; never 0, so every slice makes progress. */
static uint64_t sliceSetting(int verify) {
  const uint64_t slice = verify ? 1 : numberFrom("POLYIR_GC_SLICE", GC_DEFAULT_SLICE);

  return slice == 0 ? 1 : slice;
}

/* `globals` are the addresses of the program's globals that hold references. */
void gc_start(void*** globals, int64_t globalCount) {
  gc_state.globals = globals;
  gc_state.globalCount = globalCount;

  readTheSwitches();
  setUpTheClasses();
  scheduleTheFirstCycle();
  gc_state.running = 1;

  if (gc_state.report) {
    atexit(printTheReport);
  }
}

/* The first cycle begins at the minimum threshold (at once in the stress modes). */
static void scheduleTheFirstCycle(void) {
  setTheThreshold(0);
  gc_state.collectAt = gc_state.threshold;
}

/* Nanoseconds as microseconds with one decimal, for the report. */
static void formatMicros(char* out, size_t size, uint64_t nanoseconds) {
  snprintf(out, size, "%llu.%llu", (unsigned long long)(nanoseconds / 1000), (unsigned long long)(nanoseconds % 1000 / 100));
}

/* POLYIR_GC_REPORT=1: the collections, what they reclaimed and the most the objects held at once, then the pauses, on standard error at the
 * end. */
static void printTheReport(void) {
  char heap[300], pauses[200], line[460];
  noteThePeak();
  describeTheHeap(heap, sizeof heap);
  describeThePauses(pauses, sizeof pauses);

  snprintf(line, sizeof line, "gc: %s; %s, %s\n", heap, pauses, gc_state.incremental && !gc_state.stress ? "incremental" : "stop-the-world");
  write(2, line, strlen(line));
}

static void describeTheHeap(char* out, size_t size) {
  const GcCollector* gc = &gc_state;
  snprintf(out, size, "%llu collections, %llu objects (%llu bytes) reclaimed, peak %llu bytes, %llu bytes in %llu objects at the end (%llu pages, %llu large)",
           (unsigned long long)gc->collections, (unsigned long long)gc->reclaimedObjects, (unsigned long long)gc->reclaimedBytes,
           (unsigned long long)gc->peakBytes, (unsigned long long)gc->bytes, (unsigned long long)gc->objects,
           (unsigned long long)gc->pages.count, (unsigned long long)gc->large.count);
}

static void describeThePauses(char* out, size_t size) {
  const GcPauses* pauses = &gc_state.pauses;
  char longest[32], median[32], high[32], total[32];

  formatMicros(longest, sizeof longest, pauses->longest);
  formatMicros(median, sizeof median, percentile(50));
  formatMicros(high, sizeof high, percentile(99));
  formatMicros(total, sizeof total, pauses->total);

  snprintf(out, size, "%llu pauses, longest %s us, p50 %s us, p99 %s us, %s us in all", (unsigned long long)pauses->count, longest, median, high, total);
}
