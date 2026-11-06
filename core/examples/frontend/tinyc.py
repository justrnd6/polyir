#!/usr/bin/env python3
"""tinyc: a complete front end for the toy language "tiny", targeting polyir.

    tinyc.py prog.tl --poly prog.poly --map prog.map

It writes two files: the polyir program, and a source map that sends every line of the generated polyir text back to the
line and column of the tiny source it came from (the format described in docs/debug-and-meta.md). Then

    polyir prog.poly --debug --source-map prog.map --out prog

gives a native program a debugger can stop in by tiny file and line.

The language:  fn name(a, b) { ... }   let x = e   x = e   while e { ... }   if e { ... } [else { ... }]
               return e   print e   expressions: + - * / < <= > >= == != calls and parentheses.  Every value is a 64-bit int.
"""
import re, sys

TOKEN = re.compile(r"\s*(?:(//[^\n]*)|([A-Za-z_]\w*)|(\d+)|(<=|>=|==|!=|[-+*/<>=(){},]))")


class Tok:
    def __init__(self, kind, text, line, col):
        self.kind, self.text, self.line, self.col = kind, text, line, col


def lex(src):
    toks, pos, line, col0 = [], 0, 1, 0
    while pos < len(src):
        if src[pos] == "\n":
            pos += 1; line += 1; col0 = pos; continue
        if src[pos] in " \t\r":
            pos += 1; continue
        m = TOKEN.match(src, pos)
        if not m or m.end() == pos:
            raise SyntaxError("%d: unexpected %r" % (line, src[pos]))
        col = m.start(m.lastindex) - col0 + 1
        if m.lastindex == 1:
            pass
        elif m.lastindex == 2:
            toks.append(Tok("name", m.group(2), line, col))
        elif m.lastindex == 3:
            toks.append(Tok("num", m.group(3), line, col))
        else:
            toks.append(Tok("op", m.group(4), line, col))
        pos = m.end()
    return toks


class Parser:
    def __init__(self, toks):
        self.t, self.i = toks, 0

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else Tok("eof", "", 0, 0)

    def take(self, text=None):
        tok = self.peek()
        if text is not None and tok.text != text:
            raise SyntaxError("%d:%d: expected %r, found %r" % (tok.line, tok.col, text, tok.text))
        self.i += 1
        return tok

    def program(self):
        fns, main = [], []
        while self.peek().kind != "eof":
            if self.peek().text == "fn":
                fns.append(self.fn())
            else:
                main.append(self.stmt())
        return fns, main

    def fn(self):
        start = self.take("fn")
        name = self.take().text
        self.take("(")
        params = []
        while self.peek().text != ")":
            params.append(self.take().text)
            if self.peek().text == ",":
                self.take()
        self.take(")")
        body, end = self.block()
        return {"name": name, "params": params, "body": body, "line": start.line, "col": start.col, "end": end}

    def block(self):
        self.take("{")
        out = []
        while self.peek().text != "}":
            out.append(self.stmt())
        close = self.take("}")
        return out, close

    def stmt(self):
        tok = self.peek()
        node = {"line": tok.line, "col": tok.col}
        if tok.text == "let":
            self.take(); node.update(kind="let", name=self.take().text); self.take("="); node["value"] = self.expr()
        elif tok.text == "return":
            self.take(); node.update(kind="return", value=self.expr())
        elif tok.text == "print":
            self.take(); node.update(kind="print", value=self.expr())
        elif tok.text == "while":
            self.take(); node.update(kind="while", cond=self.expr())
            node["body"], node["end"] = self.block()
        elif tok.text == "if":
            self.take(); node.update(kind="if", cond=self.expr())
            node["then"], node["end"] = self.block()
            node["else"] = None
            if self.peek().text == "else":
                self.take(); node["else"], node["end"] = self.block()
        else:
            node.update(kind="assign", name=self.take().text); self.take("="); node["value"] = self.expr()
        return node

    def expr(self):
        return self.binary(0)

    LEVELS = [("==", "!="), ("<", "<=", ">", ">="), ("+", "-"), ("*", "/")]

    def binary(self, level):
        if level == len(self.LEVELS):
            return self.atom()
        left = self.binary(level + 1)
        while self.peek().text in self.LEVELS[level]:
            op = self.take().text
            left = ("bin", op, left, self.binary(level + 1))
        return left

    def atom(self):
        tok = self.take()
        if tok.kind == "num":
            return ("num", tok.text)
        if tok.text == "(":
            e = self.expr(); self.take(")"); return e
        if self.peek().text == "(":
            self.take(); args = []
            while self.peek().text != ")":
                args.append(self.expr())
                if self.peek().text == ",":
                    self.take()
            self.take(")")
            return ("call", tok.text, args)
        return ("var", tok.text)


class Emitter:
    """Writes polyir text and, for every line, where in the tiny source it came from."""

    def __init__(self, source_name):
        self.lines, self.map, self.indent = [], [], 0
        self.sources = [source_name, "<tiny runtime>"]
        self.loops = 0

    def put(self, text, line, col, source=0):
        self.lines.append("  " * self.indent + text)
        self.map.append((len(self.lines), source, line, col))

    def expr(self, e):
        if e[0] == "num":
            return e[1]
        if e[0] == "var":
            return e[1]
        if e[0] == "call":
            return "%s(%s)" % (e[1], ", ".join(self.expr(a) for a in e[2]))
        return "(%s %s %s)" % (self.expr(e[2]), e[1], self.expr(e[3]))

    def cond(self, e):
        # polyir comparisons give i1; tiny's `if` and `while` take that directly
        return self.expr(e)

    def assigned(self, stmts, acc):
        for s in stmts:
            if s["kind"] == "assign": acc.add(s["name"])
            for key in ("body", "then", "else"):
                if s.get(key): self.assigned(s[key], acc)
        return acc

    def stmts(self, stmts, mutable):
        for s in stmts:
            k = s["kind"]
            if k == "let":
                word = "var" if s["name"] in mutable else "let"
                self.put("%s %s: i64 = %s" % (word, s["name"], self.expr(s["value"])), s["line"], s["col"])
            elif k == "assign":
                self.put("%s = %s" % (s["name"], self.expr(s["value"])), s["line"], s["col"])
            elif k == "return":
                self.put("return %s" % self.expr(s["value"]), s["line"], s["col"])
            elif k == "print":
                self.put("printInt(%s)" % self.expr(s["value"]), s["line"], s["col"])
            elif k == "while":
                self.loops += 1
                self.put("for (w%d = 0; %s; w%d = w%d + 1) {" % (self.loops, self.cond(s["cond"]), self.loops, self.loops), s["line"], s["col"])
                self.indent += 1; self.stmts(s["body"], mutable); self.indent -= 1
                self.put("}", s["end"].line, s["end"].col)
            elif k == "if":
                self.put("if (%s) {" % self.cond(s["cond"]), s["line"], s["col"])
                self.indent += 1; self.stmts(s["then"], mutable); self.indent -= 1
                if s["else"]:
                    self.put("} else {", s["end"].line, s["end"].col)
                    self.indent += 1; self.stmts(s["else"], mutable); self.indent -= 1
                self.put("}", s["end"].line, s["end"].col)

    def function(self, fn):
        params = ", ".join("%s: i64" % p for p in fn["params"])
        mutable = self.assigned(fn["body"], set())
        self.put("func %s(%s) -> i64 {" % (fn["name"], params), fn["line"], fn["col"])
        self.indent += 1; self.stmts(fn["body"], mutable); self.indent -= 1
        self.put("}", fn["end"].line, fn["end"].col)

    def main(self, stmts):
        self.put("func main() -> i64 {", stmts[0]["line"] if stmts else 1, 1)
        self.indent += 1; self.stmts(stmts, self.assigned(stmts, set()))
        self.put("return 0", stmts[-1]["line"] if stmts else 1, 1)
        self.indent -= 1
        self.put("}", stmts[-1]["line"] if stmts else 1, 1)

    RUNTIME = [
        "extern func putchar(c: i32) -> i32 = native \"putchar\" | java \"System.out.print((char) $0)\"",
        "func printInt(n: i64) -> i64 {",
        "  var digits: i64 = 1",
        "  for (d = 1; digits * 10 <= n; d = d + 1) { digits = digits * 10 }",
        "  var rest: i64 = n",
        "  for (k = 0; digits > 0; k = k + 1) {",
        "    putchar((48 + rest / digits).toI32())",
        "    rest = rest - (rest / digits) * digits",
        "    digits = digits / 10",
        "  }",
        "  putchar(10)",
        "  return 0",
        "}",
    ]

    def runtime(self):
        for i, text in enumerate(self.RUNTIME, 1):
            self.lines.append(text)
            self.map.append((len(self.lines), 1, i, 1))

    def text(self):
        return "\n".join(self.lines) + "\n"

    def source_map(self):
        out = ["polyir-source-map 1"] + ["source " + s for s in self.sources]
        out += ["map %d %d %d %d" % m for m in self.map]
        return "\n".join(out) + "\n"


def main(argv):
    src_path, poly_path, map_path = argv[1], None, None
    for a, b in zip(argv[2:], argv[3:]):
        if a == "--poly": poly_path = b
        if a == "--map": map_path = b
    fns, stmts = Parser(lex(open(src_path).read())).program()
    em = Emitter(src_path.split("/")[-1])
    for fn in fns:
        em.function(fn)
        em.lines.append(""); em.map.append((len(em.lines), 0, fn["end"].line, 1))
    em.main(stmts)
    em.runtime()
    open(poly_path, "w").write(em.text())
    open(map_path, "w").write(em.source_map())


if __name__ == "__main__":
    main(sys.argv)
