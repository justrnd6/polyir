# tools — the tools and editor support around polyir

| directory | what it is | builds |
|---|---|---|
| [`metabuild/`](metabuild/README.md) | meta-build, written in polyir: a Polyfile → CMake/Gradle/Cargo/npm/pip projects | built into `poly` (`runtime/tools`) by `metabuild/sync-embed.py`; run by `poly up` |
| [`vscode/`](vscode/README.md) | the VS Code extension for `.poly` files: syntax, the live LLVM IR preview, run commands and the generated code of each target (through `polyir-check`) | `vscode/package.sh` → a `.vsix`; `vscode/install-dev.sh` links it into VS Code |

The extension finds `polyir-check` at `../../build/polyir-check`, then on `PATH` (its `polyir.cliPath` setting can name
another). meta-build's host library is `metabuild/src/Host.poly` (files, commands, the environment), which the VM binds.
