#!/usr/bin/env python3
"""Aperçu des panneaux hors de Rack : fond SVG + composants + textes, et détection des chevauchements.

Lit les appels addLabel(...) et create...Centered<...>(mm2px(Vec(x, y))) dans src/<Module>.cpp
(boucles for simples déroulées), mesure les textes avec la vraie police Michroma et écrit
tools/preview/<Module>.png. Usage : python3 tools/preview.py Ernest Marcel Jules
Avec --clean : rendu propre en vraies couleurs (boutons, jacks, écrans d'exemple) dans docs/images/,
pour les manuels.
"""
import os, re, subprocess, sys, tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf")
PX_PER_MM = 75 / 25.4          # échelle de Rack à 100 %
SCALE = 4                       # suréchantillonnage de l'aperçu
# Rayons approximatifs des composants (mm)
RADIUS = {"LightButton": 2.9, "RoundBigBlackKnob": 7.6, "RoundLargeBlackKnob": 6.4, "RoundBlackKnob": 4.8, "RoundBlackSnapKnob": 4.8,
          "RoundSmallBlackKnob": 3.9, "Trimpot": 2.4, "PJ301MPort": 4.2, "ScrewSilver": 2.5}
BOXES = {"CKSS": (2.4, 3.5), "CKSSThree": (2.3, 4.8)}

def expand_loops(src):
    """Déroule les boucles `for (int i = 0; i < N; i++) { ... }` et `for (...) une_ligne;`."""
    consts = {"SLOPES": 6}
    out, lines, k = [], src.split("\n"), 0
    while k < len(lines):
        m = re.match(r"\s*for \(int i = 0; i < (\w+); i\+\+\)\s*(\{)?\s*$", lines[k])
        if not m:
            out.append(lines[k]); k += 1; continue
        n = int(consts.get(m.group(1), m.group(1)))
        body, k = [], k + 1
        if m.group(2):
            depth = 1
            while depth:
                depth += lines[k].count("{") - lines[k].count("}")
                if depth: body.append(lines[k])
                k += 1
        else:
            body.append(lines[k]); k += 1
        for i in range(n):
            out += [re.sub(r"\bi\b", str(i), b) for b in body]
    return "\n".join(out)

# Couleurs des textes par module (constantes NVG et booléens des signatures d'addLabel)
COLORS = {
    "Ernest": {"default": "#26231f", "True": "#f4ecd8"},
    "Marcel": {"default": "#efe6d2", "True": "#1d2433"},
    "Jules": {"default": "#f3ecdc", "JULES_CREAM": "#f3ecdc", "JULES_CORAL": "#e8765c", "JULES_INK": "#1c2a24"},
    "Odette": {"default": "#161618", "ODETTE_INK": "#161618", "ODETTE_IVORY": "#f2efe8", "ODETTE_TEAL": "#2a7f7a"},
}

def evaluate(expr, env, colors=None):
    expr = re.sub(r"(\d+\.?\d*)f\b", r"\1", expr)
    if colors is not None:
        expr = expr.replace("true", "True").replace("false", "False")
        env = dict(env, **{k: v for k, v in colors.items()})
    expr = re.sub(r'string::f\("([^"]*)", ([^)]*)\)', lambda m: repr(m.group(1).replace("%d", "{}")) + ".format(" + m.group(2) + ")", expr)
    # Les couleurs ne servent pas à l'aperçu, et les conditions C (a ? b : c) deviennent du Python
    if colors is None:
        expr = re.sub(r"\b[A-Z]+_(?:CREAM|CORAL|INK|IVORY|TEAL)\b", "0", expr)
    m = re.fullmatch(r"\s*(.+?) \? (.+?) : (.+)", expr)
    if m:
        expr = f"({m.group(2)} if {m.group(1)} else {m.group(3)})"
    return eval(expr, {}, env)

def parse(module):
    src = open(os.path.join(ROOT, "src", module + ".cpp")).read()
    env = {}
    for name, body in re.findall(r"static const (?:float|char\* const) (\w+)\[\d+\] = \{([^}]*)\};", src):
        env[name] = evaluate("[" + body + "]", {})
    widget = src[src.index("Widget(" + module + "* module)"):]
    widget = expand_loops(widget)
    labels, parts = [], []
    for m in re.finditer(r"addLabel\(Vec\(([^,]+), ([^)]+)\), (.+?)\);", widget):
        args = [a.strip() for a in re.split(r",(?![^(]*\))", m.group(3))]
        text = evaluate(args[0], env)
        idx = size_index(src)
        size = evaluate(args[idx], env) if len(args) > idx else default_size(src)
        align = "left" if "NVG_ALIGN_LEFT" in m.group(3) else "center"
        cidx = color_index(src)
        palette = COLORS.get(module, {"default": "#000000"})
        color = palette["default"]
        if cidx is not None and len(args) > cidx:
            value = evaluate(args[cidx], env, palette)
            color = palette.get(str(value), value) if not str(value).startswith("#") else value
            if value is False:
                color = palette["default"]
        labels.append((evaluate(m.group(1), env), evaluate(m.group(2), env), text, size, align, color))
    for m in re.finditer(r"create(\w*)Centered<(?:\w+<)*(\w+)>*\(mm2px\(Vec\(([^,]+), ([^)]+)\)\)", widget):
        # Les boutons à voyant (createLightParamCentered) se dessinent comme des boutons
        kind = "LightButton" if m.group(1) == "LightParam" else m.group(2)
        parts.append((kind, evaluate(m.group(3), env), evaluate(m.group(4), env)))
    for m in re.finditer(r"addDolby\((\S+)f, (\S+)f,", widget):
        pass
    return labels, parts

def size_index(widget):
    """Rang de l'argument fontSize après le texte (la signature d'addLabel varie d'un module à l'autre)."""
    sig = re.search(r"void addLabel\(Vec posMm, std::string text,?(.*)\) \{", widget).group(1)
    names = [a.strip().split("=")[0].split()[-1] for a in sig.split(",") if a.strip()]
    return 1 + names.index("fontSize")

def color_index(widget):
    """Rang de l'argument de couleur (light, dark ou color) après le texte, s'il existe."""
    sig = re.search(r"void addLabel\(Vec posMm, std::string text,?(.*)\) \{", widget).group(1)
    names = [a.strip().split("=")[0].split()[-1] for a in sig.split(",") if a.strip()]
    for n in ("light", "dark", "color"):
        if n in names:
            return 1 + names.index(n)
    return None

def default_size(widget):
    m = re.search(r"void addLabel\([^)]*float fontSize = ([\d.]+)f", widget)
    return float(m.group(1)) if m else 6.0

def render(module):
    labels, parts = parse(module)
    svg = os.path.join(ROOT, "res", module + ".svg")
    w_mm = float(re.search(r'width="([\d.]+)mm"', open(svg).read()).group(1))
    h_mm = 128.5
    W, H = int(w_mm * PX_PER_MM * SCALE), int(h_mm * PX_PER_MM * SCALE)
    tmp = tempfile.mkdtemp()
    png = os.path.join(tmp, module + ".png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), svg, png, str(W), str(H)], capture_output=True)
    bg = Image.open(png).convert("RGB")
    d = ImageDraw.Draw(bg)
    k = PX_PER_MM * SCALE
    boxes = []
    for kind, x, y in parts:
        if kind in BOXES:
            hw, hh = BOXES[kind]
        else:
            r = RADIUS.get(kind, 1.2 if "Light" in kind else 3.0)
            hw = hh = r
        d.ellipse([(x - hw) * k, (y - hh) * k, (x + hw) * k, (y + hh) * k], outline=(120, 200, 255), width=SCALE)
        boxes.append(("composant " + kind, x - hw, y - hh, x + hw, y + hh))
    for x, y, text, size, align, _ in labels:
        font = ImageFont.truetype(FONT, int(size * SCALE))
        l, t, r, b = d.textbbox((0, 0), text, font=font)
        tw, th = (r - l) / SCALE / PX_PER_MM, (b - t) / SCALE / PX_PER_MM
        x0 = x if align == "left" else x - tw / 2
        d.text((x0 * k - l, (y - th / 2) * k - t), text, font=font, fill=(255, 80, 80))
        boxes.append(("texte " + repr(text), x0, y - th / 2, x0 + tw, y + th / 2))
    # Chevauchements texte/texte et texte/composant
    problems = []
    for i, a in enumerate(boxes):
        if not a[0].startswith("texte"): continue
        if a[1] < 0.8 or a[3] > w_mm - 0.8: problems.append(f"{a[0]} déborde du panneau")
        for j, b in enumerate(boxes):
            if j <= i and b[0].startswith("texte"): continue
            if j == i: continue
            if a[1] < b[3] - 0.15 and b[1] < a[3] - 0.15 and a[2] < b[4] - 0.15 and b[2] < a[4] - 0.15:
                problems.append(f"{a[0]} touche {b[0]}")
    os.makedirs(os.path.join(ROOT, "tools/preview"), exist_ok=True)
    out = os.path.join(ROOT, "tools/preview", module + ".png")
    bg.resize((W // 2, H // 2)).save(out)
    print(f"{module} : {len(labels)} textes, {len(parts)} composants, tailles {sorted(set(round(l[3],1) for l in labels))} -> {out}")
    for p in problems: print("   !", p)

def hexrgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))

def draw_display(module, d, k, img):
    """Écrans d'exemple, comme dans Rack avec les réglages par défaut."""
    import math
    font = lambda size: ImageFont.truetype(FONT, int(size * SCALE))
    if module == "Ernest":
        seg = ImageFont.truetype(os.path.join(ROOT, "res/fonts/DSEG14Classic-Bold.ttf"), int(10 * SCALE))
        x0, y0, w, h = 6 * k, 14 * k, 48.96 * k, 10 * k
        px = SCALE  # 1 px Rack
        for text, x, y, anchor in (("ENVELOPE", x0 + 5 * px, y0 + 0.3 * h, "lm"), ("55HZ", x0 + w - 5 * px, y0 + 0.72 * h, "rm")):
            ghost = "~" * 8
            d.text((x, y), ghost, font=seg, fill=(0x12, 0x30, 0x2a), anchor=anchor)
            d.text((x, y), text, font=seg, fill=(0x5f, 0xf2, 0xd6), anchor=anchor)
    elif module == "Marcel":
        cx, cy, size = 30.48 * k, 29 * k, 32 * k
        r0, span = size * 0.26, size * 0.22
        d.ellipse([cx - r0, cy - r0, cx + r0, cy + r0], outline=(0x6b, 0x55, 0x2c), width=SCALE)
        for i in range(128):
            amp = 0.5 + 0.4 * math.sin(i * 0.3) * math.sin(i * 0.07)
            a = 2 * math.pi * i / 128 - math.pi / 2
            d.line([cx + r0 * math.cos(a), cy + r0 * math.sin(a), cx + (r0 + span * amp) * math.cos(a), cy + (r0 + span * amp) * math.sin(a)], fill=(0xd9, 0xa4, 0x41), width=int(1.1 * SCALE))
        rh = r0 - 3.5 * SCALE
        d.ellipse([cx - 2.2 * SCALE, cy - rh - 2.2 * SCALE, cx + 2.2 * SCALE, cy - rh + 2.2 * SCALE], fill=(255, 255, 255))
    elif module == "Jules":
        cols = [9, 21.24, 33.48, 45.72, 57.96, 70.2, 82.44]
        top, bottom = (12 + 5) * k, (12 + 19 - 3) * k
        levels = [0.85, 0.6, 0.72, 0.4, 0.55, 0.3, 0.85]
        for i, x in enumerate(cols):
            x0, x1 = (x - 2) * k, (x + 2) * k
            d.rectangle([x0, top, x1, bottom], fill=(0x24, 0x33, 0x2c))
            d.rectangle([x0, bottom - (bottom - top) * levels[i], x1, bottom], fill=hexrgb("#e8765c") if i < 6 else hexrgb("#f3ecdc"))
        d.text(((4 + 83.44 - 2) * k, (12 + 0.8) * k), "LOOP / CV", font=font(8), fill=hexrgb("#f3ecdc"), anchor="ra")
    elif module == "Odette":
        x0, y0, w, h = 6 * k, 12.5 * k, 79.44 * k, 14 * k
        cell = (w - 4 * k) / 8
        for z in range(8):
            t = z / 7
            col = (int(0x9f - 0x80 * t), int(0xd8 - 0x70 * t), int(0xd2 - 0x68 * t))
            if z != 3:
                col = tuple(int(c * 0.2 + 0x16 * 0.8) for c in col)
            cx = x0 + 2 * k + z * cell
            d.rectangle([cx + SCALE, y0 + 1.5 * k, cx + cell - SCALE, y0 + 5.5 * k], fill=col)
        d.text((x0 + 2.5 * k, y0 + h - 1.2 * k), "SIZE 4", font=font(9), fill=hexrgb("#f2efe8"), anchor="ld")
        d.text((x0 + w - 2.5 * k, y0 + h - 1.2 * k), "326.6 MS", font=font(9), fill=hexrgb("#f2efe8"), anchor="rd")

def render_clean(module):
    labels, parts = parse(module)
    svg = os.path.join(ROOT, "res", module + ".svg")
    w_mm = float(re.search(r'width="([\d.]+)mm"', open(svg).read()).group(1))
    W, H = int(w_mm * PX_PER_MM * SCALE), int(128.5 * PX_PER_MM * SCALE)
    tmp = tempfile.mkdtemp()
    png = os.path.join(tmp, module + ".png")
    subprocess.run(["swift", os.path.join(ROOT, "tools/svg2png.swift"), svg, png, str(W), str(H)], capture_output=True)
    img = Image.open(png).convert("RGB")
    d = ImageDraw.Draw(img)
    k = PX_PER_MM * SCALE
    draw_display(module, d, k, img)
    circle = lambda x, y, r, **kw: d.ellipse([(x - r) * k, (y - r) * k, (x + r) * k, (y + r) * k], **kw)
    # Vis aux quatre coins
    for x in (7.62, w_mm - 7.62):
        for y in (2.54, 128.5 - 2.54):
            circle(x, y, 1.7, fill=(0xb8, 0xba, 0xbe), outline=(0x80, 0x82, 0x86), width=SCALE)
            d.line([(x - 1.1) * k, y * k, (x + 1.1) * k, y * k], fill=(0x70, 0x72, 0x76), width=SCALE)
    for kind, x, y in parts:
        if "Knob" in kind:
            r = RADIUS.get(kind, 4.8)
            circle(x, y, r, fill=(0x1b, 0x1b, 0x1c))
            circle(x, y, r * 0.82, fill=(0x29, 0x29, 0x2b))
            d.line([x * k, y * k, x * k, (y - r * 0.85) * k], fill=(0xf0, 0xf0, 0xf0), width=int(0.5 * k))
        elif kind == "Trimpot":
            circle(x, y, 2.4, fill=(0xd6, 0xd6, 0xd8), outline=(0x90, 0x90, 0x94), width=SCALE)
            d.line([x * k, y * k, x * k, (y - 2.0) * k], fill=(0x30, 0x30, 0x30), width=int(0.4 * k))
        elif kind == "PJ301MPort":
            circle(x, y, 3.3, fill=(0xc9, 0xcb, 0xd0), outline=(0x8a, 0x8c, 0x90), width=SCALE)
            circle(x, y, 1.75, fill=(0x12, 0x12, 0x12))
        elif kind in ("CKSS", "CKSSThree"):
            hw, hh = (2.0, 3.4) if kind == "CKSS" else (2.0, 4.6)
            d.rounded_rectangle([(x - hw) * k, (y - hh) * k, (x + hw) * k, (y + hh) * k], radius=0.6 * k, fill=(0x2b, 0x2b, 0x2d))
            d.rounded_rectangle([(x - hw + 0.5) * k, (y - hh + 0.5) * k, (x + hw - 0.5) * k, (y - hh + 2.6) * k], radius=0.4 * k, fill=(0xe6, 0xe6, 0xe6))
        elif kind != "LightButton" and "Light" in kind:
            circle(x, y, 1.0, fill=(0x3c, 0x3c, 0x3e))
        else:
            # Boutons à voyant (VCVLightLatch, VCVLightBezel)
            circle(x, y, 2.9, fill=(0x9c, 0x9e, 0xa2))
            circle(x, y, 2.1, fill=(0xec, 0xec, 0xee))
    for x, y, text, size, align, color in labels:
        f = ImageFont.truetype(FONT, int(size * SCALE))
        d.text((x * k, y * k), text, font=f, fill=hexrgb(color), anchor="lm" if align == "left" else "mm")
    out_dir = os.path.join(ROOT, "docs/images")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, module + ".png")
    img.save(out)
    print(f"{module} -> {out} ({img.size[0]}x{img.size[1]})")

if __name__ == "__main__":
    args = sys.argv[1:]
    clean = "--clean" in args
    for m in [a for a in args if not a.startswith("--")] or ["Ernest", "Marcel", "Jules", "Odette"]:
        (render_clean if clean else render)(m)
