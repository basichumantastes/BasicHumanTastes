#!/usr/bin/env python3
"""Disposition du panneau de Gaston (36 HP), laque bleu nuit et laiton.

Source unique des coordonnées (mm) : ce script écrit
  - res/Gaston.svg            le fond statique (laque, filets, compartiments, plaques laiton, vitres des écrans) ;
  - src/GastonLayout.hpp      les positions des composants et les textes fixes, lus par src/Gaston.cpp ;
  - tools/preview/Gaston.png  avec --preview : fond + composants et textes simulés, et chevauchements détectés.
Rack lit les SVG avec nanosvg : pas de texte (dessiné par le module en Michroma), pas de filtres.

Usage : python3 tools/gaston_panel.py [--preview]
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 182.88, 128.5

LACQ_TOP, LACQ_BOT = "#17223b", "#0e1628"
BRASS, BRASS_DIM, IVORY, LABEL, LABEL_DIM = "#c8a25c", "#6e5d3c", "#ece3cf", "#d6c9aa", "#b3a687"
GLASS, GLASS_EDGE, INK, VERD = "#060a12", "#5a4c33", "#141a2a", "#74c2b2"

# --- Colonnes
LEFT_X0, LEFT_X1 = 5.6, 41.2
MAIN_X0, MAIN_X1 = 43.6, 177.28
MAIN_CX = (MAIN_X0 + MAIN_X1) / 2
BODY_Y0, BODY_Y1 = 12.6, 123.0

# --- Colonne de gauche
CLK_BOX = (LEFT_X0, 12.6, LEFT_X1, 46.0)
BPM_SCREEN = (7.6, 16.8, 39.2, 23.6)
TEMPO = (15.6, 33.6, 6.2)            # x, y, rayon
SWING = (32.6, 35.2, 3.6)
SWING_READ_Y = 28.6
KNOB_LABEL_Y = 43.2
TR_BOX = (LEFT_X0, 48.0, LEFT_X1, 65.4)
REC = (12.6, 58.2, 4.0)
CUE = (23.8, 58.2, 4.4)
PLAY = (34.6, 58.2, 4.4)
IN_BOX = (LEFT_X0, 67.4, LEFT_X1, 102.6)
IN_JACKS = [(14.6, 80.9, "CLOCK"), (32.2, 80.9, "PLAY"), (14.6, 94.4, "RESET"), (32.2, 94.4, "PADS")]
OUT_PLATE_L = (LEFT_X0, 104.6, LEFT_X1, 121.8)
OUT_L_JACKS = [(11.53, "CLOCK"), (23.4, "RUN"), (35.27, "RESET")]
JACK_LABEL_DY = -5.4                  # étiquette au-dessus du jack
OUT_JACK_Y = 116.8

# --- Écran de voie
SCREEN = (MAIN_X0, 12.6, MAIN_X1, 20.2)

# --- Bloc PAS
SEQ_BOX = (MAIN_X0, 22.2, MAIN_X1, 86.8)
STEP_X0 = MAIN_CX - 127.6 / 2        # bord gauche du pas 1
STEP_W, STEP_PITCH, GROUP_EXTRA = 7.0, 7.8, 1.2
NUM_Y = 25.0
STEP_Y0, STEP_H = 26.6, 7.0
GAUGE_Y0, GAUGE_H = 34.6, 1.6
CV_BAR_Y0, CV_BAR_H = 26.6, 9.6
MAP_BOX = (STEP_X0 - 1.2, 38.0, STEP_X0 + 127.6 + 1.2, 68.6)
MAP_ROW_Y0, MAP_ROW_PITCH, MAP_ROW_H, MAP_SQ = 39.2, 2.4, 2.2, 1.8
PAGE_Y, PAGE_W, PAGE_H, PAGE_PITCH = 72.2, 7.6, 4.0, 9.2
PAGE_LABEL_X = MAIN_CX - 1.5 * PAGE_PITCH - PAGE_W / 2 - 6.0
RULE_Y = 75.8
CTRL_Y = 80.6
CTRL_KNOB_R = 3.2
CTRL_X = [STEP_X0 + 3.2 + 28.5 * k for k in range(4)]    # LENGTH, DIVISION, SENS, SLEW
SEG_X0, SEG_W, SEG_H = STEP_X0 + 127.6 - 21.6, 10.8, 3.0
SEG_Y = [78.7, 82.5]

# --- Bloc VOIES
VOICE_BOX = (MAIN_X0, 88.8, MAIN_X1, 123.0)
COL_X0 = MAIN_CX - 127.2 / 2
LABEL_COL_W, COL_PITCH, POLY_W, CV_GAP = 9.6, 8.8, 9.2, 2.8
TRIG_COL_X = [COL_X0 + LABEL_COL_W + COL_PITCH * (i + 0.5) for i in range(8)]
POLY_X = COL_X0 + LABEL_COL_W + 8 * COL_PITCH + POLY_W / 2
CV_X0 = COL_X0 + LABEL_COL_W + 8 * COL_PITCH + POLY_W + CV_GAP
CV_COL_X = [CV_X0 + COL_PITCH * (j + 0.5) for j in range(4)]
LED_Y, SEL_Y, SEL_SIZE = 90.9, 95.2, 6.0
PAD_Y, PAD_W, PAD_H = 102.0, 6.8, 4.8
MUTE_Y, MUTE_W, MUTE_H = 106.7, 5.6, 2.2
CV_READ = (CV_X0, 99.6, CV_X0 + 4 * COL_PITCH, 107.8)
OUT_PLATE = (MAIN_X0 + 2.2, 109.2, MAIN_X1 - 2.2, 121.8)
LABEL_COL_CX = COL_X0 + LABEL_COL_W / 2


def step_x(k):
    """Bord gauche du pas k (0 à 15) dans le ruban et la carte."""
    return STEP_X0 + STEP_PITCH * k + GROUP_EXTRA * (k // 4)


# --- Textes fixes : (x, y, texte, taille en px de Rack, couleur, alignement, espacement)
LABELS = []


def label(x, y, text, size=4.0, color=LABEL, align="center", spacing=0.0):
    LABELS.append((x, y, text, size, color, align, spacing))


def section(x, y, text, color=LABEL_DIM):
    label(x, y, text, 6.4, color, "left", 0.9)


label(13.0, 7.2, "GASTON", 15.0, "#d4ae66", "left", 1.2)
label(170.0, 7.0, "SEQUENCER · 8 TRIG · 4 CV", 5.0, LABEL, "right", 0.6)
label(W / 2, 125.7, "BASIC HUMAN TASTES", 4.6, "#a39780", "center", 1.2)
section(8.0, 15.0, "CLOCK")
label(TEMPO[0], KNOB_LABEL_Y, "TEMPO", 6.6, LABEL, "center", 0.6)
label(SWING[0], KNOB_LABEL_Y, "SWING", 6.6, LABEL, "center", 0.6)
section(8.0, 50.4, "TRANSPORT")
section(8.0, 69.8, "INPUTS")
for (x, y, n) in IN_JACKS:
    label(x, y + JACK_LABEL_DY, n, 6.6)
label((LEFT_X0 + LEFT_X1) / 2, 107.4, "OUTPUTS", 5.8, INK, "center", 0.9)
for (x, n) in OUT_L_JACKS:
    label(x, OUT_JACK_Y + JACK_LABEL_DY, n, 5.4, INK)
label(MAP_BOX[0] + 2.0, MAP_BOX[1], "MAP", 5.2, "#a39780", "left", 0.9)
label(PAGE_LABEL_X, PAGE_Y, "PAGE", 6.0, LABEL_DIM, "center", 0.9)
label(COL_X0 + 0.6, SEL_Y, "SELECT", 4.4, LABEL, "left", 0.5)
label(COL_X0 + 0.6, PAD_Y, "PADS", 4.6, LABEL, "left", 0.5)
label(COL_X0 + 0.6, MUTE_Y, "MUTE", 4.6, LABEL, "left", 0.5)
label(LABEL_COL_CX, OUT_JACK_Y, "OUT", 6.0, INK, "center", 0.9)
for i, x in enumerate(TRIG_COL_X):
    label(x, OUT_JACK_Y + JACK_LABEL_DY, str(i + 1), 6.4, INK)
label(POLY_X, OUT_JACK_Y + JACK_LABEL_DY, "POLY", 6.0, INK)
for j, x in enumerate(CV_COL_X):
    label(x, OUT_JACK_Y + JACK_LABEL_DY, "ABCD"[j], 6.4, INK)

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
    out.append('<!-- Gaston, 36HP : laque bleu nuit et laiton. Généré par tools/gaston_panel.py -->')
    out.append('<defs><linearGradient id="laque" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="%s"/>'
               '<stop offset="1" stop-color="%s"/></linearGradient></defs>' % (LACQ_TOP, LACQ_BOT))
    out.append('<rect x="0" y="0" width="%g" height="%g" fill="url(#laque)"/>' % (W, H))
    # Filet de laiton incrusté
    rect((1.4, 1.4, W - 1.4, H - 1.4), "none", BRASS, 0.2, 0.6, 0.0, 0.35)

    # Colonne de gauche
    compartment(CLK_BOX)
    rect(BPM_SCREEN, GLASS, GLASS_EDGE, 0.2, 0.6)
    compartment(TR_BOX)
    compartment(IN_BOX)
    rect(OUT_PLATE_L, BRASS, None, rx=1.0)

    # Écran de voie
    rect(SCREEN, GLASS, GLASS_EDGE, 0.2, 1.0)

    # Bloc PAS : cadre, carte, filet (le module repeint cadre et filet en vert-de-gris sur une voie CV)
    compartment(SEQ_BOX)
    rect(MAP_BOX, GLASS, "#3d3526", 0.2, 0.8)
    # Une pastille de la couleur du bloc sous l'étiquette CARTE, posée sur le bord du cadre
    rect((MAP_BOX[0] + 1.2, MAP_BOX[1] - 1.1, MAP_BOX[0] + 11.0, MAP_BOX[1] + 1.1), "#0f1628", None, rx=0.3)
    line(SEQ_BOX[0] + 2.4, RULE_Y, SEQ_BOX[2] - 2.4, RULE_Y, BRASS, 0.2, 0.18)

    # Bloc VOIES
    compartment(VOICE_BOX)
    rect(CV_READ, "#060d10", "#2e5650", 0.2, 0.8)
    rect(OUT_PLATE, BRASS, None, rx=0.8)
    xd = CV_X0 - CV_GAP / 2
    line(xd, OUT_PLATE[1] + 2.4, xd, OUT_PLATE[3] - 2.4, INK, 0.2, 0.35)

    out.append("</svg>")
    with open(os.path.join(ROOT, "res/Gaston.svg"), "w") as f:
        f.write("\n".join(out) + "\n")


def cpp_color(c):
    return "nvgRGB(0x%s, 0x%s, 0x%s)" % (c[1:3], c[3:5], c[5:7])


def header():
    def v(name, x):
        return "static const float %s = %.3ff;" % (name, x)

    def arr(name, xs):
        return "static const float %s[%d] = {%s};" % (name, len(xs), ", ".join("%.3ff" % x for x in xs))

    L = ["#pragma once", "// Généré par tools/gaston_panel.py : ne pas modifier à la main. Coordonnées en mm.", "",
         "namespace gl {", ""]
    L += [v("TEMPO_X", TEMPO[0]), v("TEMPO_Y", TEMPO[1]), v("TEMPO_R", TEMPO[2]),
          v("SWING_X", SWING[0]), v("SWING_Y", SWING[1]), v("SWING_R", SWING[2]), v("SWING_READ_Y", SWING_READ_Y)]
    L += ["static const float BPM_SCREEN[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % BPM_SCREEN]
    L += [arr("TRANSPORT_X", [REC[0], CUE[0], PLAY[0]]), arr("TRANSPORT_R", [REC[2], CUE[2], PLAY[2]]), v("TRANSPORT_Y", REC[1])]
    L += [arr("IN_X", [j[0] for j in IN_JACKS]), arr("IN_Y", [j[1] for j in IN_JACKS])]
    L += [arr("OUT_L_X", [j[0] for j in OUT_L_JACKS]), v("OUT_JACK_Y", OUT_JACK_Y)]
    L += ["static const float SCREEN[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % SCREEN,
          "static const float SEQ_BOX[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % SEQ_BOX,
          "static const float MAP_BOX[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % MAP_BOX]
    L += [v("STEP_X0", STEP_X0), v("STEP_W", STEP_W), v("STEP_PITCH", STEP_PITCH), v("GROUP_EXTRA", GROUP_EXTRA),
          v("NUM_Y", NUM_Y), v("STEP_Y0", STEP_Y0), v("STEP_H", STEP_H), v("GAUGE_Y0", GAUGE_Y0), v("GAUGE_H", GAUGE_H),
          v("CV_BAR_Y0", CV_BAR_Y0), v("CV_BAR_H", CV_BAR_H),
          v("MAP_ROW_Y0", MAP_ROW_Y0), v("MAP_ROW_PITCH", MAP_ROW_PITCH), v("MAP_ROW_H", MAP_ROW_H), v("MAP_SQ", MAP_SQ),
          v("PAGE_Y", PAGE_Y), v("PAGE_W", PAGE_W), v("PAGE_H", PAGE_H), v("PAGE_PITCH", PAGE_PITCH), v("MAIN_CX", MAIN_CX),
          v("RULE_Y", RULE_Y), v("CTRL_Y", CTRL_Y), v("CTRL_KNOB_R", CTRL_KNOB_R), arr("CTRL_X", CTRL_X),
          v("SEG_X0", SEG_X0), v("SEG_W", SEG_W), v("SEG_H", SEG_H), arr("SEG_Y", SEG_Y)]
    L += [arr("TRIG_COL_X", TRIG_COL_X), v("POLY_X", POLY_X), arr("CV_COL_X", CV_COL_X),
          v("LED_Y", LED_Y), v("SEL_Y", SEL_Y), v("SEL_SIZE", SEL_SIZE), v("PAD_Y", PAD_Y), v("PAD_W", PAD_W), v("PAD_H", PAD_H),
          v("MUTE_Y", MUTE_Y), v("MUTE_W", MUTE_W), v("MUTE_H", MUTE_H),
          "static const float CV_READ[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % CV_READ, ""]
    L += ["struct Label { float x, y; const char* text; float size; NVGcolor color; int align; float spacing; };",
          "static const Label LABELS[] = {"]
    for (x, y, t, s, c, a, sp) in LABELS:
        al = {"center": "NVG_ALIGN_CENTER", "left": "NVG_ALIGN_LEFT", "right": "NVG_ALIGN_RIGHT"}[a]
        L.append('\t{%.3ff, %.3ff, "%s", %.2ff, %s, %s, %.2ff},' % (x, y, t, s, cpp_color(c), al, sp))
    L += ["};", "", "} // namespace gl", ""]
    with open(os.path.join(ROOT, "src/GastonLayout.hpp"), "w") as f:
        f.write("\n".join(L))


def preview():
    """Fond rendu par AppKit, puis composants et contenus simulés, pour juger l'ensemble et repérer les chevauchements."""
    from PIL import Image, ImageDraw, ImageFont
    S = 8
    tmp = os.path.join(ROOT, "tools/preview")
    os.makedirs(tmp, exist_ok=True)
    png = os.path.join(tmp, "Gaston.png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), os.path.join(ROOT, "res/Gaston.svg"), png,
                    str(int(W * S)), str(int(H * S))], capture_output=True, check=True)
    img = Image.open(png).convert("RGBA")
    dr = ImageDraw.Draw(img)
    fonts, boxes = {}, []

    def font(px):
        if px not in fonts:
            fonts[px] = ImageFont.truetype(os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf"), max(1, int(px * 25.4 / 75 * S)))
        return fonts[px]

    def text_w(t, px, sp):
        return font(px).getlength(t) + sp * 25.4 / 75 * S * max(0, len(t) - 1)

    def text(x, y, t, px, color, align="center", sp=0.0, record=True):
        f = font(px)
        w = text_w(t, px, sp)
        bb = f.getbbox(t)
        X = x * S - (w / 2 if align == "center" else (w if align == "right" else 0))
        Y = y * S - (bb[1] + bb[3]) / 2
        cx = X
        for ch in t:
            dr.text((cx, Y), ch, font=f, fill=color)
            cx += f.getlength(ch) + sp * 25.4 / 75 * S
        if record:
            boxes.append(("t:" + t, X, Y + bb[1], X + w, Y + bb[3]))

    def circle(x, y, r, fill, outline=None, name=None):
        dr.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline)
        if name:
            boxes.append((name, (x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S))

    def box(x0, y0, x1, y1, fill, outline=None, name=None, r=0.6):
        dr.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=min(r, (y1 - y0) * 0.45, (x1 - x0) * 0.45) * S, fill=fill, outline=outline)
        if name:
            boxes.append((name, x0 * S, y0 * S, x1 * S, y1 * S))

    def jack(x, y, name):
        circle(x, y, 4.1, "#2a2f3a", "#e3ddcf", "j:" + name)
        circle(x, y, 1.7, "#05070c")

    def knob(x, y, r, name, color=BRASS):
        circle(x, y, r, color, "#8a7347", "k:" + name)
        dr.line([x * S, y * S, x * S, (y - r * 0.8) * S], fill=INK, width=2)

    for (x, y, t, s, c, a, sp) in LABELS:
        text(x, y, t, s, c, a, sp)
    # Gauche
    text(BPM_SCREEN[0] + 2, (BPM_SCREEN[1] + BPM_SCREEN[3]) / 2, "120.0", 7, "#ecd29a", "left", record=False)
    text(BPM_SCREEN[2] - 2, (BPM_SCREEN[1] + BPM_SCREEN[3]) / 2 + 0.6, "BPM", 3.2, LABEL_DIM, "right", record=False)
    knob(*TEMPO, "tempo"); knob(*SWING, "swing")
    text(SWING[0], SWING_READ_Y, "54 %", 3.6, "#ecd29a", record=False)
    for (x, y, r), n in zip([REC, CUE, PLAY], ["REC", "CUE", "PLAY"]):
        circle(x, y, r, None, BRASS, "b:" + n)
        text(x, y, n, 3.2, "#ecd29a", record=False)
    for (x, y, n) in IN_JACKS:
        jack(x, y, n)
    for (x, n) in OUT_L_JACKS:
        jack(x, OUT_JACK_Y, n)
    # Écran
    sy = (SCREEN[1] + SCREEN[3]) / 2
    text(SCREEN[0] + 3, sy - 0.9, "VOIE 3", 5.4, "#ecd29a", "left", record=False)
    text(SCREEN[0] + 3, sy + 1.9, "TRIGS", 2.8, LABEL_DIM, "left", record=False)
    for k, (h, val) in enumerate([("LONG", "12"), ("RAPPORT", "×3/2"), ("SENS", "→"), ("PAS", "8"), ("PAGE", "1/1")]):
        x = SCREEN[0] + 40 + k * 18
        text(x, sy - 1.3, h, 2.8, LABEL_DIM, "left", record=False)
        text(x, sy + 1.4, val, 4.4, "#ecd29a", "left", record=False)
    # Ruban
    for k in range(16):
        x = step_x(k)
        text(x + STEP_W / 2, NUM_Y, str(k + 1), 3.2, "#6e6450", record=False)
        on = k % 3 != 2
        box(x, STEP_Y0, x + STEP_W, STEP_Y0 + STEP_H, BRASS if on else "#0a101c", "#8a7347" if on else "#3d3526")
        box(x, GAUGE_Y0, x + STEP_W, GAUGE_Y0 + GAUGE_H, "#0a101c", "#6e5d3c", r=0.3)
        if on:
            box(x, GAUGE_Y0, x + STEP_W * (1 if k % 4 else 0.5), GAUGE_Y0 + GAUGE_H, "#a8874c", r=0.3)
    # Carte
    for r in range(12):
        y = MAP_ROW_Y0 + r * MAP_ROW_PITCH + MAP_ROW_H / 2
        cv = r >= 8
        prog = (r * 5 + 3) % 16
        x0, x1 = step_x(0), step_x(prog) + STEP_W
        box(x0, y - 0.5, x1, y + 0.5, "#3a3324" if not cv else "#1d3a37", r=0.4)
        for k in range(16):
            x = step_x(k) + STEP_W / 2
            on = cv or (k * (r + 3)) % 5 < 2
            c = ("#f6ce7a" if not cv else "#9fe0d2") if on else "#1a1f2a"
            box(x - MAP_SQ / 2, y - MAP_SQ / 2, x + MAP_SQ / 2, y + MAP_SQ / 2, c, r=0.25)
    # Pages
    for p in range(4):
        x = MAIN_CX + (p - 1.5) * PAGE_PITCH
        box(x - PAGE_W / 2, PAGE_Y - PAGE_H / 2, x + PAGE_W / 2, PAGE_Y + PAGE_H / 2, BRASS if p == 0 else None, BRASS, "p%d" % p, r=2)
        text(x, PAGE_Y, str(p + 1), 3.6, INK if p == 0 else "#ecd29a", record=False)
    # Réglages de voie (version CV, la plus chargée)
    names = ["LENGTH", "DIVISION", "SENS", "SLEW"]
    for k, x in enumerate(CTRL_X):
        if k == 2:
            box(x - CTRL_KNOB_R, CTRL_Y - CTRL_KNOB_R, x + CTRL_KNOB_R, CTRL_Y + CTRL_KNOB_R, None, BRASS, "c:sens")
        else:
            knob(x, CTRL_Y, CTRL_KNOB_R, names[k], VERD)
        text(x + CTRL_KNOB_R + 1.6, CTRL_Y - 1.2, names[k], 5.8, LABEL, "left", 0.5)
        text(x + CTRL_KNOB_R + 1.6, CTRL_Y + 1.4, ["16", "x3/2", "ALLER-RET.", "35 %"][k], 7.0, "#9fe0d2", "left")
    for row, y in enumerate(SEG_Y):
        for s in range(2):
            x = SEG_X0 + s * SEG_W
            box(x, y - SEG_H / 2, x + SEG_W, y + SEG_H / 2, VERD if s == 0 else None, "#2e5650", "seg%d%d" % (row, s), r=0.3)
            text(x + SEG_W / 2, y, [["±5 V", "0–10 V"], ["LIBRE", "½ TON"]][row][s], 2.8, INK if s == 0 else "#9fe0d2")
    # Voies
    for i, x in enumerate(TRIG_COL_X):
        circle(x, LED_Y, 0.5, "#3a3222")
        circle(x, SEL_Y, SEL_SIZE / 2, "#101829", BRASS, "sel%d" % i)
        text(x, SEL_Y, str(i + 1), 4, "#ecd29a", record=False)
        box(x - PAD_W / 2, PAD_Y - PAD_H / 2, x + PAD_W / 2, PAD_Y + PAD_H / 2, "#1a2338", "#5a4c33", "pad%d" % i)
        box(x - MUTE_W / 2, MUTE_Y - MUTE_H / 2, x + MUTE_W / 2, MUTE_Y + MUTE_H / 2, "#0a101c", "#5a4c33", "mute%d" % i, r=1)
        jack(x, OUT_JACK_Y, "o%d" % i)
    jack(POLY_X, OUT_JACK_Y, "poly")
    for j, x in enumerate(CV_COL_X):
        circle(x, LED_Y, 0.5, "#1f3a36")
        box(x - SEL_SIZE / 2, SEL_Y - SEL_SIZE / 2, x + SEL_SIZE / 2, SEL_Y + SEL_SIZE / 2, "#0c1a1f", "#2e5650", "selcv%d" % j)
        text(x, SEL_Y, "ABCD"[j], 4, "#9fe0d2", record=False)
        text(x, (CV_READ[1] + CV_READ[3]) / 2 + 1.6, ["D#4", "+2.1", "-0.8", "+3.0"][j], 3.0, "#9fe0d2", record=False)
        jack(x, OUT_JACK_Y, "cv%d" % j)
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
    print(os.path.join(ROOT, "res/Gaston.svg"))
    print(os.path.join(ROOT, "src/GastonLayout.hpp"))
    if "--preview" in sys.argv:
        preview()
