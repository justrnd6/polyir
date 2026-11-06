#!/usr/bin/env python3
"""Builds polyir-guide.pdf from guide.html.in.

  python3 docs/build.py            (run from anywhere; needs Google Chrome)

Placeholders in the template:
  {{file:PATH}}   the file's text, HTML-escaped (PATH is relative to docs/, to core/ or
                  to the repository, e.g. snippets/kernels.metal, stdlib/Math.poly or ideas/gpu/Vulkan.poly)
  {{lines:PATH:A:B}}  the same, but only lines A to B (1-based, inclusive)
  {{raw:PATH}}        the file's text exactly as it is (an SVG figure drawn by snippets/make-figures.py)
  {{def:PATH:NAME}}   one definition (a `func`, `kernel func`, `struct` or `extern func` called NAME, or a method of a
                      struct) with the comment lines just above it; NAME1+NAME2 gives several, one after the other.
                      Unlike `lines:` it keeps pointing at the right code when the file changes.
  {{pgm:PATH}} or {{pgm:PATH:N}}  a grey PGM picture (optionally blown up N times), embedded as a PNG data URI
The snippets in docs/snippets/ are produced by the real compiler
(`polyir --emit-msl` / `--emit-glsl`), so the guide shows what it really writes.
"""
import base64, html, os, re, struct, subprocess, sys, textwrap, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))   # core/: examples, stdlib, tests
REPO = os.path.dirname(ROOT)                     # the repository: ideas/ (the libraries under review and their examples)
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"


def read(path):
    for base in (HERE, ROOT, REPO):
        full = os.path.join(base, path)
        if os.path.exists(full):
            return open(full, encoding="utf-8").read()
    sys.exit("missing file: " + path)


def definition(path, name):
    """The definition called `name` in a source file, with the comment lines directly above it."""
    lines = read(path).splitlines(True)
    pattern = re.compile(r"^(\s*)(?:expose\s+)?(?:kernel func|func|struct|extern func)\s+" + re.escape(name) + r"\b")
    for first, line in enumerate(lines):
        found = pattern.match(line)
        if found:
            break
    else:
        sys.exit("no definition of %s in %s" % (name, path))
    indent = found.group(1)
    start = first
    while start > 0 and lines[start - 1].startswith(indent + "//"):
        start -= 1
    last = first
    if "extern func" in line:
        while lines[last].rstrip().endswith(","):
            last += 1
    else:
        while lines[last].rstrip("\n") != indent + "}":
            last += 1
    return textwrap.dedent("".join(lines[start:last + 1]))


def fill(match):
    parts = match.group(1).split(":")
    if parts[0] == "file":
        text = read(parts[1])
    elif parts[0] == "raw":
        return read(parts[1]).strip()    # inserted as it is (for an SVG diagram), not escaped
    elif parts[0] == "def":
        text = "\n".join(definition(parts[1], name) for name in parts[2].split("+"))
    else:
        lo, hi = int(parts[2]), int(parts[3])
        text = "".join(read(parts[1]).splitlines(True)[lo - 1:hi])
    return html.escape(text.rstrip("\n"))


def pgm_data_uri(path, scale=1):
    """A binary grey (PGM) or colour (PPM) picture, as the examples write, as a PNG data URI."""
    data = open(os.path.join(HERE, path), "rb").read()
    parts = data.split(b"\n", 3)
    channels = 3 if parts[0] == b"P6" else 1
    width, height = map(int, parts[1].split())
    pixels = parts[3]
    if scale > 1 and channels == 1:  # blow the picture up with nearest-neighbour copies, so small images stay crisp
        pixels = b"".join(b"".join(bytes([pixels[(y // scale) * width + x]]) * scale for x in range(width))
                          for y in range(height * scale))
        width, height = width * scale, height * scale
    stride = width * channels
    raw = b"".join(b"\x00" + pixels[y * stride:(y + 1) * stride] for y in range(height))

    def chunk(kind, body):
        crc = zlib.crc32(kind + body) & 0xFFFFFFFF
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", crc)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2 if channels == 3 else 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    return "data:image/png;base64," + base64.b64encode(png).decode()


def pgm_args(spec):
    """'path' or 'path:scale' -> (path, scale)."""
    path, _, scale = spec.partition(":")
    return path, int(scale or 1)


def renumber_figures(template):
    """Numbers the figures 1, 2, 3 ... in the order they appear on the page, and rewrites every mention.

    In the template a caption starts `<figcaption>Figure 12.` or `Figure 12a.` or, for a figure added later,
    `Figure @name.` (any label); the number written there only identifies the figure. Mentions in the text
    use the same label. A mention with no caption of its own (`Figure 33` for `33a`) means the `a` figure.
    """
    captions = re.findall(r"<figcaption>Figure (\d+[a-z]?|@\w+)", template)
    numbers = {old: str(i + 1) for i, old in enumerate(captions)}

    def mapped(match):
        token = match.group(1)
        new = numbers.get(token) or numbers.get(token + "a")
        return "Figure " + new if new else match.group(0)

    return re.sub(r"Figure (\d+[a-z]?|@\w+)", mapped, template)


def main():
    template = renumber_figures(read("guide.html.in"))
    page = re.sub(r"\{\{pgm:([^}]+)\}\}", lambda m: pgm_data_uri(*pgm_args(m.group(1))), template)
    page = re.sub(r"\{\{((?:file|lines|def|raw):[^}]+)\}\}", fill, page)
    out_html = os.path.join(HERE, "polyir-guide.html")
    open(out_html, "w", encoding="utf-8").write(page)
    pdf = os.path.join(HERE, "polyir-guide.pdf")
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--no-pdf-header-footer",
                    "--print-to-pdf=" + pdf, "file://" + out_html], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("wrote", pdf)


main()
