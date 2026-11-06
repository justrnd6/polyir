// Side-by-side views of a .poly file, like Markdown's "Open Preview to the Side": the program as generated for a
// target (Java, Kotlin, JS, TS, Python, Swift, C, C++, Objective-C, Rust, LLVM IR, HIR) and the output of running it.
// Each view is a read-only virtual document beside the source. Generated code follows the editor as you type: the
// unsaved text is checked once (`--emit-hir-json` on a hidden copy beside the file, so imports still resolve) and the
// HIR is turned into the target's code under the file's own name (`--from-hir-json F --name <Name>`), so the class
// names are the real ones. The run output refreshes when the file is saved.
const vscode = require("vscode");
const cp = require("child_process");
const path = require("path");
const fs = require("fs");
const { findCli } = require("./run");

const DEBOUNCE_MS = 500;

const SCHEME = "polyir-view";

// What each generated-code view is called, the CLI flag that makes it, and the language VS Code highlights it as.
const OUTPUTS = [
  { target: "java", label: "Java", flag: "--emit-java", language: "java", ext: "java" },
  { target: "kotlin", label: "Kotlin", flag: "--emit-kotlin", language: "kotlin", ext: "kt" },
  { target: "js", label: "JavaScript", flag: "--emit-js", language: "javascript", ext: "js" },
  { target: "ts", label: "TypeScript", flag: "--emit-ts", language: "typescript", ext: "ts" },
  { target: "python", label: "Python", flag: "--emit-python", language: "python", ext: "py" },
  { target: "swift", label: "Swift", flag: "--emit-swift", language: "swift", ext: "swift" },
  { target: "c", label: "C", flag: "--emit-c", language: "c", ext: "c" },
  { target: "cpp", label: "C++", flag: "--emit-cpp", language: "cpp", ext: "cpp" },
  { target: "objc", label: "Objective-C", flag: "--emit-objc", language: "objective-c", ext: "m" },
  { target: "rust", label: "Rust", flag: "--emit-rust", language: "rust", ext: "rs" },
  { target: "llvm", label: "LLVM IR", flag: "--emit-llvm", language: "llvm-ir", ext: "ll" },
  { target: "hir", label: "Typed HIR", flag: "--emit-hir", language: "plaintext", ext: "hir" },
];

// A view's URI: the source path plus the view's extension (so the tab reads `gcd.poly.java`), and the view in the query.
function viewUri(source, kind, target, ext) {
  return vscode.Uri.from({ scheme: SCHEME, path: source + "." + ext, query: kind + "=" + target });
}

function sourceOf(uri) {
  return uri.path.slice(0, uri.path.lastIndexOf("."));
}

function cli(source, args, input) {
  return cp.spawnSync(findCli(), args, { cwd: path.dirname(source), encoding: "utf8", timeout: 120000, input });
}

function shown(r) {
  if (r.error) {
    return r.error.code === "ENOENT" ? "polyir CLI not found at \"" + findCli() + "\" — set \"polyir.cliPath\"." : r.error.message;
  }
  return (r.stdout || "") + (r.stderr || "") + (r.status ? "\n[polyir exited with status " + r.status + "]\n" : "");
}

// The text in the editor (saved or not) for the file at `source`.
function editorText(source) {
  const doc = vscode.workspace.textDocuments.find((d) => d.uri.fsPath === source);
  return doc ? doc.getText() : fs.readFileSync(source, "utf8");
}

// `Gcd.poly` -> `Gcd`: the name the outputs call the program after (the Java class, the Kotlin object, …).
function programName(source) {
  return path.basename(source).replace(/\.poly$/, "");
}

// The checked program as HIR JSON, from the editor's text: a hidden copy beside the file is checked, then removed.
function hirJsonOf(source) {
  const copy = path.join(path.dirname(source), "." + path.basename(source, ".poly") + ".view.poly");
  fs.writeFileSync(copy, editorText(source), "utf8");
  try {
    return cli(source, [path.basename(copy), "--emit-hir-json"]);
  } finally {
    try { fs.unlinkSync(copy); } catch (e) { /* already gone */ }
  }
}

// The program as the target's code: LLVM IR from the source text, everything else from its HIR under its own name.
function generated(source, out) {
  const hir = hirJsonOf(source);
  const json = (hir.stdout || "").trimStart();
  if (hir.error || hir.status || !(json.startsWith("[") || json.startsWith("{"))) {
    return shown(hir);
  }
  if (out.target === "llvm") { return shown(cli(source, [path.basename(source), "--emit-llvm"])); }
  const flags = out.target === "hir" ? ["--emit-hir"] : ["--name", programName(source), out.flag];
  return shown(fromHirJson(source, hir.stdout, flags));
}

// The outputs made from HIR JSON, handed over in a hidden file beside the source (removed after).
function fromHirJson(source, json, flags) {
  const file = path.join(path.dirname(source), "." + programName(source) + ".view.hir.json");
  fs.writeFileSync(file, json, "utf8");
  try {
    return cli(source, ["--from-hir-json", path.basename(file)].concat(flags));
  } finally {
    try { fs.unlinkSync(file); } catch (e) { /* already gone */ }
  }
}

class ViewProvider {
  constructor() {
    this._onDidChange = new vscode.EventEmitter();
    this.onDidChange = this._onDidChange.event;
    this.open = new Set(); // URIs of the views showing, so a save refreshes only those
  }

  provideTextDocumentContent(uri) {
    const source = sourceOf(uri);
    const [kind, target] = uri.query.split("=");
    const file = path.basename(source);
    if (kind === "run") {
      const header = "// polyir " + file + " --run --target " + target + "   (re-runs when the file is saved)\n\n";
      return header + shown(cli(source, [file, "--run", "--target", target]));
    }
    return generated(source, OUTPUTS.find((o) => o.target === target));
  }

  // Generated-code views follow every edit (after a pause); run views wait for a save.
  refreshEdited(source) {
    for (const s of this.open) {
      const uri = vscode.Uri.parse(s);
      if (sourceOf(uri) === source && uri.query.startsWith("emit=")) { this._onDidChange.fire(uri); }
    }
  }

  refreshFor(source) {
    for (const s of this.open) {
      const uri = vscode.Uri.parse(s);
      if (sourceOf(uri) === source) { this._onDidChange.fire(uri); }
    }
  }
}

async function showBeside(provider, uri, language) {
  provider.open.add(uri.toString());
  let doc = await vscode.workspace.openTextDocument(uri);
  if (language) {
    // A language with no extension installed (Kotlin, Swift on a bare VS Code) shows as plain text.
    try { doc = await vscode.languages.setTextDocumentLanguage(doc, language); } catch (e) { /* stays plain text */ }
  }
  await vscode.window.showTextDocument(doc, { viewColumn: vscode.ViewColumn.Beside, preview: false, preserveFocus: true });
}

async function activeSource() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== "polyir") {
    vscode.window.showWarningMessage("polyir: open a .poly file first.");
    return null;
  }
  if (editor.document.isDirty) { await editor.document.save(); }
  return editor.document.uri.fsPath;
}

function registerPreviews(context) {
  const provider = new ViewProvider();
  context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider(SCHEME, provider));

  context.subscriptions.push(vscode.commands.registerCommand("polyir.showGenerated", async () => {
    const source = await activeSource();
    if (!source) { return; }
    const items = OUTPUTS.map((o) => ({ label: o.label, description: o.flag, output: o }));
    const last = context.workspaceState.get("polyir.lastGenerated");
    items.sort((a, b) => (b.output.target === last) - (a.output.target === last));
    const chosen = await vscode.window.showQuickPick(items, { placeHolder: "Show the program as which target's code?" });
    if (!chosen) { return; }
    context.workspaceState.update("polyir.lastGenerated", chosen.output.target);
    await showBeside(provider, viewUri(source, "emit", chosen.output.target, chosen.output.ext), chosen.output.language);
  }));

  context.subscriptions.push(vscode.commands.registerCommand("polyir.showRunOutput", async () => {
    const source = await activeSource();
    if (!source) { return; }
    const target = vscode.workspace.getConfiguration("polyir").get("defaultTarget", "interp");
    await showBeside(provider, viewUri(source, "run", target, "out"), "plaintext");
  }));

  const timers = new Map();
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument((e) => {
    if (e.document.languageId !== "polyir") { return; }
    const source = e.document.uri.fsPath;
    clearTimeout(timers.get(source));
    timers.set(source, setTimeout(() => provider.refreshEdited(source), DEBOUNCE_MS));
  }));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((doc) => {
    if (doc.languageId === "polyir") { provider.refreshFor(doc.uri.fsPath); }
  }));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((doc) => {
    if (doc.uri.scheme === SCHEME) { provider.open.delete(doc.uri.toString()); }
  }));
}

module.exports = { registerPreviews };
