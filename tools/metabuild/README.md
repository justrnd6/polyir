# meta-build

A build that builds build projects. You describe a polyir project once, in a `Polyfile`. meta-build writes each
target's own project (CMake for C and C++, Gradle for Java and Kotlin, Cargo for Rust, npm or yarn for JavaScript and
TypeScript, pip for Python) and runs that tool. The tools do what they are trusted for: they resolve, download, verify
and lock the dependencies, and build. meta-build never fetches anything itself, and every generated project can be
read, or built by hand, under `build/`.

```
poly up            gen, check, build every target, then run each one and compare  (the default)
poly gen           write the generated projects only (to read, or to eject)
poly build [T]     build every target, or T
poly run [T]       build and run T (the Polyfile's `target` when none is given)
poly test          run every target and compare what they give
poly tasks         the goals, and each target's steps
poly clean         remove build/
poly NAME          a goal the project's build.poly adds
```

## The Polyfile

One setting per line, `//` comments, and `on OS[/CPU]` at the end of a line to keep it to some machines:

```
name gcd
main src/main.poly
targets native c java rust js python
target native
dep zlib from pkg-config on linux
dep zlib from brew on macos
dep fmt from git https://github.com/fmtlib/fmt tag 10.2.1
dep com.google.guava:guava:33.0.0-jre from maven
dep serde 1.0 from cargo
dep lodash 4.17.21 from npm
dep requests 2.32 from pip
use yarn for js
```

| target | tool | what is written |
|---|---|---|
| c, cpp | cmake | `build/cmake/CMakeLists.txt`: pkg-config and brew libraries through `pkg_check_modules`, git ones through `FetchContent` |
| java, kotlin | gradle | `build/gradle-<target>/`: `settings.gradle.kts`, `build.gradle.kts` (application, Maven Central, the maven deps) |
| rust | cargo | `build/cargo/Cargo.toml`: the crates and git crates |
| js, ts | npm or yarn | `build/npm-<target>/package.json`: the npm deps and git ones |
| python | pip | `build/pip/requirements.txt` and a virtual environment |
| native, wasm, swift, objc, interp, jit | direct | nothing: `polyir main.poly --run --target T` |

- **Checks:** a brew dep is only looked for (`brew --prefix zlib`), never installed. When it is missing, the build stops
  with `zlib is not installed: brew install zlib`. A missing tool names how to get it (rustup, SDKMAN, nodejs.org,
  python.org).
- **Mistakes:** an unknown setting, target or provider, a dep no target takes, or a `use` a target cannot have is a
  problem naming its line, and nothing runs.

## Using a dep from the code

A `dep` line makes the library part of each target's build: the tool fetches it and puts it on the target's path. The
code reaches it in two steps:

- `import LANG "module"` names the host module on that target: a library the targets polyir builds itself load or link
  (`native`), a C header, an npm package, a Python module. Only that target uses it.
- `extern func` is a function the host provides. polyir checks every call against the signature, and each target
  writes the call its own way (`$0` is the first argument).

`examples/deps` computes CRC-32 with zlib (the interpreter, the JIT, the native build and C), Guava, crc32fast, crc-32
and crcmod, and all eight targets give the same answer:

```
// src/Checksum.poly
import native "z"
import c "zlib.h"
import js "crc-32" as crc
import python "crcmod.predefined"

extern func crc32(seed: i64, data: string, size: i64) -> i64
  = native "crc32"
  | c "(int64_t) crc32((unsigned long) $0, (const unsigned char *) $1, (unsigned) $2)"
  | java "com.google.common.hash.Hashing.crc32().hashString($1, java.nio.charset.StandardCharsets.UTF_8).padToLong()"
  | rust "crc32fast::hash($1.as_bytes()) as i64"
  | js "BigInt(crc.str($1) >>> 0)"
  | python "crcmod.predefined.mkPredefinedCrcFun('crc-32')($1.encode())"
```

- The extern has zlib's own shape, so the interpreter, the JIT and the native build call zlib directly. The other
  bindings use only what they need. Java and Rust need no import: a fully qualified name or a crate path reaches the
  library.
- Before any target builds, `polyir main.poly --analyze --targets "…"` reports each called extern that has no binding
  for one of the targets, with the file and line, and nothing builds.
- The targets polyir runs itself get the pkg-config and brew libraries' folders on POLYIR_LIBRARY_PATH, where the
  interpreter and the JIT load `import native` libraries from and the native build links them from.
- Strings cross as the host's own text type (UTF-8 on every target), so a library sees real text.

## build.poly: a project's own tasks

A project can add steps to the plan in a `build.poly` beside its Polyfile. It defines one function, `extend`, which
`poly` checks together with meta-build (no imports are needed: the model in `src/Project.poly` and the Text library are
there):

```
func extend(p: Project, out: Plan) -> void {
  out.steps.push(taskWriting("hir", ["polyir", p.main, "--emit-hir"], "build/main.hir"))   // poly hir
  out.steps.push(task("build", ["wc", "-l", p.main]))                                      // with every build
}
```

- `task(goal, cmd)` is a step of the whole project. `taskWriting(goal, cmd, file)` also writes what the command
  prints to `file`.
- A task with a new goal name makes `poly NAME` a goal that runs only those tasks. A task whose goal is `check`,
  `build` or `run` runs once, before the targets' own steps, whenever that goal runs.
- `poly tasks` lists the added goals with the rest. `examples/tasks` is such a project.
- build.poly runs only through `poly`, on the VM. A front end that uses `src/Library.poly` carries out the plan the Polyfile gives.

## How it is built

meta-build is written in polyir, using its strings (text blocks, `${…}`, `StringBuilder`).

- **The core is pure:** `src/Polyfile.poly` reads a Polyfile for a `Host`, and `plan(p)` in `src/Plan.poly` gives the
  files and steps. It never touches the machine, so it runs on every polyir target, and any front end can use it.
  - `src/Project.poly`: the model.
  - `src/Conventions.poly`: which tool builds each target.
  - `src/gen/*.poly`: one small generator per tool.
- **The runner:** `src/Up.poly` writes the files that changed and runs the steps. It uses the host layer
  (`src/Host.poly`: files, commands, the os and cpu), which the interpreter binds.
- **The command:** `poly` carries meta-build inside it (`runtime/tools/MetaBuildText.cpp`, written by
  `python3 tools/metabuild/sync-embed.py`) and runs it on the VM.

## From other front ends

The core is pure, so a front end can call it as a library and carry out the plan itself: `src/Library.poly` gives it plain
strings in and out (`planWords`). (The Nina library that wrapped it, `nina/`, is no longer part of this repository.)

Tests: `polyir-check tests/CoreTests.poly --interp` (a Polyfile in, the exact plan out; `main` returns how many checks
failed).
