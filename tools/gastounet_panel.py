#!/usr/bin/env python3
"""Disposition du panneau de Gastounet (20 HP), l'expander de mémoires de Gaston, collé à sa gauche.

Comme tools/gaston_panel.py, source unique des coordonnées (mm) : ce script écrit
  - res/Gastounet.svg            le fond statique ;
  - src/GastounetLayout.hpp      les positions des composants et les textes fixes, lus par src/Gastounet.cpp ;
  - tools/preview/Gastounet.png  avec --preview : fond + composants et textes simulés, et chevauchements détectés.

Usage : python3 tools/gastounet_panel.py [--preview]
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 101.6, 128.5

LACQ_TOP, LACQ_BOT = "#17223b", "#0e1628"
BRASS, LABEL, LABEL_DIM = "#c8a25c", "#d6c9aa", "#b3a687"
GLASS, GLASS_EDGE, INK = "#060a12", "#5a4c33", "#141a2a"

X0, X1 = 4.0, 97.6

SCREEN = (X0, 12.6, 78.0, 42.6)
EDIT_BOX = (80.0, 12.6, X1, 42.6)
EDIT_X, EDIT_Y, EDIT_W, EDIT_H = 88.8, [22.0, 29.4, 36.8], 15.0, 5.4
ROW_Y0, ROW_PITCH, ROWS = 24.4, 4.1, 4
MODE_Y, MODE_W, MODE_H = 47.6, 21.6, 5.6
MODE_X = [X0 + 1.0 + MODE_W / 2 + k * 23.2 for k in range(4)]
MEM_BOX = (X0, 52.4, X1, 95.0)
BANK_X, BANK_W, BANK_H = 9.8, 6.8, 7.6
ROW_Y = [62.0, 70.8, 79.6, 88.4]
SEP1_X, SEP2_X = 15.4, 56.0
SLOT_X, SLOT_W, SLOT_H = [21.0, 30.2, 39.4, 48.6], 8.2, 7.6
ACT_X, ACT_Y, ACT_R = [67.0, 87.0], [64.6, 81.8], 4.4
LAUNCH_BOX = (X0, 97.0, X1, 108.0)
SEG_Y, SEG_H = 104.0, 4.0
QUANT_X0, QUANT_W = 6.4, 10.6
CHAIN_W = 12.0
CHAIN_X0 = 95.4 - 2 * CHAIN_W
IN_BOX = (X0, 110.0, 78.6, 123.0)
IN_X, JACK_Y = [13.4, 32.0, 50.6, 69.2], 118.0
END_PLATE = (80.6, 110.0, X1, 123.0)
END_X = (END_PLATE[0] + END_PLATE[2]) / 2
JACK_LABEL_Y = 112.6

LABELS = []


def label(x, y, text, size=5.0, color=LABEL, align="center", spacing=0.0):
    LABELS.append((x, y, text, size, color, align, spacing))


label(11.0, 7.2, "GASTOUNET", 15.0, "#d4ae66", "left", 1.0)
label(90.6, 7.2, "PATTERNS · SONG", 5.8, LABEL, "right", 0.6)
label(W / 2, 125.7, "BASIC HUMAN TASTES", 4.6, "#a39780", "center", 1.2)
label(7.0, 99.4, "LAUNCH", 6.4, LABEL_DIM, "left", 0.9)
label(82.4, 15.6, "ROWS", 5.6, LABEL_DIM, "left", 0.9)
for k, n in enumerate(["WRITE", "COPY", "NEXT", "RANDOM"]):
    label(ACT_X[k % 2], ACT_Y[k // 2] + 6.8, n, 6.2, LABEL, "center", 0.5)
for x, n in zip(IN_X, ["PATTERN", "NEXT", "RANDOM", "RESET"]):
    label(x, JACK_LABEL_Y, n, 6.2)
label(END_X, JACK_LABEL_Y, "END", 6.4, INK)

out = []


def rect(box, fill, stroke=None, sw=0.25, rx=1.0, fill_opacity=1.0, stroke_opacity=1.0):
    x0, y0, x1, y1 = box
    s = ' stroke="%s" stroke-width="%.2f" stroke-opacity="%.2f"' % (stroke, sw, stroke_opacity) if stroke else ""
    out.append('<rect x="%.3f" y="%.3f" width="%.3f" height="%.3f" rx="%.2f" fill="%s" fill-opacity="%.2f"%s/>'
               % (x0, y0, x1 - x0, y1 - y0, rx, fill, fill_opacity, s))


def line(x0, y0, x1, y1, color, sw=0.2, opacity=1.0):
    out.append('<path d="M%.3f %.3f L%.3f %.3f" stroke="%s" stroke-width="%.2f" stroke-opacity="%.2f" fill="none"/>'
               % (x0, y0, x1, y1, color, sw, opacity))


def compartment(box):
    rect(box, "#04070e", BRASS, 0.22, 1.0, 0.35, 0.32)


def build():
    out.append('<?xml version="1.0" encoding="UTF-8"?>')
    out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H))
    out.append('<!-- Gastounet, 20HP : laque bleu nuit et laiton. Généré par tools/gastounet_panel.py -->')
    out.append('<defs><linearGradient id="laque" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="%s"/>'
               '<stop offset="1" stop-color="%s"/></linearGradient></defs>' % (LACQ_TOP, LACQ_BOT))
    out.append('<rect x="0" y="0" width="%g" height="%g" fill="url(#laque)"/>' % (W, H))
    rect((1.4, 1.4, W - 1.4, H - 1.4), "none", BRASS, 0.2, 0.6, 0.0, 0.35)
    rect(SCREEN, GLASS, GLASS_EDGE, 0.2, 1.0)
    line(SCREEN[0] + 2.0, 23.6, SCREEN[2] - 2.0, 23.6, GLASS_EDGE, 0.15, 0.6)
    compartment(EDIT_BOX)
    compartment(MEM_BOX)
    line(SEP1_X, ROW_Y[0] - 3.8, SEP1_X, ROW_Y[3] + 3.8, BRASS, 0.2, 0.25)
    line(SEP2_X, ROW_Y[0] - 3.8, SEP2_X, ROW_Y[3] + 3.8, BRASS, 0.2, 0.25)
    compartment(LAUNCH_BOX)
    compartment(IN_BOX)
    rect(END_PLATE, BRASS, None, rx=1.0)
    out.append("</svg>")
    with open(os.path.join(ROOT, "res/Gastounet.svg"), "w") as f:
        f.write("\n".join(out) + "\n")


def cpp_color(c):
    return "nvgRGB(0x%s, 0x%s, 0x%s)" % (c[1:3], c[3:5], c[5:7])


def header():
    def v(name, x):
        return "static const float %s = %.3ff;" % (name, x)

    def arr(name, xs):
        return "static const float %s[%d] = {%s};" % (name, len(xs), ", ".join("%.3ff" % x for x in xs))

    def box(name, b):
        return "static const float %s[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % ((name,) + tuple(b))

    L = ["#pragma once", "// Généré par tools/gastounet_panel.py : ne pas modifier à la main. Coordonnées en mm.", "",
         "namespace gnl {", ""]
    L += [box("SCREEN", SCREEN), v("ROW_Y0", ROW_Y0), v("ROW_PITCH", ROW_PITCH), "static const int ROWS = %d;" % ROWS,
          v("EDIT_X", EDIT_X), arr("EDIT_Y", EDIT_Y), v("EDIT_W", EDIT_W), v("EDIT_H", EDIT_H), v("MODE_Y", MODE_Y), v("MODE_W", MODE_W), v("MODE_H", MODE_H), arr("MODE_X", MODE_X),
          box("MEM_BOX", MEM_BOX), v("BANK_X", BANK_X), v("BANK_W", BANK_W), v("BANK_H", BANK_H), arr("ROW_Y", ROW_Y),
          arr("SLOT_X", SLOT_X), v("SLOT_W", SLOT_W), v("SLOT_H", SLOT_H), arr("ACT_X", ACT_X), arr("ACT_Y", ACT_Y), v("ACT_R", ACT_R),
          v("SEG_Y", SEG_Y), v("SEG_H", SEG_H), v("QUANT_X0", QUANT_X0), v("QUANT_W", QUANT_W), v("CHAIN_X0", CHAIN_X0),
          v("CHAIN_W", CHAIN_W), arr("IN_X", IN_X), v("JACK_Y", JACK_Y), v("END_X", END_X), ""]
    L += ["static const gl::Label LABELS[] = {"]
    for (x, y, t, s, c, a, sp) in LABELS:
        al = {"center": "NVG_ALIGN_CENTER", "left": "NVG_ALIGN_LEFT", "right": "NVG_ALIGN_RIGHT"}[a]
        L.append('\t{%.3ff, %.3ff, "%s", %.2ff, %s, %s, %.2ff},' % (x, y, t, s, cpp_color(c), al, sp))
    L += ["};", "", "} // namespace gnl", ""]
    with open(os.path.join(ROOT, "src/GastounetLayout.hpp"), "w") as f:
        f.write("\n".join(L))


def preview():
    from PIL import Image, ImageDraw, ImageFont
    S = 8
    tmp = os.path.join(ROOT, "tools/preview")
    os.makedirs(tmp, exist_ok=True)
    png = os.path.join(tmp, "Gastounet.png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), os.path.join(ROOT, "res/Gastounet.svg"), png,
                    str(int(W * S)), str(int(H * S))], capture_output=True, check=True)
    img = Image.open(png).convert("RGBA")
    dr = ImageDraw.Draw(img)
    fonts, boxes = {}, []

    def font(px):
        if px not in fonts:
            fonts[px] = ImageFont.truetype(os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf"), max(1, int(px * 25.4 / 75 * S)))
        return fonts[px]

    def text(x, y, t, px, color, align="center", sp=0.0, record=True):
        f = font(px)
        w = f.getlength(t) + sp * 25.4 / 75 * S * max(0, len(t) - 1)
        bb = f.getbbox(t)
        X = x * S - (w / 2 if align == "center" else (w if align == "right" else 0))
        Y = y * S - (bb[1] + bb[3]) / 2
        cx = X
        for ch in t:
            dr.text((cx, Y), ch, font=f, fill=color)
            cx += f.getlength(ch) + sp * 25.4 / 75 * S
        if record:
            boxes.append(("t:" + t, X, Y + bb[1], X + w, Y + bb[3]))

    def rbox(cx, cy, w, h, fill, outline, name=None, r=0.8):
        x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
        dr.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=min(r, h * 0.45, w * 0.45) * S, fill=fill, outline=outline)
        if name:
            boxes.append((name, x0 * S, y0 * S, x1 * S, y1 * S))

    def circle(x, y, r, fill, outline=None, name=None):
        dr.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline)
        if name:
            boxes.append((name, (x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S))

    def jack(x, y, name):
        circle(x, y, 4.1, "#2a2f3a", "#e3ddcf", "j:" + name)
        circle(x, y, 1.7, "#05070c")

    for (x, y, t, s, c, a, sp) in LABELS:
        text(x, y, t, s, c, a, sp)
    # Écran
    text(6.6, 16.8, "PATTERN", 6.2, LABEL_DIM, "left", record=False)
    text(23.0, 16.6, "A03", 13.0, "#ecd29a", "left", record=False)
    text(37.6, 16.8, "EDITED", 5.6, "#e0805f", "left", record=False)
    text(76.0, 16.8, "BAR 13 · BEAT 4", 5.0, LABEL_DIM, "right")
    text(6.6, 21.4, "COPY A01: PICK THE DESTINATION", 5.4, "#ecd29a", "left", record=False)
    for r in range(ROWS):
        y = ROW_Y0 + r * ROW_PITCH + ROW_PITCH / 2
        text(6.6, y, "%02d" % (r + 1), 5.2, "#8f8468", "left", record=False)
        text(14.0, y, ["A01", "A02", "A01", "B01"][r], 6.6, "#ecd29a", "left", record=False)
        text(27.0, y, ["8 BARS", "64 BARS", "8 BARS", "2 BARS"][r], 5.6, "#ecd29a", "left", record=False)
    for y, t in zip(EDIT_Y, ["+ ROW", "DUP", "− ROW"]):
        rbox(EDIT_X, y, EDIT_W, EDIT_H, None, "#8a7347", "rowbtn" + t, r=2.6)
        text(EDIT_X, y, t, 5.0, "#ecd29a", record=False)
    # Modes
    for k, (x, t) in enumerate(zip(MODE_X, ["PATTERN", "SONG", "SONG REC", "END: LOOP"])):
        rbox(x, MODE_Y, MODE_W, MODE_H, BRASS if k == 0 else None, "#8a7347", "mode%d" % k, r=2.8)
        text(x, MODE_Y, t, 6.0, INK if k == 0 else "#ecd29a", record=False)
    # Mémoires
    text(7.0, 55.0, "PATTERNS · BANK A", 6.4, LABEL_DIM, "left", 0.9)
    for r, y in enumerate(ROW_Y):
        rbox(BANK_X, y, BANK_W, BANK_H, BRASS if r == 0 else "#101829", "#6e5d3c", "bank%d" % r)
        text(BANK_X, y, "ABCD"[r], 8.4, INK if r == 0 else "#ecd29a", record=False)
        for c, x in enumerate(SLOT_X):
            k = r * 4 + c
            rbox(x, y, SLOT_W, SLOT_H, BRASS if k == 2 else "#0a101c", "#8a7347" if k < 6 else "#3d3526", "slot%d" % k)
            text(x, y - 0.4, "%02d" % (k + 1), 7.2, INK if k == 2 else ("#ecd29a" if k < 6 else "#5d5544"), record=False)
    for k in range(4):
        circle(ACT_X[k % 2], ACT_Y[k // 2], ACT_R, None, BRASS, "act%d" % k)
    # Lancement
    for k, t in enumerate(["NOW", "1 BEAT", "1 BAR", "2 BARS", "4 BARS", "TRACK 1"]):
        x = QUANT_X0 + k * QUANT_W
        rbox(x + QUANT_W / 2, SEG_Y, QUANT_W, SEG_H, BRASS if k == 2 else None, "#6e5d3c", "q%d" % k, r=0.3)
        text(x + QUANT_W / 2, SEG_Y, t, 5.0, INK if k == 2 else "#ecd29a")
    for k, t in enumerate(["RESTART", "LEGATO"]):
        x = CHAIN_X0 + k * CHAIN_W
        rbox(x + CHAIN_W / 2, SEG_Y, CHAIN_W, SEG_H, BRASS if k == 0 else None, "#6e5d3c", "c%d" % k, r=0.3)
        text(x + CHAIN_W / 2, SEG_Y, t, 5.0, INK if k == 0 else "#ecd29a")
    for x in IN_X:
        jack(x, JACK_Y, "in%.0f" % x)
    jack(END_X, JACK_Y, "end")
    for (x, y) in [(7.62, 2.54), (W - 7.62, 2.54), (7.62, H - 2.54), (W - 7.62, H - 2.54)]:
        circle(x, y, 2.4, "#bbbbbb", None, "vis")

    clashes = []
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            a, b = boxes[i], boxes[j]
            if a[1] < b[3] - 1 and b[1] < a[3] - 1 and a[2] < b[4] - 1 and b[2] < a[4] - 1:
                clashes.append((a[0], b[0]))
    img.save(png)
    print(png)
    for c in clashes:
        print("  chevauchement :", c[0], "/", c[1])
    print("%d chevauchement(s)" % len(clashes))


if __name__ == "__main__":
    build()
    header()
    print(os.path.join(ROOT, "res/Gastounet.svg"))
    print(os.path.join(ROOT, "src/GastounetLayout.hpp"))
    if "--preview" in sys.argv:
        preview()
