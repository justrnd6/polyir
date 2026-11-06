// polyir IR preview — "split view to show the translated llvm this will
// look like". Zero npm dependencies, zero build step (plain CommonJS,
// same dev-install philosophy as the rest of this extension): the real
// work (compiling polyir source to LLVM IR) always happens in a real
// polyir binary, never reimplemented here.
//
// Talks to `polyird` — a persistent background daemon (mirroring
// ninad's own shape: a long-running process spawned ONCE, talked to
// over stdio) — so a live, debounced preview doesn't re-pay the
// `polyir` CLI's process-startup cost on every edit. If the daemon
// can't be found or fails to start, this degrades gracefully to
// spawning the plain `polyir` CLI fresh per request (the ORIGINAL
// approach), so the feature still works either way — same
// fallback shape `ninad`'s own VS Code client uses.
//
// A virtual `polyir-ir:` document is the preview; VS Code's own
// TextDocumentContentProvider machinery (the same mechanism Markdown
// Preview is built on) handles the split-view editor and in-place
// refresh — no language server, no polling.
const vscode = require("vscode");
const cp = require("child_process");
const path = require("path");
const fs = require("fs");
const readline = require("readline");
const { registerRun, findCli } = require("./run");
const { registerPreviews } = require("./preview");

const IR_SCHEME = "polyir-ir";
const DEBOUNCE_MS = 400;

function config() {
  return vscode.workspace.getConfiguration("polyir");
}

function cliPath() {
  return findCli();
}

// ── fallback: spawn the plain CLI fresh, once per call ──────────────────────
// Kept as the degrade path when the daemon isn't available — see
// `IRContentProvider.provideTextDocumentContent` below.

function compileToIRviaCli(sourcePath, text) {
  const dir = path.dirname(sourcePath);
  const tempPath = path.join(dir, "." + path.basename(sourcePath) + ".ir-preview.poly");
  fs.writeFileSync(tempPath, text, "utf8");
  try {
    const result = cp.spawnSync(cliPath(), [tempPath, "--emit-llvm"], { encoding: "utf8", timeout: 10000 });
    if (result.error) {
      if (result.error.code === "ENOENT") {
        return "; polyir CLI not found at \"" + cliPath() + "\".\n" +
          "; Set \"polyir.cliPath\" in Settings, or build it first:\n" +
          ";   cd cli && nina up . build\n";
      }
      return "; failed to run the polyir CLI: " + result.error.message + "\n";
    }
    // A REPORTED compile failure already exits 0 with its own clear
    // "compile failed:\n  ..." text on stdout (confirmed empirically —
    // the CLI doesn't distinguish exit codes for this) — just show it
    // as-is, no special-casing needed. A genuinely nonzero exit here
    // means the CLI itself crashed/was killed (a signal), which has no
    // stdout message of its own to show.
    if (result.status !== 0) {
      return "; polyir exited with status " + result.status + " (crashed) — " +
        ((result.stdout || "") + (result.stderr || "")).trim() + "\n";
    }
    return result.stdout;
  } finally {
    try { fs.unlinkSync(tempPath); } catch (e) { /* best-effort cleanup */ }
  }
}

// ── polyird: the persistent daemon client ─────────────────────────────────────
// Wire protocol, deliberately NOT JSON (std `Json`/`Dynamic` have no
// cpp17 binding at all — confirmed by trying it; porting one is out of
// scope for what this single-purpose daemon needs): a request is a
// header line `<id>\t<path>\t<lineCount>` followed by exactly
// `lineCount` more lines of source text. A response is the same shape:
// `<id>\t<ok 0|1>\t<lineCount>` then that many lines of either
// rendered LLVM IR or the compiler's own error text. See
// `../polyird/src/Main.nina` for the daemon's own side of this.

// Mirrors nina-vscode's own `findNinad()`: an explicit setting first;
// then the path FIXED relative to this extension's own install
// location (`polyird` is always this file's own sibling project,
// `../polyird`, regardless of what the user opened as their workspace
// — the one check that still works if they opened the WHOLE monorepo,
// not just the `polyir/` folder, since polyird then sits two levels
// BELOW the workspace root, not above it, where the next check alone
// would never find it); then walk up from each workspace folder
// looking for the project's own build output; then just assume it's
// on PATH.
function findPolyird() {
  const configured = config().get("daemonPath", "");
  if (configured) { return configured; }
  const relative = path.join(__dirname, "..", "polyird", "build", "out", "polyird");
  if (fs.existsSync(relative)) { return relative; }
  for (const folder of vscode.workspace.workspaceFolders || []) {
    let dir = folder.uri.fsPath;
    for (let i = 0; i < 4; i++) {
      const candidate = path.join(dir, "polyird", "build", "out", "polyird");
      if (fs.existsSync(candidate)) { return candidate; }
      dir = path.dirname(dir);
    }
  }
  return "polyird";
}

class PolyirdClient {
  constructor(command) {
    this.child = cp.spawn(command, [], { stdio: ["pipe", "pipe", "pipe"] });
    this.nextId = 1;
    this.pending = new Map(); // id (string) -> resolve({ok, text})
    this.header = null; // {id, ok, remaining, lines} while collecting a response body
    this.rl = readline.createInterface({ input: this.child.stdout });
    this.rl.on("line", line => this._onLine(line));
  }

  _onLine(line) {
    if (!this.header) {
      const parts = line.split("\t");
      const remaining = parseInt(parts[2], 10) || 0;
      this.header = { id: parts[0], ok: parts[1] === "1", remaining: remaining, lines: [] };
      if (remaining === 0) { this._finish(); }
      return;
    }
    this.header.lines.push(line);
    this.header.remaining -= 1;
    if (this.header.remaining <= 0) { this._finish(); }
  }

  _finish() {
    const h = this.header;
    this.header = null;
    const resolve = this.pending.get(h.id);
    this.pending.delete(h.id);
    if (resolve) { resolve({ ok: h.ok, text: h.lines.join("\n") }); }
  }

  request(sourcePath, text) {
    const id = String(this.nextId++);
    const textLines = text.split("\n");
    return new Promise(resolve => {
      this.pending.set(id, resolve);
      this.child.stdin.write(id + "\t" + sourcePath + "\t" + textLines.length + "\n");
      this.child.stdin.write(textLines.join("\n") + "\n");
    });
  }

  dispose() {
    this.rl.close();
    this.child.kill();
  }
}

// Spawns the daemon ONCE, lazily, on first use — not at `activate()` —
// so opening VS Code never pays this cost for a user who never opens
// a .poly file. Returns `null` (not a rejected promise) on failure,
// so every call site can just fall back to the plain-CLI path without
// its own try/catch.
let daemonClient = null;
let daemonAttempted = false;

function getDaemon() {
  if (daemonAttempted) { return daemonClient; }
  daemonAttempted = true;
  try {
    const client = new PolyirdClient(findPolyird());
    client.child.on("error", () => {
      // ENOENT or similar — never reached once spawned successfully;
      // disposing drops it back to the plain-CLI fallback path.
      if (daemonClient === client) { daemonClient = null; }
    });
    client.child.on("exit", () => {
      if (daemonClient === client) { daemonClient = null; }
    });
    daemonClient = client;
  } catch (e) {
    daemonClient = null;
  }
  return daemonClient;
}

async function compileToIR(sourcePath, text) {
  const daemon = getDaemon();
  if (daemon) {
    try {
      const result = await daemon.request(sourcePath, text);
      return result.text;
    } catch (e) {
      // Fall through to the plain-CLI path below.
    }
  }
  return compileToIRviaCli(sourcePath, text);
}

class IRContentProvider {
  constructor() {
    this._onDidChange = new vscode.EventEmitter();
    this.onDidChange = this._onDidChange.event;
  }

  async provideTextDocumentContent(uri) {
    // `irUriFor` appends ".ll" to the real source path purely so the
    // preview tab's own title/icon read as IR, not polyir — strip it
    // back off to recover the real file to compile.
    const sourcePath = uri.path.endsWith(".ll") ? uri.path.slice(0, -3) : uri.path;
    const open = vscode.workspace.textDocuments.find(d => d.uri.fsPath === sourcePath);
    const text = open ? open.getText() : fs.readFileSync(sourcePath, "utf8");
    return compileToIR(sourcePath, text);
  }

  refresh(uri) {
    this._onDidChange.fire(uri);
  }
}

function irUriFor(sourcePath) {
  return vscode.Uri.from({ scheme: IR_SCHEME, path: sourcePath + ".ll" });
}

function activate(context) {
  registerRun(context);
  registerPreviews(context);
  const provider = new IRContentProvider();
  context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider(IR_SCHEME, provider));

  // source fsPath -> that source's preview Uri (only while its preview
  // tab is actually open, so an edit to a file nobody is previewing
  // never spends a compile).
  const openPreviews = new Map();
  const debounceTimers = new Map();

  context.subscriptions.push(vscode.commands.registerCommand("polyir.showIRPreview", async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== "polyir") {
      vscode.window.showWarningMessage("polyir: open a .poly file first.");
      return;
    }
    const sourcePath = editor.document.uri.fsPath;
    const uri = irUriFor(sourcePath);
    openPreviews.set(sourcePath, uri.toString());
    const doc = await vscode.workspace.openTextDocument(uri);
    await vscode.languages.setTextDocumentLanguage(doc, "llvm-ir");
    await vscode.window.showTextDocument(doc, { viewColumn: vscode.ViewColumn.Beside, preview: true });
  }));

  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(e => {
    const sourcePath = e.document.uri.fsPath;
    if (e.document.languageId !== "polyir" || !openPreviews.has(sourcePath)) { return; }
    clearTimeout(debounceTimers.get(sourcePath));
    debounceTimers.set(sourcePath, setTimeout(() => {
      provider.refresh(vscode.Uri.parse(openPreviews.get(sourcePath)));
    }, DEBOUNCE_MS));
  }));

  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(doc => {
    if (doc.uri.scheme !== IR_SCHEME) { return; }
    for (const [sourcePath, uriStr] of openPreviews) {
      if (uriStr === doc.uri.toString()) { openPreviews.delete(sourcePath); }
    }
  }));
}

function deactivate() {
  if (daemonClient) { daemonClient.dispose(); daemonClient = null; }
}

module.exports = { activate, deactivate };
