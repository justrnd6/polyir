#!/usr/bin/env python3
# Draws the figures of the sound sections that are plots of exact formulas or diagrams (written to figs/*.svg).
# Run by regenerate.sh. Nothing in them is typed in by hand: the curves are computed here.
import math, os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "figs")
os.makedirs(OUT, exist_ok=True)

def path(points):
    return "M" + "L".join("%.1f %.1f" % p for p in points)

# ---- Figure: the panning law -------------------------------------------------
def pan_figure():
    # left panel: gains L and R against position; right panel: total power for the constant-power rule and for a straight-line rule
    out = ['<svg viewBox="0 0 700 250" width="100%" role="img" aria-label="Panning gains and total power">']
    def panel(x0, title, curves, ymax, ylabels):
        w, h, top = 270, 150, 40
        o = []
        o.append('<text x="%d" y="22" font-size="12" font-weight="700" text-anchor="middle" fill="#1b1f24">%s</text>' % (x0 + w / 2, title))
        o.append('<rect x="%d" y="%d" width="%d" height="%d" fill="none" stroke="#c9d1d9"/>' % (x0, top, w, h))
        for v, label in ylabels:
            y = top + h - v / ymax * h
            o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e1e4e8"/>' % (x0, y, x0 + w, y))
            o.append('<text x="%d" y="%.1f" font-size="10" text-anchor="end" fill="#586069">%s</text>' % (x0 - 5, y + 3, label))
        for pos, label in [(-1, "hard left"), (0, "centre"), (1, "hard right")]:
            x = x0 + (pos + 1) / 2 * w
            o.append('<text x="%.1f" y="%d" font-size="10" text-anchor="middle" fill="#586069">%s</text>' % (x, top + h + 15, label))
        for color, dash, f, label in curves:
            pts = [(x0 + (p + 1) / 2 * w, top + h - f(p) / ymax * h) for p in [i / 40 - 1 for i in range(81)]]
            extra = ' stroke-dasharray="5 4"' if dash else ""
            o.append('<path d="%s" fill="none" stroke="%s" stroke-width="2"%s/>' % (path(pts), color, extra))
        return o
    L = lambda p: math.cos((p + 1) * math.pi / 4)
    R = lambda p: math.sin((p + 1) * math.pi / 4)
    out += panel(60, "volume in each speaker", [("#0b5cad", False, L, "left"), ("#9a6700", False, R, "right")], 1.0,
                 [(0, "0"), (0.5, "0.5"), (0.7071, "0.707"), (1, "1")])
    out += panel(390, "total loudness (left² + right²)", [("#1b7f3b", False, lambda p: L(p) ** 2 + R(p) ** 2, "constant power"),
                                                           ("#c0392b", True, lambda p: ((1 - p) / 2) ** 2 + ((1 + p) / 2) ** 2, "straight line")], 1.2,
                 [(0, "0"), (0.5, "0.5"), (1, "1")])
    out.append('<g font-size="11"><line x1="395" y1="236" x2="425" y2="236" stroke="#1b7f3b" stroke-width="2"/><text x="430" y="240" fill="#1b1f24">cos and sin: stays at 1</text>'
               '<line x1="545" y1="236" x2="575" y2="236" stroke="#c0392b" stroke-width="2" stroke-dasharray="5 4"/><text x="580" y="240" fill="#1b1f24">straight line: dips</text></g>')
    out.append('<g font-size="11"><line x1="65" y1="236" x2="95" y2="236" stroke="#0b5cad" stroke-width="2"/><text x="100" y="240" fill="#1b1f24">left = cos</text>'
               '<line x1="185" y1="236" x2="215" y2="236" stroke="#9a6700" stroke-width="2"/><text x="220" y="240" fill="#1b1f24">right = sin</text></g>')
    out.append('</svg>')
    return "\n".join(out)

# ---- Figure: the windowed sinc ----------------------------------------------------
def sinc_figure():
    reach = 6.0
    def sinc(x):
        return 1.0 if abs(x) < 1e-9 else math.sin(math.pi * x) / (math.pi * x)
    def win(x):
        u = x / reach
        return 0.42 + 0.5 * math.cos(math.pi * u) + 0.08 * math.cos(2 * math.pi * u) if abs(u) <= 1 else 0.0
    x0, w, top, h = 50, 600, 20, 170
    def X(x):
        return x0 + (x + reach) / (2 * reach) * w
    zero = top + 0.72 * h
    scale = 0.62 * h
    def Yv(v):
        return zero - v * scale
    o = ['<svg viewBox="0 0 700 250" width="100%" role="img" aria-label="The windowed sinc curve used to rebuild a wave between samples">']
    o.append('<rect x="%d" y="%d" width="%d" height="%d" fill="none" stroke="#c9d1d9"/>' % (x0, top, w, h))
    o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#8b949e"/>' % (x0, zero, x0 + w, zero))
    pts_sinc = [(X(t), Yv(sinc(t))) for t in [i / 20 - reach for i in range(int(2 * reach * 20) + 1)]]
    pts_win = [(X(t), Yv(win(t))) for t in [i / 20 - reach for i in range(int(2 * reach * 20) + 1)]]
    pts_prod = [(X(t), Yv(sinc(t) * win(t))) for t in [i / 20 - reach for i in range(int(2 * reach * 20) + 1)]]
    o.append('<path d="%s" fill="none" stroke="#0b5cad" stroke-width="1.3" stroke-dasharray="4 3"/>' % path(pts_sinc))
    o.append('<path d="%s" fill="none" stroke="#9a6700" stroke-width="1.3" stroke-dasharray="2 3"/>' % path(pts_win))
    o.append('<path d="%s" fill="none" stroke="#1b7f3b" stroke-width="2.4"/>' % path(pts_prod))
    for k in range(-int(reach), int(reach) + 1):
        o.append('<circle cx="%.1f" cy="%.1f" r="3.2" fill="#1b1f24"/>' % (X(k), Yv(sinc(k) * win(k))))
        o.append('<text x="%.1f" y="%d" font-size="10" text-anchor="middle" fill="#586069">%d</text>' % (X(k), top + h + 14, k))
    o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#586069">distance from the output instant, in input samples</text>' % (x0 + w / 2, top + h + 30))
    o.append('<g font-size="11"><line x1="60" y1="238" x2="90" y2="238" stroke="#0b5cad" stroke-width="1.3" stroke-dasharray="4 3"/><text x="95" y="242" fill="#1b1f24">sinc(x): 0 at every other whole number</text>'
             '<line x1="330" y1="238" x2="360" y2="238" stroke="#9a6700" stroke-width="1.3" stroke-dasharray="2 3"/><text x="365" y="242" fill="#1b1f24">window</text>'
             '<line x1="430" y1="238" x2="460" y2="238" stroke="#1b7f3b" stroke-width="2.4"/><text x="465" y="242" fill="#1b1f24">sinc &#215; window = the weights</text></g>')
    o.append('</svg>')
    return "\n".join(o)

# ---- Figure: the biquad signal flow ----------------------------------------------
def biquad_figure():
    o = ['<svg viewBox="0 0 700 260" width="100%" role="img" aria-label="The signal flow of a biquad filter">']
    def box(x, y, label, w=44, h=30, fill="#f6f8fa"):
        return ('<rect x="%d" y="%d" width="%d" height="%d" rx="4" fill="%s" stroke="#8b949e"/><text x="%d" y="%d" font-size="12" text-anchor="middle" fill="#1b1f24">%s</text>'
                % (x, y, w, h, fill, x + w / 2, y + h / 2 + 4, label))
    def arrow(x1, y1, x2, y2, color="#586069"):
        ang = math.atan2(y2 - y1, x2 - x1)
        hx = [x2 - 8 * math.cos(ang - 0.4), y2 - 8 * math.sin(ang - 0.4), x2 - 8 * math.cos(ang + 0.4), y2 - 8 * math.sin(ang + 0.4)]
        return ('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="1.5"/><path d="M%.1f %.1f L%.1f %.1f L%.1f %.1f Z" fill="%s"/>'
                % (x1, y1, x2, y2, color, x2, y2, hx[0], hx[1], hx[2], hx[3], color))
    # input line on the left at y=70 ; rows of delays going down on the input side (x column 130), outputs on the right (x column 470)
    o.append('<text x="22" y="64" font-size="12" fill="#1b1f24">x[n]</text>')
    o.append(arrow(55, 70, 110, 70))
    # input side: three taps
    o.append(box(110, 55, "&#215; b0"))
    o.append(box(110, 130, "&#215; b1"))
    o.append(box(110, 205, "&#215; b2"))
    o.append(box(60, 100, "z&#8315;&#185;", 40, 24, "#fff8e5"))
    o.append(box(60, 175, "z&#8315;&#185;", 40, 24, "#fff8e5"))
    o.append(arrow(80, 70, 80, 100))
    o.append(arrow(80, 124, 80, 145))
    o.append(arrow(80, 145, 110, 145))
    o.append(arrow(80, 145, 80, 175))
    o.append(arrow(80, 199, 80, 220))
    o.append(arrow(80, 220, 110, 220))
    # adder
    o.append('<circle cx="320" cy="70" r="16" fill="#eef7ee" stroke="#1b7f3b"/><text x="320" y="75" font-size="16" text-anchor="middle" fill="#1b7f3b">&#931;</text>')
    o.append(arrow(154, 70, 304, 70))
    o.append('<path d="M154 145 L240 145 L240 90 L320 90 L320 86" fill="none" stroke="#586069" stroke-width="1.5"/>' + arrow(320, 98, 320, 87))
    o.append('<path d="M154 220 L270 220 L270 100 L305 82" fill="none" stroke="#586069" stroke-width="1.5"/>')
    # output
    o.append(arrow(336, 70, 560, 70))
    o.append('<text x="565" y="74" font-size="12" fill="#1b1f24">y[n]</text>')
    # feedback side
    o.append(box(470, 130, "&#215; &#8722;a1"))
    o.append(box(470, 205, "&#215; &#8722;a2"))
    o.append(box(520, 100, "z&#8315;&#185;", 40, 24, "#fff8e5"))
    o.append(box(520, 175, "z&#8315;&#185;", 40, 24, "#fff8e5"))
    o.append('<path d="M500 70 L500 60" stroke="none"/>')
    o.append('<path d="M540 70 L540 100" fill="none" stroke="#586069" stroke-width="1.5"/>' + arrow(540, 90, 540, 100))
    o.append(arrow(540, 124, 540, 145))
    o.append(arrow(540, 145, 514, 145))
    o.append(arrow(540, 145, 540, 175))
    o.append(arrow(540, 199, 540, 220))
    o.append(arrow(540, 220, 514, 220))
    o.append('<path d="M470 145 L380 145 L380 92 L336 76" fill="none" stroke="#586069" stroke-width="1.5"/>')
    o.append('<path d="M470 220 L400 220 L400 100 L330 84" fill="none" stroke="#586069" stroke-width="1.5"/>')
    o.append('<text x="60" y="250" font-size="11" fill="#586069">the past INPUTS (left, feed forward)</text>')
    o.append('<text x="430" y="250" font-size="11" fill="#586069">the past OUTPUTS (right, fed back)</text>')
    o.append('</svg>')
    return "\n".join(o)

# ---- Figure: buffers and callbacks over time ----------------------------------------
def callback_figure():
    o = ['<svg viewBox="0 0 700 215" width="100%" role="img" aria-label="Three buffers in flight: playing and refilling over time">']
    x0, bw = 110, 80
    colors = {"A": "#d6e6f7", "B": "#fbe9c8", "C": "#d9f0dc"}
    stroke = {"A": "#0b5cad", "B": "#9a6700", "C": "#1b7f3b"}
    o.append('<text x="10" y="48" font-size="12" font-weight="700" fill="#1b1f24">speakers</text>')
    o.append('<text x="10" y="53" font-size="10" fill="#586069"> </text>')
    names = "ABCABCA"
    for i, n in enumerate(names):
        x = x0 + i * bw
        o.append('<rect x="%d" y="30" width="%d" height="30" fill="%s" stroke="%s"/><text x="%d" y="50" font-size="12" text-anchor="middle" fill="#1b1f24">plays %s</text>' % (x, bw, colors[n], stroke[n], x + bw / 2, n))
    o.append('<text x="10" y="120" font-size="12" font-weight="700" fill="#1b1f24">your fill</text><text x="10" y="134" font-size="12" font-weight="700" fill="#1b1f24">function</text>')
    for i in range(4):
        n = "ABC"[i % 3]
        x = x0 + (i + 1) * bw - 14
        o.append('<rect x="%d" y="105" width="28" height="22" rx="3" fill="%s" stroke="%s"/><text x="%d" y="120" font-size="11" text-anchor="middle" fill="#1b1f24">fill</text>' % (x, colors[n], stroke[n], x + 14))
        # arrow from fill block to the later play of the same buffer, three buffers later
        tx = x0 + (i + 3) * bw + bw / 2 - 20 if i + 3 < len(names) else None
        if tx is not None:
            o.append('<path d="M%d 105 C%d 80, %d 80, %d 62" fill="none" stroke="%s" stroke-width="1.4" stroke-dasharray="3 3"/>' % (x + 14, x + 14, tx, tx, stroke[n]))
    o.append('<line x1="%d" y1="170" x2="%d" y2="170" stroke="#8b949e"/>' % (x0, x0 + 7 * bw))
    for i in range(8):
        o.append('<line x1="%d" y1="166" x2="%d" y2="174" stroke="#8b949e"/>' % (x0 + i * bw, x0 + i * bw))
    o.append('<text x="%d" y="190" font-size="11" fill="#586069">time: each buffer lasts 2,048 frames = 46 ms at 44,100 Hz</text>' % x0)
    o.append('<text x="%d" y="206" font-size="11" fill="#586069">a buffer finishing triggers the callback; the refilled buffer plays three slots later (about 139 ms ahead)</text>' % x0)
    o.append('</svg>')
    return "\n".join(o)


# ---- Figure: a reduction inside one workgroup (8 threads shown; the real group has 256) ---------------------
def reduce_figure():
    o = ['<svg viewBox="0 0 700 270" width="100%" role="img" aria-label="Adding eight numbers inside a workgroup">']
    lanes = 8
    x0, dx = 215, 62
    rounds = [(("start:", "each thread copies its", "number into tile[lane]"), None),
              (("round 1 (s = 4):", "lane l adds tile[l + 4]", ""), 4),
              (("round 2 (s = 2):", "lane l adds tile[l + 2]", ""), 2),
              (("round 3 (s = 1):", "lane l adds tile[l + 1]", ""), 1)]
    ys = [40, 105, 170, 235]
    colors = ["#586069", "#0b5cad", "#9a6700", "#1b7f3b"]
    def box(x, y, text, fill, stroke):
        return '<rect x="%d" y="%d" width="46" height="26" rx="4" fill="%s" stroke="%s"/><text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#1b1f24">%s</text>' % (x - 23, y - 13, fill, stroke, x, y + 4, text)
    for r, (label, step) in enumerate(rounds):
        y = ys[r]
        for line, text in enumerate(label):
            o.append('<text x="8" y="%d" font-size="11" fill="%s">%s</text>' % (y - 9 + 14 * line, colors[r], text))
        for lane in range(lanes):
            active = step is None or lane < step
            text = "x%d" % lane if r == 0 else ("&#931;" if active else "")
            fill = "#ffffff" if (r > 0 and not active) else "#f6f8fa"
            if r > 0 and not active:
                o.append('<rect x="%d" y="%d" width="46" height="26" rx="4" fill="none" stroke="#d0d7de" stroke-dasharray="3 3"/>' % (x0 + lane * dx - 23, y - 13))
            else:
                o.append(box(x0 + lane * dx, y, text, fill, colors[r]))
        if step is not None:
            for lane in range(step):
                x_from = x0 + (lane + step) * dx
                x_to = x0 + lane * dx
                o.append('<path d="M%d %d C%d %d %d %d %d %d" fill="none" stroke="%s" stroke-width="1.3" marker-end="url(#a%d)"/>' % (x_from, ys[r - 1] + 13, x_from, y - 25, x_to + 14, y - 25, x_to + 6, y - 14, colors[r], r))
    for r in range(1, 4):
        o.append('<defs><marker id="a%d" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6 Z" fill="%s"/></marker></defs>' % (r, colors[r]))
        o.append('<line x1="20" y1="%d" x2="680" y2="%d" stroke="#c0392b" stroke-dasharray="6 4"/><text x="684" y="%d" font-size="10" fill="#c0392b" text-anchor="end">barrier()</text>' % (ys[r] - 38 if r > 0 else 0, ys[r] - 38 if r > 0 else 0, ys[r] - 41))
    o.append('<text x="%d" y="%d" font-size="11" fill="#1b1f24">thread 0 now holds the group total in tile[0]: it writes the partial sum out</text>' % (150, 266))
    o.append('</svg>')
    return "\n".join(o)

# ---- Figure: containers are references (names, one header, the data) -----------------------------------
def containers_figure():
    o = ['<svg viewBox="0 0 700 246" width="100%" role="img" aria-label="Three names for one mutarray, before and after a push">']
    o.append('<defs><marker id="rf" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6 Z" fill="#586069"/></marker></defs>')
    def panel(px, title, values, length, changed, note1, note2):
        out = ['<text x="%d" y="20" font-size="12" font-weight="700" fill="#1b1f24">%s</text>' % (px, title)]
        names = ["a", "b", "bag.items"]
        for i, name in enumerate(names):
            y = 46 + i * 38
            out.append('<rect x="%d" y="%d" width="62" height="26" rx="4" fill="#eef5fc" stroke="#0b5cad"/><text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#1b1f24">%s</text>' % (px, y, px + 31, y + 17, name))
            out.append('<path d="M%d %d L%d %d" stroke="#586069" stroke-width="1.3" fill="none" marker-end="url(#rf)"/>' % (px + 62, y + 13, px + 104, 96 if i == 1 else (84 if i == 0 else 108)))
        # the header: three 8-byte fields
        hx = px + 108
        out.append('<text x="%d" y="46" font-size="10" fill="#586069">header on the heap</text>' % hx)
        fields = [("data", "&#8594;"), ("length", str(length)), ("capacity", str(length))]
        for k, (label, value) in enumerate(fields):
            y = 58 + k * 26
            hot = changed
            fill = "#fff4d6" if hot else "#f6f8fa"
            stroke = "#9a6700" if hot else "#586069"
            out.append('<rect x="%d" y="%d" width="86" height="26" fill="%s" stroke="%s"/><text x="%d" y="%d" font-size="11" fill="#1b1f24">%s</text><text x="%d" y="%d" font-size="11" font-weight="700" text-anchor="end" fill="#1b1f24">%s</text>' % (hx, y, fill, stroke, hx + 6, y + 17, label, hx + 80, y + 17, value))
        # the elements
        ax = px + 224
        out.append('<text x="%d" y="46" font-size="10" fill="#586069">the elements</text>' % ax)
        out.append('<path d="M%d %d L%d %d" stroke="#586069" stroke-width="1.3" fill="none" marker-end="url(#rf)"/>' % (hx + 86, 71, ax - 2, 71))
        for k, v in enumerate(values):
            new = changed and k == len(values) - 1
            fill = "#d9f0dc" if new else "#f6f8fa"
            stroke = "#1b7f3b" if new else "#586069"
            out.append('<rect x="%d" y="58" width="24" height="26" fill="%s" stroke="%s"/><text x="%d" y="75" font-size="11" text-anchor="middle" fill="#1b1f24">%s</text>' % (ax + k * 24, fill, stroke, ax + k * 24 + 12, v))
        out.append('<text x="%d" y="176" font-size="11" fill="#1b1f24">%s</text><text x="%d" y="192" font-size="11" fill="#586069">%s</text>' % (px, note1, px, note2))
        return out
    o += panel(10, "three names, one mutarray", ["5", "6", "9"], 3, False,
               "a, b and bag.items hold the same pointer.", "There is only one header and one list of elements.")
    o += panel(360, "after push(b, 7)", ["5", "6", "9", "7"], 4, True,
               "The push changed the header (and moved the elements).", "a and bag.items see length 4 as well.")
    o.append('<text x="10" y="222" font-size="11" fill="#586069">A mutarray, sparse or mutmap is this pointer. Copying it, passing it or storing it copies the pointer,</text>')
    o.append('<text x="10" y="238" font-size="11" fill="#586069">never the elements; a + b is the way to make a new list.</text>')
    o.append('</svg>')
    return "\n".join(o)


def raster_figure():
    o = ['<svg viewBox="0 0 700 260" width="100%" role="img" aria-label="Interlaced scanning of a television picture">']
    o.append('<rect x="20" y="30" width="300" height="200" fill="#f6f8fa" stroke="#c9d1d9"/>')
    o.append('<text x="170" y="20" font-size="12" font-weight="700" text-anchor="middle" fill="#1b1f24">the beam draws the picture as lines</text>')
    for k in range(10):
        y1 = 40 + 20 * k
        o.append('<line x1="26" y1="%d" x2="314" y2="%d" stroke="#0366d6" stroke-width="1.6"/>' % (y1, y1 + 4))
        o.append('<line x1="26" y1="%d" x2="314" y2="%d" stroke="#d73a49" stroke-width="1.6" stroke-dasharray="5 3"/>' % (y1 + 10, y1 + 14))
        if k < 4:
            o.append('<line x1="314" y1="%d" x2="26" y2="%d" stroke="#8b949e" stroke-width="0.8" stroke-dasharray="1 3"/>' % (y1 + 4, y1 + 10))
    o.append('<text x="340" y="60" font-size="12" font-weight="700" fill="#0366d6">solid blue: field 1 (lines 1, 3, 5 ...)</text>')
    o.append('<text x="340" y="82" font-size="12" font-weight="700" fill="#d73a49">dashed red: field 2 (lines 2, 4, 6 ...)</text>')
    o.append('<text x="340" y="108" font-size="12" fill="#586069">grey dotted: the beam snaps back to the left</text>')
    o.append('<text x="340" y="124" font-size="12" fill="#586069">(horizontal retrace, while the signal is blanked).</text>')
    o.append('<text x="340" y="150" font-size="12" fill="#586069">525 lines in all, drawn as two interleaved fields:</text>')
    o.append('<text x="340" y="166" font-size="12" fill="#586069">60 fields a second, so 30 whole pictures a second.</text>')
    o.append('<text x="340" y="192" font-size="12" fill="#586069">Interlacing refreshes the screen 60 times a second,</text>')
    o.append('<text x="340" y="208" font-size="12" fill="#586069">which the eye sees as steady, for the bandwidth of 30.</text>')
    o.append('<text x="340" y="232" font-size="11" fill="#586069">(10 lines per field are drawn here; a real field has about 262.)</text>')
    o.append('</svg>')
    return "\n".join(o)


def scanline_figure():
    import math
    width, left = 600.0, 50.0
    def X(us):
        return left + us * width / 63.5
    def panel(top, title, colour):
        base = top + 110
        def Y(ire):
            return base - (ire + 40) * 0.72
        o = ['<text x="%d" y="%d" font-size="12" font-weight="700" fill="#1b1f24">%s</text>' % (left, top + 4, title)]
        o.append('<rect x="%d" y="%d" width="%d" height="%d" fill="none" stroke="#c9d1d9"/>' % (left, top + 12, width, 100))
        for ire, name in [(-40, "sync tip"), (0, "blanking"), (100, "white")]:
            o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e1e4e8"/>' % (left, Y(ire), left + width, Y(ire)))
            o.append('<text x="%d" y="%.1f" font-size="10" text-anchor="end" fill="#586069">%s</text>' % (left - 4, Y(ire) + 3, name))
        pts = [(0, 0), (1.5, 0), (1.5, -40), (6.2, -40), (6.2, 0)]
        steps = [7.5, 20, 35, 50, 65, 80, 95, 100]
        edge = 10.9
        seg = (63.5 - edge) / len(steps)
        poly = list(pts)
        if colour:
            for k in range(0, 40):
                us = 7.0 + 2.6 * k / 40.0
                poly.append((us, 20 * math.sin(2 * math.pi * 9 * k / 40.0)))
            poly.append((10.9, 0))
        else:
            poly.append((edge, 0))
        for k, level in enumerate(steps):
            poly.append((edge + seg * k, level))
            wave = colour and k in (1, 2, 3, 4, 5)
            if wave:
                amp = (6, 14, 22, 14, 6)[k - 1]
                for j in range(1, 40):
                    us = edge + seg * k + seg * j / 40.0
                    poly.append((us, level + amp * math.sin(2 * math.pi * 7 * j / 40.0 + k)))
            poly.append((edge + seg * (k + 1), level))
        poly.append((63.5, 0))
        o.append('<polyline fill="none" stroke="#0366d6" stroke-width="1.4" points="%s"/>' % " ".join("%.1f,%.1f" % (X(a), Y(b)) for a, b in poly))
        for us, label in [(3.85, "sync"), (8.5, "back porch")]:
            o.append('<text x="%.1f" y="%d" font-size="9" text-anchor="middle" fill="#586069">%s</text>' % (X(us), top + 124, label))
        o.append('<text x="%.1f" y="%d" font-size="9" text-anchor="middle" fill="#586069">active picture: dark to bright, left to right</text>' % (X(37), top + 124))
        return o
    o = ['<svg viewBox="0 0 700 330" width="100%" role="img" aria-label="One scan line of a monochrome and a colour television signal">']
    o += panel(10, "Black-and-white: the voltage IS the brightness", False)
    o += panel(170, "Colour: the same line, with the subcarrier added (burst on the back porch)", True)
    o.append('<text x="%d" y="324" font-size="10" fill="#586069">One line lasts 63.5 microseconds. The real subcarrier has about 227 cycles per line; far fewer are drawn.</text>' % left)
    o.append('</svg>')
    return "\n".join(o)


def wave_figure():
    import math
    o = ['<svg viewBox="0 0 700 190" width="100%" role="img" aria-label="Frequency, amplitude and phase of a wave">']
    def panel(x0, title, curves, note):
        o.append('<text x="%d" y="18" font-size="12" font-weight="700" fill="#1b1f24">%s</text>' % (x0, title))
        o.append('<rect x="%d" y="28" width="210" height="110" fill="none" stroke="#c9d1d9"/>' % x0)
        o.append('<line x1="%d" y1="83" x2="%d" y2="83" stroke="#e1e4e8"/>' % (x0, x0 + 210))
        for amp, cycles, phase, colour, dash in curves:
            pts = " ".join("%.1f,%.1f" % (x0 + 210 * k / 200.0, 83 - 50 * amp * math.sin(2 * math.pi * cycles * k / 200.0 + phase)) for k in range(201))
            o.append('<polyline fill="none" stroke="%s" stroke-width="1.6"%s points="%s"/>' % (colour, ' stroke-dasharray="5 3"' if dash else "", pts))
        for k, line in enumerate(note):
            o.append('<text x="%d" y="%d" font-size="11" fill="#586069">%s</text>' % (x0, 156 + 14 * k, line))
    panel(10, "frequency: how fast", [(1, 2, 0, "#0366d6", False), (1, 5, 0, "#d73a49", True)],
          ["blue: 2 cycles; red: 5 cycles.", "Same height, faster repeat."])
    panel(245, "amplitude: how big", [(1, 3, 0, "#0366d6", False), (0.4, 3, 0, "#d73a49", True)],
          ["blue: height 1; red: height 0.4.", "Same speed, smaller swing."])
    panel(480, "phase: where it starts", [(1, 3, 0, "#0366d6", False), (1, 3, -math.pi / 2, "#d73a49", True)],
          ["red starts a quarter-cycle later.", "Same speed and size, shifted in time."])
    o.append('</svg>')
    return "\n".join(o)


def modulation_figure():
    import math
    o = ['<svg viewBox="0 0 700 330" width="100%" role="img" aria-label="A slow message riding on a fast carrier">']
    def row(top, title, f, envelope=None):
        o.append('<text x="40" y="%d" font-size="12" font-weight="700" fill="#1b1f24">%s</text>' % (top, title))
        o.append('<rect x="40" y="%d" width="620" height="80" fill="none" stroke="#c9d1d9"/>' % (top + 8))
        o.append('<line x1="40" y1="%d" x2="660" y2="%d" stroke="#e1e4e8"/>' % (top + 48, top + 48))
        pts = " ".join("%.1f,%.1f" % (40 + 620 * k / 600.0, top + 48 - 34 * f(k / 600.0)) for k in range(601))
        o.append('<polyline fill="none" stroke="#0366d6" stroke-width="1.2" points="%s"/>' % pts)
        if envelope:
            for sign in (1, -1):
                e = " ".join("%.1f,%.1f" % (40 + 620 * k / 600.0, top + 48 - sign * 34 * envelope(k / 600.0)) for k in range(601))
                o.append('<polyline fill="none" stroke="#d73a49" stroke-width="1.4" stroke-dasharray="5 3" points="%s"/>' % e)
    message = lambda t: math.sin(2 * math.pi * 2 * t)
    carrier = lambda t: math.sin(2 * math.pi * 40 * t)
    level = lambda t: 0.55 + 0.4 * message(t)
    row(10, "the message (slow): what we want to send", message)
    row(115, "the carrier (fast, constant): it carries nothing yet", carrier)
    row(220, "the carrier's SIZE follows the message (amplitude modulation); dashed red = the message", lambda t: level(t) * carrier(t), level)
    o.append('</svg>')
    return "\n".join(o)


def _svg_box(o, x, y, w, h, lines, fill="#eef5fc", stroke="#0b5cad", bold=True, size=11):
    o.append('<rect x="%d" y="%d" width="%d" height="%d" rx="5" fill="%s" stroke="%s"/>' % (x, y, w, h, fill, stroke))
    top = y + h / 2 - (len(lines) - 1) * 7 + 4
    for k, line in enumerate(lines):
        weight = ' font-weight="700"' if (bold and k == 0) else ""
        o.append('<text x="%.1f" y="%.1f" font-size="%d" text-anchor="middle"%s fill="#1b1f24">%s</text>' % (x + w / 2, top + 14 * k, size, weight, line))


def _svg_arrow(o, x1, y1, x2, y2, colour="#586069", dash=False):
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.4"%s marker-end="url(#arr)"/>' % (x1, y1, x2, y2, colour, ' stroke-dasharray="5 3"' if dash else ""))


def _svg_open(o, width, height, label):
    o.append('<svg viewBox="0 0 %d %d" width="100%%" role="img" aria-label="%s">' % (width, height, label))
    o.append('<defs><marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="M0 0L10 5L0 10z" fill="#586069"/></marker></defs>')


def timeline_figure():
    o = []
    _svg_open(o, 700, 295, "The CPU and the GPU working on a job, with and without waiting")
    def lane(y, name):
        o.append('<text x="12" y="%d" font-size="12" font-weight="700" fill="#1b1f24">%s</text>' % (y + 22, name))
        o.append('<line x1="80" y1="%d" x2="690" y2="%d" stroke="#e1e4e8"/>' % (y + 30, y + 30))
    def bar(x, y, w, text, fill, stroke="#586069"):
        o.append('<rect x="%d" y="%d" width="%d" height="28" rx="4" fill="%s" stroke="%s"/>' % (x, y, w, fill, stroke))
        o.append('<text x="%d" y="%d" font-size="10" text-anchor="middle" fill="#1b1f24">%s</text>' % (x + w / 2, y + 18, text))
    o.append('<text x="80" y="16" font-size="12" font-weight="700" fill="#1b1f24">A. What the polyir runtimes do: send a job, then wait for it</text>')
    lane(24, "CPU")
    lane(64, "GPU")
    bar(80, 28, 90, "write job 1", "#eef5fc", "#0b5cad")
    bar(170, 28, 60, "wait", "#fff8e6", "#9a6700")
    bar(230, 28, 60, "read", "#eef5fc", "#0b5cad")
    bar(290, 28, 90, "write job 2", "#eef5fc", "#0b5cad")
    bar(380, 28, 60, "wait", "#fff8e6", "#9a6700")
    bar(440, 28, 60, "read", "#eef5fc", "#0b5cad")
    bar(170, 68, 60, "run 1", "#e6f4ea", "#1a7f37")
    bar(380, 68, 60, "run 2", "#e6f4ea", "#1a7f37")
    _svg_arrow(o, 170, 56, 175, 68)
    _svg_arrow(o, 380, 56, 385, 68)
    o.append('<text x="80" y="140" font-size="12" font-weight="700" fill="#1b1f24">B. What real engines do: keep both busy (the GPU runs job 1 while the CPU writes job 2)</text>')
    lane(148, "CPU")
    lane(188, "GPU")
    bar(80, 152, 90, "write job 1", "#eef5fc", "#0b5cad")
    bar(170, 152, 90, "write job 2", "#eef5fc", "#0b5cad")
    bar(260, 152, 90, "write job 3", "#eef5fc", "#0b5cad")
    bar(350, 152, 60, "read 1", "#eef5fc", "#0b5cad")
    bar(170, 192, 90, "run 1", "#e6f4ea", "#1a7f37")
    bar(260, 192, 90, "run 2", "#e6f4ea", "#1a7f37")
    bar(350, 192, 90, "run 3", "#e6f4ea", "#1a7f37")
    _svg_arrow(o, 170, 180, 175, 192)
    o.append('<text x="80" y="250" font-size="11" fill="#586069">Time runs to the right. In A the CPU idles while the GPU works, and the GPU idles while the CPU writes.</text>')
    o.append('<text x="80" y="266" font-size="11" fill="#586069">In B the work overlaps, so the total is shorter. The price of B is that the CPU must keep track of</text>')
    o.append('<text x="80" y="282" font-size="11" fill="#586069">which job has finished before it reads or reuses anything.</text>')
    o.append('</svg>')
    return "\n".join(o)


def mchip_figure():
    o = []
    _svg_open(o, 700, 300, "An Apple M-series chip shares one memory; a PC with a graphics card has two")
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">Apple M-series: one chip, one pool of memory</text>')
    o.append('<rect x="20" y="28" width="320" height="170" rx="8" fill="#f6f8fa" stroke="#586069"/>')
    o.append('<text x="30" y="46" font-size="10" fill="#586069">system on a chip</text>')
    _svg_box(o, 34, 54, 90, 44, ["CPU cores", "fast + efficient"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 134, 54, 90, 44, ["GPU cores", "your kernels"], "#e6f4ea", "#1a7f37", True, 10)
    _svg_box(o, 234, 54, 92, 44, ["Neural Engine", "media engines"], "#fff8e6", "#9a6700", True, 10)
    _svg_box(o, 34, 130, 292, 40, ["unified memory (one pool)"], "#fdf2f2", "#d73a49", True, 11)
    for x in (79, 179, 280):
        _svg_arrow(o, x, 98, x, 130)
    o.append('<text x="30" y="190" font-size="10" fill="#586069">everything reads and writes the same bytes: no copy needed</text>')
    o.append('<text x="360" y="18" font-size="12" font-weight="700" fill="#1b1f24">PC with a separate graphics card: two pools</text>')
    _svg_box(o, 370, 54, 90, 44, ["CPU", "+ main memory"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 590, 54, 90, 44, ["GPU", "+ its own memory"], "#e6f4ea", "#1a7f37", True, 10)
    _svg_box(o, 370, 130, 90, 40, ["RAM"], "#fdf2f2", "#d73a49", True, 11)
    _svg_box(o, 590, 130, 90, 40, ["VRAM"], "#fdf2f2", "#d73a49", True, 11)
    _svg_arrow(o, 415, 98, 415, 130)
    _svg_arrow(o, 635, 98, 635, 130)
    o.append('<line x1="460" y1="150" x2="590" y2="150" stroke="#9a6700" stroke-width="2" stroke-dasharray="6 3" marker-end="url(#arr)"/>')
    o.append('<text x="525" y="144" font-size="10" text-anchor="middle" fill="#9a6700">bus: data is copied</text>')
    o.append('<text x="360" y="190" font-size="10" fill="#586069">inputs are copied across before the kernel; results copied back after</text>')
    o.append('<text x="20" y="232" font-size="11" fill="#586069">This is why the Metal calls in this guide can read results straight out of a buffer. A kernel still has to be started by a command</text>')
    o.append('<text x="20" y="248" font-size="11" fill="#586069">(section 5.6), but the data is already where both sides can see it. The speed of that shared memory still limits memory-bound kernels.</text>')
    o.append('</svg>')
    return "\n".join(o)


def apistack_figure():
    o = []
    _svg_open(o, 700, 330, "The layers between an polyir program and the GPU on a Mac")
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">On a Mac (this machine)</text>')
    _svg_box(o, 20, 30, 150, 36, ["your polyir program"], "#f6f8fa", "#586069", True, 11)
    _svg_box(o, 20, 90, 150, 44, ["Metal.poly", "(Objective-C calls)"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 220, 90, 150, 44, ["Vulkan.poly", "(C function calls)"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 220, 150, 150, 36, ["Vulkan loader"], "#f6f8fa", "#586069", True, 10)
    _svg_box(o, 220, 204, 150, 36, ["MoltenVK"], "#fff8e6", "#9a6700", True, 10)
    _svg_box(o, 20, 258, 350, 36, ["Metal (Apple's API and driver)"], "#eef5fc", "#0b5cad", True, 11)
    _svg_box(o, 20, 306, 350, 20, ["the GPU in the M chip"], "#e6f4ea", "#1a7f37", True, 10)
    _svg_arrow(o, 95, 66, 95, 90)
    o.append('<polyline points="170,48 295,48 295,88" fill="none" stroke="#586069" stroke-width="1.4" marker-end="url(#arr)"/>')
    _svg_arrow(o, 295, 134, 295, 150)
    _svg_arrow(o, 295, 186, 295, 204)
    _svg_arrow(o, 295, 240, 295, 258)
    _svg_arrow(o, 95, 134, 95, 258)
    _svg_arrow(o, 195, 294, 195, 306)
    o.append('<text x="400" y="18" font-size="12" font-weight="700" fill="#1b1f24">On Windows, Linux, Android (for comparison)</text>')
    _svg_box(o, 400, 30, 150, 36, ["your program"], "#f6f8fa", "#586069", True, 11)
    _svg_box(o, 400, 90, 150, 44, ["Vulkan", "(same calls)"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 400, 150, 150, 36, ["Vulkan loader"], "#f6f8fa", "#586069", True, 10)
    _svg_box(o, 400, 204, 150, 44, ["the GPU maker's driver", "(NVIDIA, AMD, Intel ...)"], "#fff8e6", "#9a6700", True, 10)
    _svg_box(o, 400, 266, 150, 36, ["the GPU"], "#e6f4ea", "#1a7f37", True, 11)
    _svg_arrow(o, 475, 66, 475, 90)
    _svg_arrow(o, 475, 134, 475, 150)
    _svg_arrow(o, 475, 186, 475, 204)
    _svg_arrow(o, 475, 248, 475, 266)
    o.append('<text x="575" y="110" font-size="10" fill="#586069">Vulkan is a standard that the</text>')
    o.append('<text x="575" y="124" font-size="10" fill="#586069">GPU makers implement.</text>')
    o.append('<text x="575" y="224" font-size="10" fill="#586069">Apple has not made a Vulkan</text>')
    o.append('<text x="575" y="238" font-size="10" fill="#586069">driver; MoltenVK translates.</text>')
    o.append('</svg>')
    return "\n".join(o)


def vkobjects_figure():
    o = []
    _svg_open(o, 700, 300, "The Vulkan objects a compute launch needs, and what makes what")
    _svg_box(o, 20, 20, 110, 40, ["Instance", "vulkanInstance"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 160, 20, 130, 40, ["Physical device", "the GPU itself"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 320, 20, 130, 40, ["Logical device", "your handle to it"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 480, 20, 100, 40, ["Queue", "the in-tray"], "#fff8e6", "#9a6700", True, 10)
    _svg_box(o, 600, 20, 80, 40, ["Command", "pool"], "#eef5fc", "#0b5cad", True, 10)
    _svg_arrow(o, 130, 40, 160, 40)
    _svg_arrow(o, 290, 40, 320, 40)
    _svg_arrow(o, 450, 40, 480, 40)
    _svg_arrow(o, 580, 40, 600, 40)
    o.append('<text x="20" y="92" font-size="11" fill="#586069">Set up once per program (vulkanStart)</text>')
    _svg_box(o, 20, 110, 130, 44, ["Buffer", "the array's handle"], "#e6f4ea", "#1a7f37", True, 10)
    _svg_box(o, 20, 170, 130, 44, ["Device memory", "the bytes (mapped)"], "#fdf2f2", "#d73a49", True, 10)
    _svg_arrow(o, 85, 170, 85, 154)
    _svg_box(o, 190, 110, 130, 44, ["Shader module", "your SPIR-V"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 190, 170, 130, 44, ["Set layout +", "pipeline layout"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 350, 140, 130, 44, ["Compute pipeline", "the prepared kernel"], "#e6f4ea", "#1a7f37", True, 10)
    _svg_arrow(o, 320, 132, 350, 155)
    _svg_arrow(o, 320, 192, 350, 172)
    o.append('<text x="20" y="246" font-size="11" fill="#586069">Made once per kernel / once per buffer</text>')
    _svg_box(o, 520, 110, 160, 44, ["Descriptor set", "binding i = i-th buffer"], "#eef5fc", "#0b5cad", True, 10)
    _svg_box(o, 520, 170, 160, 44, ["Command buffer", "bind, push, dispatch"], "#eef5fc", "#0b5cad", True, 10)
    _svg_arrow(o, 600, 154, 600, 170)
    _svg_box(o, 520, 232, 160, 40, ["vkQueueSubmit, then wait"], "#fff8e6", "#9a6700", True, 10)
    _svg_arrow(o, 600, 214, 600, 232)
    o.append('<text x="345" y="236" font-size="11" fill="#586069">Made again for every launch</text>')
    o.append('<text x="345" y="252" font-size="11" fill="#586069">in polyir (an engine reuses them)</text>')
    o.append('<text x="20" y="288" font-size="11" fill="#586069">Metal needs far fewer objects: device, library/pipeline, buffers, one queue, and a command buffer with an encoder per launch.</text>')
    o.append('</svg>')
    return "\n".join(o)


def _camera_icon(o, x, y, scale=1.0, angle=0):
    o.append('<g transform="translate(%d,%d) rotate(%d) scale(%.2f)">' % (x, y, angle, scale))
    o.append('<rect x="-14" y="-9" width="28" height="18" rx="3" fill="#eef5fc" stroke="#0b5cad" stroke-width="1.4"/>')
    o.append('<polygon points="14,-6 24,-11 24,11 14,6" fill="#e6f4ea" stroke="#1a7f37" stroke-width="1.4"/>')
    o.append('</g>')


def pinhole_figure():
    o = []
    _svg_open(o, 700, 270, "A real pinhole camera and the virtual camera")
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">A real pinhole camera: the picture is upside-down</text>')
    o.append('<line x1="40" y1="190" x2="40" y2="70" stroke="#d73a49" stroke-width="3" marker-end="url(#arr)"/>')
    o.append('<text x="40" y="212" font-size="11" text-anchor="middle" fill="#586069">the toy</text>')
    o.append('<rect x="200" y="80" width="130" height="100" fill="#f6f8fa" stroke="#586069" stroke-width="1.6"/>')
    o.append('<rect x="198" y="126" width="5" height="8" fill="#fff"/>')
    o.append('<line x1="40" y1="70" x2="330" y2="178.75" stroke="#9a6700" stroke-width="1.2"/>')
    o.append('<line x1="40" y1="190" x2="330" y2="81.25" stroke="#9a6700" stroke-width="1.2"/>')
    o.append('<line x1="326" y1="81" x2="326" y2="178" stroke="#d73a49" stroke-width="3" marker-end="url(#arr)"/>')
    o.append('<text x="200" y="200" font-size="11" text-anchor="middle" fill="#586069">a tiny hole</text>')
    o.append('<text x="265" y="72" font-size="11" text-anchor="middle" fill="#586069">dark box</text>')
    o.append('<text x="326" y="198" font-size="11" text-anchor="middle" fill="#586069">its picture,</text>')
    o.append('<text x="326" y="212" font-size="11" text-anchor="middle" fill="#586069">upside-down</text>')
    o.append('<text x="380" y="18" font-size="12" font-weight="700" fill="#1b1f24">The virtual camera: the picture plane is put in front</text>')
    o.append('<circle cx="400" cy="130" r="4" fill="#0b5cad"/>')
    o.append('<text x="400" y="152" font-size="11" text-anchor="middle" fill="#586069">the eye</text>')
    o.append('<line x1="470" y1="70" x2="470" y2="190" stroke="#0b5cad" stroke-width="2"/>')
    o.append('<text x="470" y="210" font-size="11" text-anchor="middle" fill="#0b5cad">picture plane</text>')
    o.append('<text x="470" y="224" font-size="11" text-anchor="middle" fill="#0b5cad">("the window")</text>')
    o.append('<line x1="660" y1="190" x2="660" y2="70" stroke="#d73a49" stroke-width="3" marker-end="url(#arr)"/>')
    o.append('<text x="660" y="212" font-size="11" text-anchor="middle" fill="#586069">the toy</text>')
    o.append('<line x1="400" y1="130" x2="660" y2="70" stroke="#9a6700" stroke-width="1.2"/>')
    o.append('<line x1="400" y1="130" x2="660" y2="190" stroke="#9a6700" stroke-width="1.2"/>')
    o.append('<line x1="466" y1="146.8" x2="466" y2="113.2" stroke="#d73a49" stroke-width="3" marker-end="url(#arr)" transform="translate(0,0)"/>')
    o.append('<text x="20" y="254" font-size="11" fill="#586069">Light travels in straight lines. Each point of the toy sends one ray through the hole, so each point lands on one spot of the picture.</text>')
    o.append('<text x="20" y="268" font-size="11" fill="#586069">The virtual camera uses the same rays but puts the picture plane between the eye and the toy, which makes the picture the right way up.</text>')
    o.append('</svg>')
    return "\n".join(o)


def eyeray_figure():
    o = []
    _svg_open(o, 700, 250, "One ray from the eye, many things that fit it")
    o.append('<line x1="60" y1="150" x2="680" y2="150" stroke="#d0d7de" stroke-width="1" stroke-dasharray="4 3"/>')
    o.append('<circle cx="60" cy="150" r="5" fill="#0b5cad"/>')
    o.append('<text x="60" y="176" font-size="11" text-anchor="middle" fill="#586069">the eye</text>')
    o.append('<line x1="200" y1="90" x2="200" y2="210" stroke="#0b5cad" stroke-width="2"/>')
    o.append('<text x="200" y="228" font-size="11" text-anchor="middle" fill="#0b5cad">the window</text>')
    o.append('<line x1="60" y1="150" x2="610" y2="95" stroke="#9a6700" stroke-width="1.2"/>')
    o.append('<circle cx="200" cy="136" r="4" fill="#0b5cad"/>')
    for x, h, lab, sub in [(300, 24, "near and small", "0.9 m at 10 m"), (450, 39, "middle", "1.35 m at 15 m"), (600, 54, "far and big", "1.8 m at 20 m")]:
        o.append('<line x1="%d" y1="150" x2="%d" y2="%d" stroke="#d73a49" stroke-width="3" marker-end="url(#arr)"/>' % (x, x, 150 - h + 4))
        o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#586069">%s</text>' % (x, 176, lab))
        o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#586069">%s</text>' % (x, 190, sub))
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">Three different people, one identical picture</text>')
    o.append('<text x="20" y="38" font-size="11" fill="#586069">All three tops lie on the same ray, so they hit the same spot of the window. A single eye cannot tell them apart.</text>')
    o.append('<text x="20" y="56" font-size="11" fill="#586069">What the picture holds is the direction of each ray, not the distance along it.</text>')
    o.append('</svg>')
    return "\n".join(o)


def depthz_figure():
    o = []
    _svg_open(o, 700, 320, "Depth and the z coordinate, seen from above")
    cx, cy = 350, 270
    o.append('<polygon points="%d,%d %d,%d %d,%d" fill="#eef5fc" stroke="none"/>' % (cx, cy, cx - 170, 30, cx + 170, 30))
    _camera_icon(o, cx, cy, 1.0, -90)
    o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#586069">camera at z = 0</text>' % (cx, cy + 30))
    for m in (0, 5, 10):
        y = cy - 20 * m - (0 if m else 0)
        o.append('<line x1="150" y1="%d" x2="550" y2="%d" stroke="#d0d7de" stroke-dasharray="4 3"/>' % (y, y))
        o.append('<text x="140" y="%d" font-size="12" text-anchor="end" fill="#1b1f24">z = %s</text>' % (y + 4, "0" if m == 0 else "&minus;%d" % m))
        o.append('<text x="560" y="%d" font-size="12" fill="#1b1f24">depth = %d</text>' % (y + 4, m))
    for m, nm in ((5, "toy B"), (10, "toy A")):
        o.append('<circle cx="%d" cy="%d" r="7" fill="#d73a49"/>' % (cx, cy - 20 * m))
        o.append('<text x="%d" y="%d" font-size="11" fill="#586069">%s</text>' % (cx + 14, cy - 20 * m + 4, nm))
    o.append('<line x1="40" y1="60" x2="40" y2="260" stroke="#0b5cad" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="40" y="276" font-size="11" text-anchor="middle" fill="#0b5cad">z grows</text>')
    o.append('<text x="40" y="290" font-size="11" text-anchor="middle" fill="#0b5cad">(toward you)</text>')
    o.append('<line x1="660" y1="260" x2="660" y2="60" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="660" y="276" font-size="11" text-anchor="middle" fill="#1a7f37">depth grows</text>')
    o.append('<text x="660" y="290" font-size="11" text-anchor="middle" fill="#1a7f37">(away from you)</text>')
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">Right-handed room: the camera looks toward smaller z, so depth = camera z &minus; point z</text>')
    o.append('</svg>')
    return "\n".join(o)


def turn_figure():
    o = []
    _svg_open(o, 700, 270, "Pan (yaw) seen from above and tilt (pitch) seen from the side")
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">A pan, from above: the camera turns on the spot</text>')
    cx, cy = 130, 205
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#586069" stroke-width="1.4" stroke-dasharray="5 3" marker-end="url(#arr)"/>' % (cx, cy - 20, cx, 62))
    o.append('<text x="%d" y="52" font-size="10" text-anchor="middle" fill="#586069">yaw 0: looks along -z (the back wall)</text>' % cx)
    a = math.radians(35)
    ex, ey = cx + 150 * math.sin(a), cy - 150 * math.cos(a)
    o.append('<line x1="%d" y1="%d" x2="%.0f" y2="%.0f" stroke="#9a6700" stroke-width="2.2" marker-end="url(#arr)"/>' % (cx, cy, ex, ey))
    o.append('<path d="M %d %d A 70 70 0 0 1 %.0f %.0f" fill="none" stroke="#9a6700" stroke-width="1.4"/>' % (cx, cy - 70, cx + 70 * math.sin(a), cy - 70 * math.cos(a)))
    o.append('<text x="%.0f" y="%.0f" font-size="10" fill="#9a6700">yaw +35&#176;: now looks toward +x</text>' % (ex - 20, ey - 8))
    _camera_icon(o, cx, cy, 1.1, -90 + 35)
    o.append('<text x="%d" y="%d" font-size="10" text-anchor="middle" fill="#586069">the camera does not move,</text>' % (cx, cy + 34))
    o.append('<text x="%d" y="%d" font-size="10" text-anchor="middle" fill="#586069">it only swings round</text>' % (cx, cy + 47))
    o.append('<line x1="250" y1="240" x2="300" y2="240" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="306" y="244" font-size="10" fill="#1a7f37">+x (right)</text>')
    o.append('<text x="380" y="18" font-size="12" font-weight="700" fill="#1b1f24">A tilt, from the side: the camera nods</text>')
    bx, by = 430, 190
    o.append('<line x1="380" y1="%d" x2="680" y2="%d" stroke="#d0d7de" stroke-dasharray="3 3"/>' % (by + 20, by + 20))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#586069" stroke-width="1.4" stroke-dasharray="5 3" marker-end="url(#arr)"/>' % (bx + 20, by, bx + 190, by))
    o.append('<text x="%d" y="%d" font-size="10" text-anchor="end" fill="#586069">pitch 0: level, at the horizon</text>' % (bx + 235, by + 14))
    b = math.radians(30)
    fx, fy = bx + 150 * math.cos(b), by - 150 * math.sin(b)
    o.append('<line x1="%d" y1="%d" x2="%.0f" y2="%.0f" stroke="#9a6700" stroke-width="2.2" marker-end="url(#arr)"/>' % (bx, by, fx, fy))
    o.append('<path d="M %d %d A 80 80 0 0 0 %.0f %.0f" fill="none" stroke="#9a6700" stroke-width="1.4"/>' % (bx + 80, by, bx + 80 * math.cos(b), by - 80 * math.sin(b)))
    o.append('<text x="%.0f" y="%.0f" font-size="10" fill="#9a6700">pitch +30&#176;: looks up</text>' % (fx - 60, fy - 8))
    _camera_icon(o, bx, by, 1.1, -30)
    o.append('<line x1="660" y1="250" x2="660" y2="215" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="650" y="240" font-size="10" text-anchor="end" fill="#1a7f37">+y (up)</text>')
    o.append('</svg>')
    return "\n".join(o)


def topview_figure():
    o = []
    _svg_open(o, 700, 330, "The camera seen from above: right, ahead and the field of view")
    cx, cy = 300, 290
    o.append('<polygon points="%d,%d %d,%d %d,%d" fill="#eef5fc" stroke="none"/>' % (cx, cy, cx - 135, 40, cx + 135, 40))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#0b5cad" stroke-width="1.5" stroke-dasharray="6 3"/>' % (cx, cy, cx - 135, 40))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#0b5cad" stroke-width="1.5" stroke-dasharray="6 3"/>' % (cx, cy, cx + 135, 40))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#586069" stroke-width="1" stroke-dasharray="2 4"/>' % (cx, cy, cx, 40))
    _camera_icon(o, cx, cy + 4, 1.0, -90)
    o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#586069">the camera</text>' % (cx, cy + 30))
    o.append('<text x="%d" y="30" font-size="11" text-anchor="middle" fill="#0b5cad">what it can see (the field of view)</text>' % cx)
    px, py = 400, 120
    o.append('<circle cx="%d" cy="%d" r="6" fill="#d73a49"/>' % (px, py))
    o.append('<text x="%d" y="%d" font-size="12" font-weight="700" fill="#d73a49">P</text>' % (px + 10, py + 4))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#d73a49" stroke-width="1.5" marker-end="url(#arr)"/>' % (cx, py, px - 8, py))
    o.append('<text x="%d" y="%d" font-size="11" text-anchor="middle" fill="#d73a49">right</text>' % ((cx + px) / 2, py - 6))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#1a7f37" stroke-width="1.5" marker-end="url(#arr)"/>' % (cx - 12, cy - 20, cx - 12, py + 4))
    o.append('<text x="%d" y="%d" font-size="11" text-anchor="end" fill="#1a7f37">ahead (the distance in front)</text>' % (cx - 30, (cy + py) / 2))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#9a6700" stroke-width="1.2" stroke-dasharray="3 3"/>' % (cx, cy, px, py))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#0b5cad" stroke-width="2.5"/>' % (cx - 40, cy - 40, cx + 40, cy - 40))
    o.append('<text x="%d" y="%d" font-size="10" fill="#0b5cad">picture plane</text>' % (cx + 46, cy - 36))
    o.append('<text x="470" y="52" font-size="11" fill="#586069">Seen from above, with the camera at the</text>')
    o.append('<text x="470" y="68" font-size="11" fill="#586069">bottom looking up the page. The dashed</text>')
    o.append('<text x="470" y="84" font-size="11" fill="#586069">blue lines are the edges of what it sees;</text>')
    o.append('<text x="470" y="100" font-size="11" fill="#586069">each makes half the field of view with</text>')
    o.append('<text x="470" y="116" font-size="11" fill="#586069">the dotted middle line.</text>')
    o.append('<text x="470" y="160" font-size="11" fill="#d73a49">right: how far to its right P is (x)</text>')
    o.append('<text x="470" y="180" font-size="11" fill="#1a7f37">ahead: how far in front of it P is (depth)</text>')
    o.append('<text x="470" y="220" font-size="11" fill="#586069">Drawn from the side, the same picture would</text>')
    o.append('<text x="470" y="236" font-size="11" fill="#586069">show "above" (y) against "ahead" in just the</text>')
    o.append('<text x="470" y="252" font-size="11" fill="#586069">same way: three numbers, three directions.</text>')
    o.append('<text x="20" y="324" font-size="10" fill="#586069">Not to scale.</text>')
    o.append('</svg>')
    return "\n".join(o)


def steps_figure():
    o = []
    _svg_open(o, 700, 230, "The four steps from a point in the world to a pixel, with the numbers of the worked example")
    xs = [10, 150, 290, 430, 570]
    heads = ["the world", "step 1", "step 2", "step 3", "step 4"]
    names = ["a point in the world", "relative to the camera", "in the camera's terms", "on the picture plane", "a pixel"]
    nums = ["(2, 1, -10)", "(2, -0.5, -10)", "right 2, above -0.5, ahead 10", "(0.195, -0.087)", "(1147.1, 586.8)"]
    fills = ["#f6f8fa", "#eef5fc", "#eef5fc", "#eef5fc", "#e6f4ea"]
    strokes = ["#586069", "#0b5cad", "#0b5cad", "#0b5cad", "#1a7f37"]
    for k in range(5):
        _svg_box(o, xs[k], 30, 120, 62, [names[k]], fills[k], strokes[k], True, 10)
        o.append('<text x="%d" y="%d" font-size="10" text-anchor="middle" fill="#1b1f24">%s</text>' % (xs[k] + 60, 82, nums[k]))
    for k in range(4):
        _svg_arrow(o, xs[k] + 120, 61, xs[k + 1], 61)
    lines1 = ["subtract the camera's", "dot products with its", "divide by ahead and", "scale to pixels;"]
    lines2 = ["position (walk to it)", "right, up and forward", "by the lens: far = small", "row 0 is the top"]
    for k in range(4):
        o.append('<text x="%d" y="115" font-size="10" text-anchor="middle" font-weight="700" fill="#0b5cad">step %d</text>' % (xs[k + 1] - 10, k + 1))
        o.append('<text x="%d" y="132" font-size="10" text-anchor="middle" fill="#1b1f24">%s</text>' % (xs[k + 1] - 10, lines1[k]))
        o.append('<text x="%d" y="146" font-size="10" text-anchor="middle" fill="#1b1f24">%s</text>' % (xs[k + 1] - 10, lines2[k]))
    o.append('<text x="20" y="190" font-size="11" fill="#586069">The point (2, 1, -10) is seen by a camera at (0, 1.5, 0) looking down -z, with a 60 degree lens, on a 1920 x 1080 picture.</text>')
    o.append('<text x="20" y="206" font-size="11" fill="#586069">Step 4 gives column 1147 (a little right of the middle, 960) and row 587 (a little below the middle, 540).</text>')
    o.append('</svg>')
    return "\n".join(o)


def moves_figure():
    o = []
    _svg_open(o, 700, 280, "The camera moves of a film crew")
    o.append('<text x="20" y="18" font-size="12" font-weight="700" fill="#1b1f24">From above</text>')
    _camera_icon(o, 100, 170, 1.1, -90)
    o.append('<line x1="100" y1="140" x2="100" y2="60" stroke="#d73a49" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<line x1="100" y1="200" x2="100" y2="240" stroke="#d73a49" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="108" y="64" font-size="10" fill="#d73a49">dolly in</text>')
    o.append('<text x="108" y="246" font-size="10" fill="#d73a49">dolly out</text>')
    o.append('<line x1="70" y1="170" x2="20" y2="170" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<line x1="130" y1="170" x2="180" y2="170" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="20" y="188" font-size="10" fill="#1a7f37">truck left</text>')
    o.append('<text x="132" y="188" font-size="10" fill="#1a7f37">truck right</text>')
    o.append('<path d="M 72 124 A 40 40 0 0 1 128 124" fill="none" stroke="#9a6700" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="136" y="112" font-size="10" fill="#9a6700">pan</text>')
    o.append('<text x="250" y="18" font-size="12" font-weight="700" fill="#1b1f24">From the side</text>')
    _camera_icon(o, 330, 150, 1.1, 0)
    o.append('<line x1="330" y1="125" x2="330" y2="60" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<line x1="330" y1="175" x2="330" y2="235" stroke="#1a7f37" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="338" y="68" font-size="10" fill="#1a7f37">pedestal up (boom)</text>')
    o.append('<text x="338" y="232" font-size="10" fill="#1a7f37">pedestal down</text>')
    o.append('<path d="M 372 120 A 50 50 0 0 0 372 180" fill="none" stroke="#9a6700" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="382" y="154" font-size="10" fill="#9a6700">tilt</text>')
    o.append('<text x="500" y="18" font-size="12" font-weight="700" fill="#1b1f24">From behind the camera</text>')
    o.append('<rect x="560" y="120" width="60" height="45" rx="3" fill="#eef5fc" stroke="#0b5cad" stroke-width="1.4"/>')
    o.append('<circle cx="590" cy="142" r="12" fill="#e6f4ea" stroke="#1a7f37" stroke-width="1.4"/>')
    o.append('<path d="M 545 118 A 55 55 0 0 1 635 118" fill="none" stroke="#9a6700" stroke-width="2" marker-end="url(#arr)"/>')
    o.append('<text x="590" y="74" font-size="10" text-anchor="middle" fill="#9a6700">roll (a tilted horizon,</text>')
    o.append('<text x="590" y="88" font-size="10" text-anchor="middle" fill="#9a6700">the "Dutch angle")</text>')
    o.append('<text x="500" y="200" font-size="11" fill="#586069">Zoom is not a move: the camera stays</text>')
    o.append('<text x="500" y="214" font-size="11" fill="#586069">where it is and the LENS changes.</text>')
    o.append('<text x="20" y="272" font-size="10" fill="#586069">Red: along the camera&#39;s forward axis. Green: along its other two axes. Gold: turning about an axis.</text>')
    o.append('</svg>')
    return "\n".join(o)


for name, f in [("pan", pan_figure), ("sinc", sinc_figure), ("biquad", biquad_figure), ("callback", callback_figure), ("reduce", reduce_figure), ("containers", containers_figure), ("raster", raster_figure), ("scanline", scanline_figure), ("wave", wave_figure), ("modulation", modulation_figure), ("timeline", timeline_figure), ("mchip", mchip_figure), ("apistack", apistack_figure), ("vkobjects", vkobjects_figure), ("pinhole", pinhole_figure), ("eyeray", eyeray_figure), ("depthz", depthz_figure), ("turn", turn_figure), ("topview", topview_figure), ("steps", steps_figure), ("moves", moves_figure)]:
    open(os.path.join(OUT, name + ".svg"), "w").write(f())
print("ok")
