#!/usr/bin/env python3
"""Disposition du panneau de Louis (34 HP), bakélite ivoire, bandeaux bleu ardoise, écran au phosphore vert.

Source unique des coordonnées (mm) : ce script écrit
  - res/Louis.svg            le fond statique (ivoire, filet, bandeaux, blocs, plaques de sortie, vitre de l'écran) ;
  - src/LouisLayout.hpp      les positions des composants et les textes fixes, lus par src/Louis.cpp ;
  - tools/preview/Louis.png  avec --preview : fond + composants et textes simulés, et chevauchements détectés.
Rack lit les SVG avec nanosvg : pas de texte (dessiné par le module), pas de filtres.
Étiquettes en IBM Plex Sans (Michroma est trop large pour des colonnes de 10 mm), titre et écran en Michroma.
Maquette de référence : https://claude.ai/artifact/Es8mchpaVusoknQRdvowev

Usage : python3 tools/louis_panel.py [--preview]
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 172.72, 128.5

IVORY_TOP, IVORY_BOT = "#f1ead9", "#e3d9c2"
INK, LABEL_DIM, SCREW_DOT = "#1d1b17", "#4f483c", "#8f887a"
BAND, BAND_TEXT, HEAD_TEXT = "#2c4250", "#f1ead9", "#e6dcc5"
PLATE, PLATE_TEXT, PLATE_DIM = "#1d1b17", "#f1ead9", "#cfc6b2"
GLASS, PHOS, PHOS_DIM = "#06120b", "#8ff5b0", "#2f6b47"

# Rapport entre la taille NanoVG (hauteur de ligne) et le em, pour chaque police : les tailles ci-dessous sont des em
FONT_FILES = {"reg": "IBMPlexSans-Regular.ttf", "bold": "IBMPlexSans-Bold.ttf", "mic": "Michroma-Regular.ttf"}
FONT_EM = {"reg": 0.7692, "bold": 0.7692, "mic": 0.7033}

# --- Grandes zones
X0, X1 = 4.6, W - 4.6
HEAD = (1.4, 1.4, W - 1.4, 9.6)
SCREEN = (X0, 11.4, X1, 37.0)
BAND_H = 4.0
ROW1 = (38.8, 65.2)
CLIM_BOX = (X0, ROW1[0], 114.0, ROW1[1])
PITCH_BOX = (116.0, ROW1[0], X1, ROW1[1])
LINK_BOX = (X0, 67.0, 50.2, 94.0)
CLOCK_BOX = (X0, 96.0, 50.2, 123.0)
VOICE_BOX = (52.6, 67.0, X1, 123.0)

# --- Rangée du haut : potard, nom, prise
KNOB_Y, LABEL_Y, JACK_Y = 48.4, 55.4, 60.7
PACE_R, KNOB_R = 4.6, 3.8
CLIM_X = [6.2 + 10.6 * (k + 0.5) for k in range(8)]
CLIM_NAMES = ["PACE", "TILT", "DRIFT", "WANDER", "DENSITY", "MEMORY", "CORREL", "GATE"]
DRIFT_PLATE = (92.6, 43.4, 112.4, 64.6)
DRIFT_X = [97.55, 107.45]
METER_W, METER_H = 1.8, 8.0
PITCH_X = [117.6 + 9.784 * (k + 0.5) for k in range(5)]
PITCH_NAMES = ["SCALE", "ROOT", "RANGE", "SKEW"]
WHEEL_R = 3.9

# --- LINK
LINK_KNOB = (11.0, 77.6, 4.0)
LINK_LABEL_Y, LINK_JACK_Y = 84.6, 89.6
MODE_X0, MODE_X1, MODE_Y0, MODE_H, MODE_GAP = 17.4, 49.0, 72.0, 6.6, 0.5

# --- CLOCK
GRID_KNOB = (10.4, 106.0, 3.9)
SNAP_KNOB = (22.0, 106.0, 3.9)
CLOCK_LABEL_Y, CLOCK_JACK_Y = 112.8, 117.9
CLOCK_SEP_X = 28.6
CLOCK_IN = (39.4, 101.9, 107.2)     # x, étiquette, prise
RESET_IN = (39.4, 112.6, 117.9)

# --- VOICES
PLATE_BOX = (113.4, 72.0, 166.6, 122.4)
OUT_X = [113.4 + 53.2 * (k + 0.5) / 4 for k in range(4)]
OUT_NAMES = ["TRIG", "GATE", "PITCH", "ENV"]
HEAD_Y = 74.4
ROW_Y = [80.6, 89.2, 97.8, 106.4]
SEP_Y = 111.0
ALL_LABEL_Y, ALL_JACK_Y = 113.0, 118.0
NUM_X, TIME_X, OCT_X, HOLD_X, LED_X = 57.0, 66.4, 78.6, 90.8, 103.0
SMALL_R = 3.2
HOLD_W, HOLD_H = 6.6, 4.4
LED_R = 1.0
STRIKE_X = 72.5
ANY_CHIP = (98.2, 111.6, 107.8, 122.4)

# --- Textes fixes : (x, y, texte, taille em en px de Rack, couleur, alignement, espacement, police)
LABELS = []


def label(x, y, text, size=6.2, color=INK, align="center", spacing=0.0, font="reg"):
    LABELS.append((x, y, text, size, color, align, spacing, font))


def title(box, text):
    label(box[0] + 1.8, box[1] + BAND_H / 2, text, 5.4, BAND_TEXT, "left", 1.2, "bold")


label(13.0, 5.5, "LOUIS", 14.0, BAND_TEXT, "left", 1.4, "mic")
label(W - 13.0, 5.6, "KRELL GENERATOR · FOUR VOICES · TRIG GATE PITCH ENV", 5.0, HEAD_TEXT, "right", 0.8)
label(W / 2, 125.75, "BASIC HUMAN TASTES", 4.6, SCREW_DOT, "center", 1.2, "mic")
for b, t in [(CLIM_BOX, "CLIMATE"), (PITCH_BOX, "PITCH"), (LINK_BOX, "LINK"), (CLOCK_BOX, "CLOCK"), (VOICE_BOX, "VOICES")]:
    title(b, t)
for x, n in zip(CLIM_X, CLIM_NAMES):
    label(x, LABEL_Y, n)
for x, n in zip(DRIFT_X, ["DRIFT A", "DRIFT B"]):
    label(x, LABEL_Y, n, 5.2, PLATE_TEXT)
for x, n in zip(PITCH_X, PITCH_NAMES):
    label(x, LABEL_Y, n)
label(PITCH_X[4], LABEL_Y, "SCALE IN", 5.2)
label(LINK_KNOB[0], LINK_LABEL_Y, "LINK")
label(GRID_KNOB[0], CLOCK_LABEL_Y, "GRID")
label(SNAP_KNOB[0], CLOCK_LABEL_Y, "SNAP")
label(CLOCK_IN[0], CLOCK_IN[1], "CLOCK", 5.6)
label(RESET_IN[0], RESET_IN[1], "RESET", 5.6)
for x, n in zip([TIME_X, OCT_X, HOLD_X], ["TIME", "OCT", "HOLD"]):
    label(x, HEAD_Y, n, 5.6)
for i, y in enumerate(ROW_Y):
    label(NUM_X, y, str(i + 1), 7.4, INK, "center", 0.0, "bold")
for x, n in zip(OUT_X, OUT_NAMES):
    label(x, HEAD_Y, n, 5.6, PLATE_TEXT)
    label(x, ALL_LABEL_Y, "POLY", 4.8, PLATE_DIM, "center", 0.6)
label(NUM_X, ALL_JACK_Y, "ALL", 4.8, LABEL_DIM, "center", 0.4)
label(STRIKE_X, ALL_LABEL_Y, "STRIKE", 5.6)
label(HOLD_X, ALL_LABEL_Y, "FREEZE", 5.6)
label(LED_X, ALL_LABEL_Y, "ANY", 5.6, PLATE_TEXT)

out = []


def rect(box, fill, stroke=None, sw=0.25, rx=1.0, fill_opacity=1.0, stroke_opacity=1.0):
    x0, y0, x1, y1 = box
    s = ' stroke="%s" stroke-width="%.2f" stroke-opacity="%.2f"' % (stroke, sw, stroke_opacity) if stroke else ""
    out.append('<rect x="%.3f" y="%.3f" width="%.3f" height="%.3f" rx="%.2f" fill="%s" fill-opacity="%.2f"%s/>'
               % (x0, y0, x1 - x0, y1 - y0, rx, fill, fill_opacity, s))


def line(x0, y0, x1, y1, color, sw=0.2, opacity=1.0):
    out.append('<path d="M%.3f %.3f L%.3f %.3f" stroke="%s" stroke-width="%.2f" stroke-opacity="%.2f" fill="none"/>'
               % (x0, y0, x1, y1, color, sw, opacity))


def block(box):
    """Un bloc : fond légèrement éclairci, filet encre, bandeau ardoise en tête (coins du bas carrés)."""
    x0, y0, x1, y1 = box
    rect(box, "#ffffff", INK, 0.22, 1.0, 0.18, 0.45)
    rect((x0, y0, x1, y0 + BAND_H), BAND, None, rx=1.0)
    rect((x0, y0 + BAND_H - 1.2, x1, y0 + BAND_H), BAND, None, rx=0.0)
    line(x0 + 0.2, y0 + BAND_H, x1 - 0.2, y0 + BAND_H, "#000000", 0.25, 0.25)


def build():
    out.append('<?xml version="1.0" encoding="UTF-8"?>')
    out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H))
    out.append('<!-- Louis, 34HP : bakélite ivoire, bandeaux ardoise. Généré par tools/louis_panel.py -->')
    out.append('<defs><linearGradient id="ivoire" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="%s"/>'
               '<stop offset="1" stop-color="%s"/></linearGradient></defs>' % (IVORY_TOP, IVORY_BOT))
    out.append('<rect x="0" y="0" width="%g" height="%g" fill="url(#ivoire)"/>' % (W, H))
    rect((1.4, 1.4, W - 1.4, H - 1.4), "none", INK, 0.2, 0.6, 0.0, 0.35)

    # Bandeau de tête
    rect(HEAD, BAND, None, rx=0.6)
    line(HEAD[0], HEAD[3], HEAD[2], HEAD[3], "#000000", 0.35, 0.25)

    # Écran : cadre encre, vitre
    rect(SCREEN, INK, None, rx=1.2)
    rect((SCREEN[0] + 0.4, SCREEN[1] + 0.4, SCREEN[2] - 0.4, SCREEN[3] - 0.4), GLASS, None, rx=0.9)

    for b in [CLIM_BOX, PITCH_BOX, LINK_BOX, CLOCK_BOX, VOICE_BOX]:
        block(b)

    # Plaques de sortie : dérives, seize sorties, ANY
    rect(DRIFT_PLATE, PLATE, None, rx=0.8)
    rect(PLATE_BOX, PLATE, None, rx=1.0)
    line(PLATE_BOX[0] + 1.2, SEP_Y, PLATE_BOX[2] - 1.2, SEP_Y, PLATE_TEXT, 0.2, 0.35)
    rect(ANY_CHIP, PLATE, None, rx=0.8)

    # Filets : la rangée ALL sous les voix, la séparation CLOCK / entrées
    line(VOICE_BOX[0] + 1.4, SEP_Y, PLATE_BOX[0] - 1.6, SEP_Y, INK, 0.2, 0.45)
    line(CLOCK_SEP_X, CLOCK_BOX[1] + BAND_H + 1.4, CLOCK_SEP_X, CLOCK_BOX[3] - 1.4, INK, 0.2, 0.3)

    out.append("</svg>")
    with open(os.path.join(ROOT, "res/Louis.svg"), "w") as f:
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

    L = ["#pragma once", "// Généré par tools/louis_panel.py : ne pas modifier à la main. Coordonnées en mm.", "",
         "namespace ll {", ""]
    L += [box("SCREEN", SCREEN), v("KNOB_Y", KNOB_Y), v("LABEL_Y", LABEL_Y), v("JACK_Y", JACK_Y),
          v("PACE_R", PACE_R), v("KNOB_R", KNOB_R), arr("CLIM_X", CLIM_X), arr("DRIFT_X", DRIFT_X),
          v("METER_W", METER_W), v("METER_H", METER_H), arr("PITCH_X", PITCH_X), v("WHEEL_R", WHEEL_R)]
    L += [arr("LINK_KNOB", LINK_KNOB), v("LINK_JACK_Y", LINK_JACK_Y),
          v("MODE_X0", MODE_X0), v("MODE_X1", MODE_X1), v("MODE_Y0", MODE_Y0), v("MODE_H", MODE_H), v("MODE_GAP", MODE_GAP)]
    L += [arr("GRID_KNOB", GRID_KNOB), arr("SNAP_KNOB", SNAP_KNOB), v("CLOCK_JACK_Y", CLOCK_JACK_Y),
          arr("CLOCK_IN", CLOCK_IN), arr("RESET_IN", RESET_IN)]
    L += [arr("OUT_X", OUT_X), arr("ROW_Y", ROW_Y), v("ALL_JACK_Y", ALL_JACK_Y),
          v("TIME_X", TIME_X), v("OCT_X", OCT_X), v("HOLD_X", HOLD_X), v("LED_X", LED_X), v("SMALL_R", SMALL_R),
          v("HOLD_W", HOLD_W), v("HOLD_H", HOLD_H), v("LED_R", LED_R), v("STRIKE_X", STRIKE_X), ""]
    L += ["enum LabelFont { FONT_REG, FONT_BOLD, FONT_MIC };",
          "static const char* const FONT_FILES[3] = {%s};" % ", ".join('"res/fonts/%s"' % FONT_FILES[k] for k in ["reg", "bold", "mic"]),
          "// Taille NanoVG = em / FONT_EM : NanoVG prend la taille pour la hauteur de ligne, pas pour le em",
          "static const float FONT_EM[3] = {%s};" % ", ".join("%.4ff" % FONT_EM[k] for k in ["reg", "bold", "mic"]), "",
          "struct Label { float x, y; const char* text; float size; NVGcolor color; int align; float spacing; int font; };",
          "static const Label LABELS[] = {"]
    for (x, y, t, s, c, a, sp, fo) in LABELS:
        al = {"center": "NVG_ALIGN_CENTER", "left": "NVG_ALIGN_LEFT", "right": "NVG_ALIGN_RIGHT"}[a]
        fe = {"reg": "FONT_REG", "bold": "FONT_BOLD", "mic": "FONT_MIC"}[fo]
        L.append('\t{%.3ff, %.3ff, "%s", %.2ff, %s, %s, %.2ff, %s},' % (x, y, t, s, cpp_color(c), al, sp, fe))
    L += ["};", "", "} // namespace ll", ""]
    with open(os.path.join(ROOT, "src/LouisLayout.hpp"), "w") as f:
        f.write("\n".join(L))


def preview():
    """Fond rendu par AppKit, puis composants et contenus simulés, pour juger l'ensemble et repérer les chevauchements."""
    from PIL import Image, ImageDraw, ImageFont
    S = 8
    tmp = os.path.join(ROOT, "tools/preview")
    os.makedirs(tmp, exist_ok=True)
    png = os.path.join(tmp, "Louis.png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), os.path.join(ROOT, "res/Louis.svg"), png,
                    str(int(W * S)), str(int(H * S))], capture_output=True, check=True)
    img = Image.open(png).convert("RGBA")
    dr = ImageDraw.Draw(img)
    fonts, boxes = {}, []

    def font(px, fo):
        k = (px, fo)
        if k not in fonts:
            fonts[k] = ImageFont.truetype(os.path.join(ROOT, "res/fonts", FONT_FILES[fo]), max(1, int(px * 25.4 / 75 * S)))
        return fonts[k]

    def text(x, y, t, px, color, align="center", sp=0.0, fo="reg", record=True):
        f = font(px, fo)
        w = f.getlength(t) + sp * 25.4 / 75 * S * max(0, len(t) - 1)
        bb = f.getbbox("HAg")
        X = x * S - (w / 2 if align == "center" else (w if align == "right" else 0))
        Y = y * S - (bb[1] + bb[3]) / 2
        cx = X
        for ch in t:
            dr.text((cx, Y), ch, font=f, fill=color)
            cx += f.getlength(ch) + sp * 25.4 / 75 * S
            if fo != "mic":
                dr.text((cx - f.getlength(ch) - sp * 25.4 / 75 * S + 0.35 * 25.4 / 75 * S, Y), ch, font=f, fill=color)
        if record:
            tb = f.getbbox(t)
            boxes.append(("t:" + t, X, Y + tb[1], X + w, Y + tb[3]))

    def circle(x, y, r, fill, outline=None, name=None, width=1):
        dr.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline, width=width)
        if name:
            boxes.append((name, (x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S))

    def box(x0, y0, x1, y1, fill, outline=None, name=None, r=0.6):
        dr.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=min(r, (y1 - y0) * 0.45, (x1 - x0) * 0.45) * S, fill=fill, outline=outline)
        if name:
            boxes.append((name, x0 * S, y0 * S, x1 * S, y1 * S))

    def jack(x, y, name):
        # PJ301M de Rack : 8,13 mm hors tout ; on compte 7,6 mm (l'ombre déborde un peu de l'écrou)
        circle(x, y, 3.8, "#3a3733", "#cfc9bc", "j:" + name, 6)
        circle(x, y, 1.7, "#05070c")

    def knob(x, y, r, name):
        circle(x, y, r, "#24211e", "#0b0a09", "k:" + name)
        dr.line([x * S, (y - r * 0.3) * S, x * S, (y - r * 0.86) * S], fill="#f3ecdb", width=3)

    for (x, y, t, s, c, a, sp, fo) in LABELS:
        text(x, y, t, s, c, a, sp, fo)
    # Écran simulé
    text(SCREEN[0] + 2.4, SCREEN[1] + 2.6, "FREE TIMING", 4.2, PHOS, "left", 0.6, "mic", record=False)
    text(SCREEN[2] - 2.4, SCREEN[1] + 2.6, "MIN PENTA · C", 4.2, PHOS, "right", 0.6, "mic", record=False)
    for i in range(4):
        y = SCREEN[1] + 5.0 + i * 7.6 / 1.6
        text(SCREEN[0] + 2.2, y + 2.4, str(i + 1), 4.0, PHOS_DIM, "left", record=False)
    # Rangée du haut
    for k, x in enumerate(CLIM_X):
        knob(x, KNOB_Y, PACE_R if k == 0 else KNOB_R, CLIM_NAMES[k])
        jack(x, JACK_Y, "cv" + CLIM_NAMES[k])
    for k, x in enumerate(DRIFT_X):
        box(x - METER_W / 2, KNOB_Y - METER_H / 2, x + METER_W / 2, KNOB_Y + METER_H / 2, GLASS, "#3a3733", "m%d" % k, r=0.3)
        jack(x, JACK_Y, "drift%d" % k)
    for k, x in enumerate(PITCH_X):
        if k < 4:
            knob(x, KNOB_Y, KNOB_R, PITCH_NAMES[k])
        else:
            circle(x, KNOB_Y, WHEEL_R, GLASS, INK, "wheel")
        jack(x, JACK_Y, "p%d" % k)
    # LINK
    knob(*LINK_KNOB, "link")
    jack(LINK_KNOB[0], LINK_JACK_Y, "link")
    for k, (n, vb) in enumerate([("CHAIN", "next voice plays"), ("HUSH", "next voice rests"), ("ECHO", "next voice repeats")]):
        y0 = MODE_Y0 + k * (MODE_H + MODE_GAP)
        box(MODE_X0, y0, MODE_X1, y0 + MODE_H, INK if k == 0 else None, INK, "mode%d" % k, r=0.6)
        c = PLATE_TEXT if k == 0 else INK
        circle(MODE_X0 + 2.2, y0 + MODE_H / 2, 0.6, PHOS if k == 0 else "#2b3a30")
        text(MODE_X0 + 4.2, y0 + MODE_H * 0.31, n, 6.4, c, "left", 1.0, "bold", record=False)
        text(MODE_X0 + 4.2, y0 + MODE_H * 0.72, vb, 5.6, c, "left", 0.1, record=False)
    # CLOCK
    knob(*GRID_KNOB, "grid"); knob(*SNAP_KNOB, "snap")
    jack(GRID_KNOB[0], CLOCK_JACK_Y, "grid"); jack(SNAP_KNOB[0], CLOCK_JACK_Y, "snap")
    jack(CLOCK_IN[0], CLOCK_IN[2], "clock"); jack(RESET_IN[0], RESET_IN[2], "reset")
    # VOICES
    for i, y in enumerate(ROW_Y):
        knob(TIME_X, y, SMALL_R, "time%d" % i)
        knob(OCT_X, y, SMALL_R, "oct%d" % i)
        box(HOLD_X - HOLD_W / 2, y - HOLD_H / 2, HOLD_X + HOLD_W / 2, y + HOLD_H / 2, None, INK, "hold%d" % i, r=0.6)
        circle(LED_X, y, LED_R, "#2b3a30", INK, "led%d" % i)
        for k, x in enumerate(OUT_X):
            jack(x, y, "o%d%d" % (i, k))
    for k, x in enumerate(OUT_X):
        jack(x, ALL_JACK_Y, "poly%d" % k)
    jack(STRIKE_X, ALL_JACK_Y, "strike"); jack(HOLD_X, ALL_JACK_Y, "freeze"); jack(LED_X, ALL_JACK_Y, "any")
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
    print(os.path.join(ROOT, "res/Louis.svg"))
    print(os.path.join(ROOT, "src/LouisLayout.hpp"))
    if "--preview" in sys.argv:
        preview()
