# Safety checks for arrays and maps: a proposal

**Status: the container and divisor checks of section 3 are implemented (build steps 0 to 3, `--unchecked`); the rest of 3.5, `@unchecked`, `getUnchecked`, `m.get`/`getOr` and the Java/wasm parts are not.** The first section is measured (each row is a small program compiled and run);
the rest is a design to decide on before building. It follows from two things in `java-mapping.md`: the Java back end needs a
defined answer for "index out of range" (T8, T10), and `optional<T>` (2.11) only helps if the unchecked reads are removed.

## 1. What happened before the checks

(Measured before they were added. Now an index outside a list, array, buffer, grid or tensor, and a zero divisor, trap: code `-1` or `-4` inside a `try`, otherwise SIGTRAP; `--unchecked` restores the old behaviour. A 20M-access benchmark showed no measurable difference with the checks on.)

| Program | Result |
|---|---|
| `xs[100000]` on a 1-element `mutarray` | no error; returns whatever is in memory |
| `xs[0 - 1]` | no error; reads the word before the data |
| `xs[100000000] = 1` | writes out of bounds; segfault here, silent corruption if the address is mapped |
| `xs[0]` on an empty `mutarray` | segfault (null data pointer) |
| `a[9]` on `i64[4]` | no error; reads the stack past the array |
| `g[5, 5]` on `grid<i64,2,2>` | no error |
| `a[7]` on an immutable `array<i64>` | no error |
| `buffer.push` past capacity | does not build (`buffer` push is not lowered yet); documented as "never grows" |
| **`mutmap` with capacity 2, four inserts** (fixed 2026-10-16, see 3.4) | **silent corruption**: `m[1]` reads `44` (key 4's value was written into key 1's slot), `m[3]` and `m[4]` read `0`, `len()` is 2 |
| `m[k]` for a missing key | the value type's zero (documented); `m.has(k)` tells absent from zero |
| `5 / 0`, `1 << 70` | LLVM undefined behaviour: `5 / 0` returned `0` and `1 << 70` returned `64` on this machine (arm64) |

Two of these are bugs rather than missing checks, and should be fixed whatever else is decided: **a full `mutmap` corrupts
existing entries**, and **a write past the end of a list is undefined**. The rest are gaps in a design that has so far left misuse
"to break honestly" (`README`: exceptions section).

## 2. Goals and non-goals

- **Memory safety for `array`, `mutarray`, `buffer`, `T[N]`, `grid`, `tensor`, `map`, `mutmap`, `set`, `mutset`:** a polyir program
  that does not use `ptr<T>` or `extern` cannot read or write outside a container, whatever its inputs.
- **One defined behaviour that every target can match** (LLVM, wasm, Java, JS), not LLVM's.
- **Cheap on the hot path**, and removable where a programmer has proved it unnecessary.
- **Non-goals:** checking `ptr<T>` (it is the unsafe escape hatch and stays unchecked); GPU kernels (a thread cannot trap; they keep
  today's rules, see 3.6); recovering from every failure (3.1).

## 3. The proposal

### 3.1 A trap is a `throw` with a reserved code

A failed check calls one runtime function, `polyir_trap(kind, line, col)` (emitted on demand like `ensureFree`):

- if the program is inside a `try`, it does `throw` with a **reserved negative code** (`-1` index out of range, `-2` key problem, `-3`
  empty/unwrap, `-4` division by zero, `-5` capacity), so existing `catch (code)` code works and no new mechanism is needed;
- otherwise it prints one line to stderr and exits with status **134** (the abort convention):
  `polyir: index 7 out of range for length 3 (array xs) at Tour.tl:12:9`;
- the position comes from the **source map** (`--debug`/`--source-map`): a trap in a front end's program names the front end's file
  and line, which is the debug story working for free. Without debug information the message has the polyir line.

Why not "always fatal"? A server-style program wants to catch an out-of-range index from user input; `throw` already exists and
maps to Java exceptions (T12). Why not "always catchable"? A program with no `try` should not pay for `setjmp` to learn it has a bug.

### 3.2 Lists and arrays (`array`, `mutarray`, `buffer`)

- `xs[i]` read and write: one **unsigned** compare, `i <u length`. That rejects negatives and large values with a single branch, and
  also covers the empty list (length 0), so the null-data crash disappears.
- `xs[i] = v` on an immutable `array`: already a compile error; unchanged.
- `buffer.push` at capacity: trap `-5` (capacity). Today it is "never reallocs" and silently undefined.
- `first`/`last`/`pop`-style reads on an empty list (when they exist): trap `-3`, or return `optional<T>` once that exists.
- `with`, `freeze`, `thaw`, `copy`: no new checks (they allocate and copy).

### 3.3 Fixed arrays, grids and tensors

- `a[i]` on `T[N]`: check `i <u N`. When `i` is a constant, check **at compile time** and report an error with the line and column
  (`xs[0 - 1]`, `a[9]` on `i64[4]`); no runtime cost.
- `g[r, c]` and `t[i, j, k]`: each index checked against its own dimension (not only the flattened offset, which would let `g[0, 5]`
  alias `g[1, 3]` on a 2x2 grid). Constant indices again at compile time.
- `sparse` reads of an unset cell stay "zero", as the map rule below; negative indices are accepted (they are keys, not offsets).

### 3.4 Maps and sets

- **A `mutmap` / `mutset` grows.** Today `mutmap<K,V>(capacity)` is fixed and a full table corrupts (section 1). Growing at 3/4 load,
  by reallocating the key/value/state arrays behind the **same header pointer**, keeps every alias valid (a map is a reference) and
  makes the capacity argument an initial hint, like Java's `HashMap`. It also removes the one place the Java back end would be more
  forgiving than the native one (java-mapping T10). Alternative if growth is unwanted: trap `-5` on insert into a full table;
  either is safe, corruption is not.
- **Missing key.** `m[k]` keeps returning the value type's zero: it is documented, and changing it would silently change programs.
  Add `m.get(k)` returning `optional<V>` (and `m.getOr(k, default)` for now), and recommend them in the README over `m[k]` for
  anything that may be absent. A lint warning for `m[k]` where `k` was never `has`-checked is possible but noisy; leave it out.
- **Set/map keys.** No new runtime check: key types are already validated when the program is checked (no NaN keys, structural
  `eq`/`hash`).
- Iteration order stays unspecified (java-mapping T10); a program must not depend on it.

### 3.5 Related checks that use the same trap

These are not about containers but come for free once the trap exists, and the Java mapping needs them defined (T16):

| Operation | Rule |
|---|---|
| integer `/` and `%` by zero | trap `-4` |
| `INT_MIN / -1` | wraps to `INT_MIN` (as Java), no trap |
| shift count `>= width` or negative | masked to the width (as Java), no poison |
| `either.unwrapOk()` on the error case, `variant.unwrapAt(i)` on another case, `optional.unwrap()` on none | trap `-3` (done for `optional.unwrap()` only) |
| a `string` index | needs the string model (java-mapping T2); until then unchecked and documented |

### 3.6 Where checks are off

- **`ptr<T>`, `extern` calls:** unchecked; the programmer asked for it.
- **GPU kernels:** unchanged. A GPU thread cannot throw. Inside a kernel an out-of-range index is clamped or ignored per the kernel
  profile (to be written), and the host side keeps its checks.
- **`--unchecked` (release) and `@unchecked func`:** remove the checks from a build or a function. Default is **on in every build**,
  debug and release: a program that is safe in testing should not become unsafe in production.
- **`xs.getUnchecked(i)` / `setUnchecked`:** a per-access escape for an inner loop that the optimizer cannot prove.

### 3.7 Cost

One compare and one never-taken branch per access. In `for (i = 0; i < len(xs); i = i + 1)` loops LLVM removes the check because the
loop condition proves it; elsewhere the branch is well predicted. This is an estimate, not a measurement: step 1 below includes a
micro-benchmark (sum over a 10M-element list, checked vs `--unchecked`) before the default is final.

### 3.8 Other targets

- **Java:** emit nothing: `ArrayList.get`, arrays and `List.copyOf` already throw `IndexOutOfBoundsException`. The back end converts
  at the `catch` boundary: a polyir `catch (code)` receives `-1` for `IndexOutOfBoundsException`, `-4` for `ArithmeticException`, and
  so on, so a program behaves the same on both targets. Index casts use `Math.toIntExact` (T8) instead of a silent `(int)`.
- **wasm:** the trap becomes `unreachable` when there is no `try` (the engine reports it), the same `throw` otherwise.
- **JS/others:** same table as Java; a JS array read out of range returns `undefined`, so the back end must add the compare.

## 4. Build order

| Step | Work | Test |
|---|---|---|
| 0 | **Done: a full `mutmap` grows** (`Containers.nina`: `growIfNearlyFull`). | `core/tests/programs/ContainerTests.poly`: 1000 keys into capacity 2; aliases; string and struct keys; sets |
| 1 | `polyir_trap` runtime function; checked `xs[i]` read/write for `mutarray`/`array`; exit status and message; micro-benchmark | a program per case run as a subprocess, exit 134 and the message text |
| 2 | Reserved throw codes when inside `try`; `catch` sees `-1` | catch an index error, continue |
| 3 | `buffer`, `T[N]`, `grid`, `tensor` per-dimension checks; constant-index compile errors with positions | one error test per shape |
| 4 | Division, shifts, `unwrap` traps | one test each |
| 5 | `m.get(k)`, `m.getOr`, README guidance (needs `optional<T>` for the full form) | missing vs zero |
| 6 | `--unchecked`, `@unchecked`, `getUnchecked` | build size/benchmark, check the flag removes the compare |
| 7 | Java and wasm mappings (with the Java back end) | the same trap program gives the same result on both |

The tests need to run programs and look at their exit status. The current suite JITs or inspects IR; subprocess runs exist in
`examples/java-mapping/run-check.sh` and `examples/frontend/demo.sh`, and the suite needs a small helper for them.

## 5. Decisions needed

1. **Checks on in release builds, or only in debug?** Recommendation: on everywhere, opt out per build or function.
2. **Are traps catchable?** Recommendation: yes, as `throw` with reserved codes inside a `try`, fatal outside.
3. **Full `mutmap`: grow or trap?** Decided and done: it grows (step 0).
4. **Keep `m[k]` returning zero for a missing key?** Recommendation: keep, add `get`/`getOr`, and steer the docs to them.
5. **Reserved code values.** Negative `i64`s collide with nothing today because user codes are positive in every example; confirm.
6. **Exit status 134 and the message format**, versus a status chosen by the program (`main` returning).
7. **Strings** wait for the string model; do you want a length-prefixed representation now so `s[i]` can be checked in O(1)?
