# polyir VS Code extension

Syntax highlighting for `.poly` files and `Polyfile` projects, split views of the program as any target's code and
of its output (like Markdown's "Open Preview to the Side"), running on any target, compile errors as diagnostics,
and a live preview of the LLVM IR. No npm install/build step for this extension itself; `./package.sh` builds a
`.vsix` when you want one (`code --install-extension polyir-lang-<version>.vsix`).

Translate pages (`targets/<lang>/<Lang>.translate`) and canons (`targets/Polyir.canon`, nina-lite's `NinaLite.canon`) are
highlighted too (`core/docs/translate.md`). In a page you'll see:
- the head lines and `## sections`;
- each rule's construct, its role, its parts and their patterns;
- the `todo`, `unimplemented`, `unsupported`, `lower` and `style` values;
- the `{part}`, `{part.role}`, `{part?}` and `{part|sep}` holes in the templates;
- the `before:`, `hoist:`, `escape:` and `uses:` lines;
- the operators, reserved words, imports and support files.

In a canon you'll see forms with their parts' kinds, `facts:` and `lowered`, kinds and families. A line ending in
`:=` indents the next one.

## The editor title bar

With a `.poly` file open, four buttons sit in the title bar, left to right:

| button | command | what it does |
|---|---|---|
| ▶ | **polyir: Run** (`Cmd+F5`) | runs the file on `polyir.defaultTarget` (default `interp`); the output streams into the **polyir** Output panel |
| output | **polyir: Show Run Output to the Side** | the run's output in a pane beside the source, re-run on every save |
| preview | **polyir: Show Generated Code to the Side…** (`Cmd+K V`) | pick a target (Java, Kotlin, JavaScript, TypeScript, Python, Swift, C, C++, Objective-C, Rust, LLVM IR, typed HIR) and its code opens beside the source, highlighted as that language and refreshed as you type |
| binary | **polyir: Show Compiled LLVM IR** (`Cmd+K Cmd+I`) | the live LLVM IR (below) |

The `…` menu next to them has **Run on Target…** (any target; one whose toolchain is missing says how to install
it) and **Run on All Targets** (each installed target, results compared). A `▶ Run | Run on…` code lens sits above
every function that takes no arguments. The generated-code view checks your unsaved text, then writes the target's
code under the file's own name, so the class and file names match what `polyir FILE --emit-<target>` prints for the
saved file. A file inside a folder with a `Polyfile` runs as that project.

Settings: `polyir.cliPath` (empty: this repository's `build/polyir-check`, then `PATH`), `polyir.defaultTarget`,
`polyir.entry`, `polyir.args`, `polyir.checkOnSave` (diagnostics from `polyir FILE --check`), `polyir.daemonPath`.

**Install (dev-style, symlinked):**

```
./install-dev.sh
```

Then reload VS Code (`Cmd+Shift+P` → "Developer: Reload Window"). Run
`./install-dev.sh --uninstall` to remove it.

## Show Compiled LLVM IR

With a `.poly` file open: click the preview icon in the editor's title
bar, run **polyir: Show Compiled LLVM IR** from the command palette, or
press `Cmd+K Cmd+I` (`Ctrl+K Ctrl+I` on Linux/Windows). Opens the real
compiled LLVM IR beside your source and keeps it live: editing the
source (even before saving) refreshes the IR pane a moment later. A
compile error shows the compiler's own error text in the IR pane
instead.

It can talk to a `polyird` daemon, a persistent background process spawned
once and kept alive over stdio, so a live preview doesn't re-pay process
startup on every edit. The daemon was written in Nina and is no longer part
of this repository; without one the preview uses the plain CLI.

The daemon binary is auto-detected (checked, in order: the
`polyir.daemonPath` setting if you set one; a path fixed relative to
this extension's own install location; then walking up from your open
workspace folder; then just assuming it's on `PATH`). If the daemon
can't be found or fails to start, the preview degrades automatically
to shelling out to the plain `polyir-check` CLI fresh per request instead (no
daemon needed, just slower), which the repository's build makes:

```
cmake -B build -G Ninja && ninja -C build      # from the repository root
```

If neither binary is on your `PATH`, set `polyir.daemonPath`/
`polyir.cliPath` in Settings to their absolute paths.

Related: `../../README.md`, `../README.md`.
