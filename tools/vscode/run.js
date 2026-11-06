// Running polyir from the editor: "Run" (the default target), "Run on Target…" (any target the polyir CLI knows),
// "Run on All Targets" (each available target, results compared), a ▶ code lens above every function that takes no
// arguments, and compile errors as diagnostics. All the work is the polyir CLI's (`--run --target T`,
// `--run-targets`, `--check`, `--project DIR` for a Polyfile project); this file only spawns it and shows what it says.
const vscode = require("vscode");
const cp = require("child_process");
const path = require("path");
const fs = require("fs");

// The targets in the order the picker lists them; `polyir --run-targets` says which are installed.
const TARGETS = ["interp", "jit", "native", "wasm", "java", "kotlin", "js", "ts", "python", "swift", "c", "cpp",
                 "objc", "rust"];

function config() {
  return vscode.workspace.getConfiguration("polyir");
}

// The CLI: the setting, then this repo's own build (`../../build/polyir-check`; this extension is tools/vscode), then PATH.
function findCli() {
  const configured = config().get("cliPath", "");
  if (configured && configured !== "polyir-check") { return configured; }
  const built = path.join(__dirname, "..", "..", "build", "polyir-check");
  return fs.existsSync(built) ? built : "polyir-check";
}

// The Polyfile project the file belongs to: the nearest folder above it holding a `Polyfile`, or none.
function projectOf(file) {
  let dir = path.dirname(file);
  for (let i = 0; i < 12; i++) {
    if (fs.existsSync(path.join(dir, "Polyfile"))) { return dir; }
    const up = path.dirname(dir);
    if (up === dir) { break; }
    dir = up;
  }
  return null;
}

// What the CLI is asked to run: the file, or (with a Polyfile) the project the file is part of.
function subjectArgs(file) {
  const project = projectOf(file);
  return project ? ["--project", project] : [path.basename(file)];
}

function runArgs(file, target, entry) {
  const args = subjectArgs(file).concat(["--run"]);
  if (target) { args.push("--target", target); }
  const chosen = entry || config().get("entry", "");
  if (chosen) { args.push("--entry", chosen); }
  const words = config().get("args", "");
  if (words) { args.push("--args", words); }
  return args;
}

class Runner {
  constructor() {
    this.output = vscode.window.createOutputChannel("polyir");
    this.running = null;
  }

  // Spawns the CLI with `args` in the file's folder and streams everything it prints into the Output panel.
  run(file, args, title) {
    if (this.running) { this.running.kill(); }
    this.output.clear();
    this.output.show(true);
    this.output.appendLine("$ polyir " + args.join(" ") + "    (" + title + ")");
    const child = cp.spawn(findCli(), args, { cwd: path.dirname(file) });
    this.running = child;
    child.stdout.on("data", (d) => this.output.append(d.toString()));
    child.stderr.on("data", (d) => this.output.append(d.toString()));
    child.on("error", (e) => this.output.appendLine(this.spawnError(e)));
    child.on("close", (code) => {
      if (this.running === child) { this.running = null; }
      this.output.appendLine("\n[polyir exited with status " + code + "]");
    });
  }

  spawnError(e) {
    if (e.code !== "ENOENT") { return "failed to start polyir: " + e.message; }
    return "polyir CLI not found at \"" + findCli() + "\" — set \"polyir.cliPath\", or build it: cmake -B build -G Ninja && ninja -C build";
  }
}

// `polyir --run-targets` prints `name<TAB>available` or `name<TAB>missing<TAB>how to install` per target.
function availableTargets() {
  const r = cp.spawnSync(findCli(), ["--run-targets"], { encoding: "utf8", timeout: 10000 });
  if (r.error || r.status !== 0 || !r.stdout) {
    return TARGETS.map((name) => ({ name, available: true, hint: "" }));
  }
  return r.stdout.split("\n").filter((l) => l.trim()).map((l) => {
    const [name, state, hint] = l.split("\t");
    return { name, available: state === "available", hint: hint || "" };
  });
}

async function pickTarget() {
  const items = availableTargets().map((t) => ({
    label: t.name,
    description: t.available ? "" : "not installed",
    detail: t.available ? undefined : t.hint,
    target: t,
  }));
  const chosen = await vscode.window.showQuickPick(items, { placeHolder: "Run on which target?" });
  if (!chosen) { return null; }
  if (!chosen.target.available) {
    vscode.window.showWarningMessage("polyir: " + chosen.label + " is not installed. " + chosen.target.hint);
    return null;
  }
  return chosen.label;
}

function activePolyFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== "polyir") {
    vscode.window.showWarningMessage("polyir: open a .poly file first.");
    return null;
  }
  return editor.document;
}

// Runs the document after saving it (the CLI reads the file from disk).
async function runDocument(runner, doc, target, entry) {
  if (doc.isDirty) { await doc.save(); }
  const file = doc.uri.fsPath;
  runner.run(file, runArgs(file, target, entry), (target || "default target") + (entry ? ", " + entry : ""));
}

// ── code lens: ▶ Run | Run on… above `func main(` and every function that takes no arguments ────────────────────

const RUNNABLE = /^\s*func\s+([A-Za-z_]\w*)\s*\(\s*\)/;

class RunLens {
  provideCodeLenses(doc) {
    const lenses = [];
    for (let i = 0; i < doc.lineCount; i++) {
      const m = RUNNABLE.exec(doc.lineAt(i).text);
      if (!m) { continue; }
      const range = new vscode.Range(i, 0, i, 0);
      lenses.push(new vscode.CodeLens(range, { title: "▶ Run", command: "polyir.runEntry", arguments: [doc.uri, m[1], null] }));
      lenses.push(new vscode.CodeLens(range, { title: "Run on…", command: "polyir.runEntry", arguments: [doc.uri, m[1], "pick"] }));
    }
    return lenses;
  }
}

// ── diagnostics: `polyir FILE --check` prints `file:line:col: message` lines under its headline ─────────────────

const PROBLEM = /^\s*(.+?):(\d+):(\d+):\s*(.*)$/;

function checkDocument(collection, doc) {
  if (doc.languageId !== "polyir" || doc.uri.scheme !== "file") { return; }
  const file = doc.uri.fsPath;
  cp.execFile(findCli(), [path.basename(file), "--check"], { cwd: path.dirname(file), timeout: 20000 }, (err, stdout) => {
    if (err && err.code === "ENOENT") { return; }
    const found = [];
    for (const line of (stdout || "").split("\n")) {
      const m = PROBLEM.exec(line);
      if (!m || path.basename(m[1]) !== path.basename(file)) { continue; }
      const row = Math.max(0, parseInt(m[2], 10) - 1), col = Math.max(0, parseInt(m[3], 10) - 1);
      const warning = /warning|never reassigned|unused/i.test(m[4]);
      const severity = warning ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error;
      const d = new vscode.Diagnostic(new vscode.Range(row, col, row, col + 1), m[4], severity);
      d.source = "polyir";
      found.push(d);
    }
    collection.set(doc.uri, found);
  });
}

function registerRun(context) {
  const runner = new Runner();
  context.subscriptions.push(runner.output);
  const command = (name, fn) => context.subscriptions.push(vscode.commands.registerCommand(name, fn));

  command("polyir.run", async () => {
    const doc = activePolyFile();
    if (doc) { await runDocument(runner, doc, config().get("defaultTarget", "interp"), null); }
  });
  command("polyir.runOnTarget", async () => {
    const doc = activePolyFile();
    const target = doc ? await pickTarget() : null;
    if (target) { await runDocument(runner, doc, target, null); }
  });
  command("polyir.runAll", async () => {
    const doc = activePolyFile();
    if (!doc) { return; }
    if (doc.isDirty) { await doc.save(); }
    const file = doc.uri.fsPath;
    runner.run(file, subjectArgs(file).concat(["--run", "--all"]), "every available target");
  });
  command("polyir.runEntry", async (uri, entry, how) => {
    const doc = await vscode.workspace.openTextDocument(uri);
    const target = how === "pick" ? await pickTarget() : config().get("defaultTarget", "interp");
    if (target) { await runDocument(runner, doc, target, entry === "main" ? null : entry); }
  });

  context.subscriptions.push(vscode.languages.registerCodeLensProvider({ language: "polyir" }, new RunLens()));

  const diagnostics = vscode.languages.createDiagnosticCollection("polyir");
  context.subscriptions.push(diagnostics);
  const check = (doc) => { if (config().get("checkOnSave", true)) { checkDocument(diagnostics, doc); } };
  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(check));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(check));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((doc) => diagnostics.delete(doc.uri)));
  vscode.workspace.textDocuments.forEach(check);
}

module.exports = { registerRun, findCli };
