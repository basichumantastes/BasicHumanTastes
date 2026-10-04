#!/usr/bin/env python3
"""Dessine le fond du panneau de Lucienne (res/Lucienne.svg) : un schéma vivant, traits cuivre sur encre.

Les coordonnées (mm) reprennent celles de src/Lucienne.cpp ; si l'on déplace un composant là-bas, on le déplace ici.
Les traits sont légèrement tremblés, comme tracés à la main. Rack lit les SVG avec nanosvg : pas de motifs,
pas de filtres, pas de texte (les textes sont dessinés par le module).

Usage : python3 tools/lucienne_panel.py            écrit res/Lucienne.svg
        python3 tools/lucienne_panel.py --preview  écrit aussi tools/preview/Lucienne.png (composants et textes simulés)
"""
import math, os, random, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 284.48, 128.5

INK_A, INK_B = "#1b1e2e", "#0f1119"
COPPER, VERDIGRIS, BONE, INK = "#d08c5c", "#7fc0ac", "#ece2cf", "#12141f"

# --- Disposition (identique à src/Lucienne.cpp)
HEX_X, HEX_Y, HEX_R = 142.24, 46.0, 27.0
DIAL_X, DIAL_Y = (24.0, 260.48), 48.0
DIAL_LABEL_R, DIAL_ARC_R = 18.5, 13.4
SWEEP = 0.83 * math.pi
SLOTS = 12
WINGS = [(80.0, 28.0, "SERRAGE"), (80.0, 62.0, "ÉCART"), (204.48, 28.0, "COURANT"), (204.48, 62.0, "SOUFFLE")]
BAND_Y = 90.0
BAND_X = [59.24, 72.24, 85.24, 98.24, 116.24, 129.24, 142.24, 155.24, 168.24, 186.24, 199.24, 212.24, 225.24]
BAND_NAMES = ["HAUTEUR", "TIMBRE", "DÉRIVE", "HALO", "FRÉQ A", "RÉSO A", "ROUTAGE", "FRÉQ B", "RÉSO B", "CADENCE", "DENSITÉ", "BOUCLE", "MÉMOIRE"]
JACK_X = [59.74 + 16.5 * i for i in range(11)]
IN_Y, OUT_Y = 104.0, 116.0
IN_NAMES = ["V/OCT", "TRIG", "ÉCART", "SERRAGE", "COURANT", "SOUFFLE", "HALO", "FILTRE A", "FILTRE B", "DENSITÉ", "HORLOGE"]
OUT_NAMES = ["1", "2", "3", "4", "5", "6", "PAS", "TOP", "RAIL", "L", "R"]
STRIKE = (170.0, 12.0)
RAIL_Y = 78.5
RAIL_LABELS = [(52.5, "VOIX"), (109.5, "FILTRES"), (179.5, "REGISTRE")]
SLOT_NAMES = [["HAUT", "ÉCART", "SERR", "COUR", "SOUF", "DENS", "HALO", "FLT B", "FLT A", "TIMB", "VIT·B", "DST·B"],
              ["HAUT", "ÉCART", "SERR", "COUR", "SOUF", "DENS", "HALO", "FLT B", "FLT A", "TIMB", "VIT·A", "DST·A"]]

# Rayons des composants (mm) et des anneaux cuivre qui les entourent
R_LARGE, R_MED, R_SMALL, R_JACK = 9.15, 6.1, 3.9, 4.2
RING_LARGE, RING_MED, RING_SMALL = 11.0, 7.6, 5.2


def voice(i, r=HEX_R):
    a = i * math.pi / 3
    return HEX_X + r * math.sin(a), HEX_Y - r * math.cos(a)


def number_pos(i):
    """Numéro de voix : la borne de l'écran, sur le rayon de sa voix."""
    return voice(i, 17.8)


def slot_angle(s):
    return -SWEEP + s * 2 * SWEEP / (SLOTS - 1)


def polar(cx, cy, r, a):
    return cx + r * math.sin(a), cy - r * math.cos(a)


rng = random.Random(1924)


def smooth(points, closed=False):
    """Chemin lissé (Catmull-Rom en courbes de Bézier) passant par les points."""
    pts = points + points[:3] if closed else [points[0]] + points + [points[-1]]
    d = "M%.3f %.3f" % pts[1]
    for k in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[k - 1], pts[k], pts[k + 1], pts[k + 2]
        c1 = (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6)
        d += " C%.3f %.3f %.3f %.3f %.3f %.3f" % (c1 + c2 + p2)
    return d + (" Z" if closed else "")


def hand_line(a, b, wobble=0.1):
    """Segment tremblé : points intermédiaires déplacés un peu perpendiculairement."""
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    n = max(2, int(L / 3.0))
    nx, ny = -(b[1] - a[1]) / (L or 1), (b[0] - a[0]) / (L or 1)
    pts = []
    for k in range(n + 1):
        t = k / n
        j = 0 if k in (0, n) else rng.gauss(0, wobble)
        pts.append((a[0] + (b[0] - a[0]) * t + nx * j, a[1] + (b[1] - a[1]) * t + ny * j))
    return smooth(pts)


def hand_poly(points, wobble=0.1):
    return " ".join(hand_line(points[k], points[k + 1], wobble) for k in range(len(points) - 1))


def hand_circle(cx, cy, r, wobble=0.07):
    n = max(12, int(2 * math.pi * r / 2.2))
    off = rng.uniform(0, 2 * math.pi)
    pts = []
    for k in range(n):
        a = off + 2 * math.pi * k / n
        rr = r + rng.gauss(0, wobble)
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return smooth(pts, closed=True)


def toward(a, b, d):
    """Point à la distance d de a, en direction de b."""
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    return a[0] + (b[0] - a[0]) * d / L, a[1] + (b[1] - a[1]) * d / L


out = []


def path(d, color=COPPER, width=0.3, opacity=0.85, dash=None, fill="none"):
    extra = ' stroke-dasharray="%s"' % dash if dash else ""
    out.append('<path d="%s" fill="%s" stroke="%s" stroke-width="%.2f" stroke-opacity="%.2f" stroke-linecap="round"%s/>'
               % (d, fill, color, width, opacity, extra))


def dot(x, y, r=0.45, color=COPPER, opacity=0.9):
    out.append('<circle cx="%.3f" cy="%.3f" r="%.2f" fill="%s" fill-opacity="%.2f"/>' % (x, y, r, color, opacity))


def text_width(text, size_px):
    """Largeur (mm) d'un texte Michroma à la taille de police de Rack (px à 100 %)."""
    try:
        from PIL import ImageFont
        f = ImageFont.truetype(os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf"), 100)
        return f.getlength(text) / 100 * size_px * 25.4 / 75
    except Exception:
        return len(text) * 0.85 * size_px * 25.4 / 75


def build():
    out.append('<?xml version="1.0" encoding="UTF-8"?>')
    out.append('<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H))
    out.append('<!-- Lucienne, 56HP : un schéma vivant, cuivre et vert-de-gris sur encre. Généré par tools/lucienne_panel.py -->')
    out.append('<defs><radialGradient id="ink" cx="0.5" cy="0.4" r="0.75"><stop offset="0" stop-color="%s"/>'
               '<stop offset="1" stop-color="%s"/></radialGradient></defs>' % (INK_A, INK_B))
    out.append('<rect x="0" y="0" width="%g" height="%g" fill="url(#ink)"/>' % (W, H))

    # Grille de plan, à peine visible, au pas de la grille de Rack
    g = []
    x = 5.08
    while x < W:
        g.append("M%.2f 0 V%.2f" % (x, H)); x += 5.08
    y = 5.08
    while y < H:
        g.append("M0 %.2f H%.2f" % (y, W)); y += 5.08
    path(" ".join(g), color="#8fa6c8", width=0.08, opacity=0.06)

    # --- Le nœud : écran, rayons, hexagone de l'anneau avec le sens de circulation
    path(hand_circle(HEX_X, HEX_Y, 15.2), width=0.35)
    path(hand_circle(HEX_X, HEX_Y, 14.2, 0.03), width=0.15, opacity=0.5, dash="0.6 0.9")
    for i in range(6):
        vx, vy = voice(i)
        path(hand_circle(vx, vy, RING_MED), width=0.3)
        dot(*voice(i, 15.2), r=0.35)
        # Lien vers la voix suivante, d'anneau à anneau, avec un chevron au milieu
        n = voice((i + 1) % 6)
        p, q = toward((vx, vy), n, RING_MED), toward(n, (vx, vy), RING_MED)
        path(hand_line(p, q), width=0.3)
        mx, my = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        ang = math.atan2(q[1] - p[1], q[0] - p[0])
        s = 0.9
        c1 = (mx - s * math.cos(ang - 0.6), my - s * math.sin(ang - 0.6))
        c2 = (mx - s * math.cos(ang + 0.6), my - s * math.sin(ang + 0.6))
        path("M%.3f %.3f L%.3f %.3f L%.3f %.3f" % (c1 + (mx, my) + c2), width=0.3)
    # La voix 1 est la référence : un petit repère au-dessus
    rx, ry = voice(0, HEX_R + RING_MED + 1.4)
    path("M%.2f %.2f L%.2f %.2f L%.2f %.2f Z" % (rx - 1, ry - 0.8, rx + 1, ry - 0.8, rx, ry + 0.6), fill=COPPER, opacity=0.8, width=0.1)

    # --- Les ailes et leurs liaisons vers le nœud
    for (wx, wy, _) in WINGS:
        path(hand_circle(wx, wy, RING_LARGE), width=0.35)
        path(hand_circle(wx, wy, RING_LARGE + 1.0, 0.04), width=0.12, opacity=0.4)
    links = [((80.0, 28.0), 5), ((80.0, 62.0), 4), ((204.48, 28.0), 1), ((204.48, 62.0), 2)]
    for (w, i) in links:
        v = voice(i)
        a, b = toward(w, v, RING_LARGE), toward(v, w, RING_MED)
        path(hand_line(a, b), width=0.3)
        dot(*a); dot(*b)
    # SOUFFLE porte un condensateur sur sa liaison (la constante de temps des portes)
    v = voice(2)
    a, b = toward((204.48, 62.0), v, RING_LARGE), toward(v, (204.48, 62.0), RING_MED)
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    out.append('<rect x="%.2f" y="%.2f" width="1.6" height="4" fill="%s"/>' % (mx - 0.8, my - 2, INK_A))
    path("M%.2f %.2f V%.2f M%.2f %.2f V%.2f" % (mx - 0.6, my - 1.8, my + 1.8, mx + 0.6, my - 1.8, my + 1.8), width=0.35)

    # COURANT descend jusqu'au rail par une résistance
    top, bottom = (223.0, 28.0), (223.0, RAIL_Y)
    path(hand_line(toward((204.48, 28.0), top, RING_LARGE), top), width=0.3)
    zig_top, zig_bot = 44.0, 52.0
    path(hand_line(top, (223.0, zig_top)), width=0.3)
    zz = [(223.0, zig_top)]
    for k in range(1, 8):
        zz.append((223.0 + (1.0 if k % 2 else -1.0), zig_top + k * (zig_bot - zig_top) / 8))
    zz.append((223.0, zig_bot))
    path("M" + " L".join("%.2f %.2f" % p for p in zz), width=0.3)
    path(hand_line((223.0, zig_bot), bottom), width=0.3)
    dot(223.0, 28.0); dot(*bottom, r=0.55)

    # --- Le rail : une double ligne qui traverse le centre, interrompue par les étiquettes des sections
    gaps = [(x - 1.2, x + text_width(t, 4.6) + 1.2) for (x, t) in RAIL_LABELS]
    vx, vy = voice(3)
    gaps.append((vx - RING_MED - 0.3, vx + RING_MED + 0.3))
    gaps.sort()
    segs, x0 = [], 48.5
    for (a, b) in gaps:
        segs.append((x0, a)); x0 = b
    segs.append((x0, 236.0))
    for (a, b) in segs:
        if b - a > 0.5:
            path(hand_line((a, RAIL_Y - 0.35), (b, RAIL_Y - 0.35), 0.05), color=VERDIGRIS, width=0.25, opacity=0.8)
            path(hand_line((a, RAIL_Y + 0.35), (b, RAIL_Y + 0.35), 0.05), color=VERDIGRIS, width=0.12, opacity=0.5)
    for xe in (48.5, 236.0):
        out.append('<circle cx="%.2f" cy="%.2f" r="0.9" fill="none" stroke="%s" stroke-width="0.25"/>' % (xe, RAIL_Y, VERDIGRIS))

    # --- La bande du bas : un anneau par potard, des cloisons fines entre les groupes
    for x in BAND_X:
        path(hand_circle(x, BAND_Y, RING_SMALL, 0.05), width=0.25, opacity=0.7)
    for x in (107.24, 177.24):
        path(hand_line((x, 84.0), (x, 95.0), 0.04), width=0.15, opacity=0.5)

    # --- Les entrées : de petits anneaux ; les sorties : une plaque os
    for x in JACK_X:
        path(hand_circle(x, IN_Y, 5.0, 0.04), width=0.2, opacity=0.55)
    out.append('<rect x="%.2f" y="108.3" width="%.2f" height="14.2" rx="1.4" fill="%s"/>' % (JACK_X[0] - 7.4, JACK_X[-1] - JACK_X[0] + 14.8, BONE))
    # La frappe : un anneau, relié au nœud par un trait qui descend vers la voix 2
    path(hand_circle(STRIKE[0], STRIKE[1], 4.4, 0.05), width=0.3)
    v = voice(1)
    a, b = toward(STRIKE, v, 4.4), toward(v, STRIKE, RING_MED)
    path(hand_line(a, b), width=0.2, opacity=0.6, dash="0.8 0.8")

    # --- Les LFO : anneaux, cadran à douze crans, ligne de modulation vers le nœud
    for li, cx in enumerate(DIAL_X):
        accent = COPPER if li == 0 else VERDIGRIS
        path(hand_circle(cx - 9.0, 19.5, RING_MED), color=accent, width=0.3)
        path(hand_circle(cx + 11.0, 19.5, RING_SMALL, 0.05), color=accent, width=0.25, opacity=0.7)
        # Le cadran : piste de l'arc, crans, repère de la brèche entre le dernier et le premier
        arc = [polar(cx, DIAL_Y, DIAL_ARC_R, -SWEEP + k * 2 * SWEEP / 80) for k in range(81)]
        path(smooth(arc), color=accent, width=0.15, opacity=0.35)
        for s in range(SLOTS):
            a = slot_angle(s)
            path("M%.3f %.3f L%.3f %.3f" % (polar(cx, DIAL_Y, 10.4, a) + polar(cx, DIAL_Y, 11.9, a)), color=accent, width=0.3)
        path(hand_circle(cx, DIAL_Y, RING_LARGE - 0.6, 0.03), color=accent, width=0.12, opacity=0.4)
        for k in range(3):
            path(hand_circle(cx + (k - 1) * 14.5, 80.0, RING_SMALL, 0.05), color=accent, width=0.25, opacity=0.7)
        for k in range(3):
            jx, jy = cx + (8.0 if k % 2 else -8.0), IN_Y if k < 2 else OUT_Y
            path(hand_circle(jx, jy, 5.0, 0.04), color=accent, width=0.2, opacity=0.55)
        ox = cx + 8.0
        out.append('<rect x="%.2f" y="108.3" width="12" height="14.2" rx="1.4" fill="%s"/>' % (ox - 6.0, BONE))
        # La ligne de modulation : du cadran vers le nœud, en pointillés, avec une flèche
        if li == 0:
            a, b = (47.0, HEX_Y), (HEX_X - 15.6, HEX_Y)
        else:
            a, b = (237.5, HEX_Y), (HEX_X + 15.6, HEX_Y)
        path(hand_line(a, b, 0.06), color=accent, width=0.3, opacity=0.75, dash="1.4 1.1")
        d = 1 if b[0] > a[0] else -1
        path("M%.2f %.2f L%.2f %.2f L%.2f %.2f" % (b[0] - d * 1.2, b[1] - 0.8, b[0], b[1], b[0] - d * 1.2, b[1] + 0.8), color=accent, width=0.35)

    out.append("</svg>")
    with open(os.path.join(ROOT, "res/Lucienne.svg"), "w") as f:
        f.write("\n".join(out) + "\n")


def preview():
    """Fond rendu par AppKit, puis composants et textes simulés, pour juger l'ensemble et repérer les chevauchements."""
    from PIL import Image, ImageDraw, ImageFont
    S = 6  # px par mm
    tmp = os.path.join(ROOT, "tools/preview")
    os.makedirs(tmp, exist_ok=True)
    png = os.path.join(tmp, "Lucienne.png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), os.path.join(ROOT, "res/Lucienne.svg"), png,
                    str(int(W * S)), str(int(H * S))], capture_output=True, check=True)
    img = Image.open(png).convert("RGBA")
    dr = ImageDraw.Draw(img)
    fonts = {}
    boxes = []

    def font(px):
        if px not in fonts:
            fonts[px] = ImageFont.truetype(os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf"), int(px * 25.4 / 75 * S))
        return fonts[px]

    def label(x, y, t, px, color=BONE):
        f = font(px)
        w = f.getlength(t)
        bb = f.getbbox(t)
        X, Y = x * S - w / 2, y * S - (bb[1] + bb[3]) / 2
        dr.text((X, Y), t, font=f, fill=color)
        boxes.append(("t:" + t, X, Y + bb[1], X + w, Y + bb[3]))

    def knob(x, y, r, color, name):
        dr.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=color, outline="#000000")
        dr.line([x * S, y * S, x * S, (y - r * 0.8) * S], fill="#333333" if color != "#222222" else "#dddddd", width=2)
        boxes.append(("k:" + name, (x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S))

    def jack(x, y, name):
        dr.ellipse([(x - R_JACK) * S, (y - R_JACK) * S, (x + R_JACK) * S, (y + R_JACK) * S], fill="#9a9a9a", outline="#333333")
        dr.ellipse([(x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S], fill="#111111")
        boxes.append(("j:" + name, (x - R_JACK) * S, (y - R_JACK) * S, (x + R_JACK) * S, (y + R_JACK) * S))

    label(HEX_X, 5.0, "LUCIENNE", 11)
    knob(STRIKE[0], STRIKE[1], 3.0, "#dddddd", "frappe")
    f = font(5); w = f.getlength("FRAPPE")
    label(175.0 + w / S / 2, 12.0, "FRAPPE", 5, COPPER)
    for i in range(6):
        knob(*voice(i), R_MED, "#e8e4da", "v%d" % i)
        label(*number_pos(i), str(i + 1), 5, COPPER)
    for (x, y, n) in WINGS:
        label(x, y - 13.0, n, 7)
        knob(x, y, R_LARGE, "#e8e4da", n)
    for x, n in zip(BAND_X, BAND_NAMES):
        label(x, BAND_Y - 6.5, n, 4.6)
        knob(x, BAND_Y, R_SMALL, "#222222", n)
    for (x, t) in RAIL_LABELS:
        f = font(4.6); w = f.getlength(t)
        label(x + w / S / 2, RAIL_Y, t, 4.6, COPPER)
    for x, a, b in zip(JACK_X, IN_NAMES, OUT_NAMES):
        label(x, IN_Y - 6.5, a, 4.4); jack(x, IN_Y, a)
        label(x, OUT_Y - 6.5, b, 4.4, INK); jack(x, OUT_Y, b)
    for li, cx in enumerate(DIAL_X):
        accent = COPPER if li == 0 else VERDIGRIS
        label(cx, 5.0, "LFO A" if li == 0 else "LFO B", 7, accent)
        label(cx - 9, 11.5, "VITESSE", 4.6); knob(cx - 9, 19.5, R_MED, "#e8e4da", "rate%d" % li)
        label(cx + 11, 11.5, "FORME", 4.6); knob(cx + 11, 19.5, R_SMALL, "#222222", "shape%d" % li)
        for s in range(SLOTS):
            px, py = polar(cx, DIAL_Y, DIAL_LABEL_R, slot_angle(s))
            label(px, py, SLOT_NAMES[li][s], 4)
        knob(cx, DIAL_Y, R_LARGE, "#e8e4da", "dest%d" % li)
        for k, n in enumerate(["LARGEUR", "PROFONDEUR", "ÉVENTAIL"]):
            kx = cx + (k - 1) * 14.5
            label(kx, 73.5, n, 4); knob(kx, 80.0, R_SMALL, "#222222", n + str(li))
        for k, n in enumerate(["IN", "VITESSE", "DEST", "OUT"]):
            jx, jy = cx + (8.0 if k % 2 else -8.0), IN_Y if k < 2 else OUT_Y
            label(jx, jy - 6.5, n, 4.4, INK if k == 3 else BONE); jack(jx, jy, n + str(li))
    # Vis
    for (x, y) in [(7.62, 2.54), (W - 7.62, 2.54), (7.62, H - 2.54), (W - 7.62, H - 2.54)]:
        dr.ellipse([(x - 2.4) * S, (y - 2.4) * S, (x + 2.4) * S, (y + 2.4) * S], fill="#bbbbbb")
        boxes.append(("vis", (x - 2.4) * S, (y - 2.4) * S, (x + 2.4) * S, (y + 2.4) * S))

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
    print(os.path.join(ROOT, "res/Lucienne.svg"))
    if "--preview" in sys.argv:
        preview()
