#!/usr/bin/env python3
"""Patine discrète pour un panneau SVG : vignettage, taches légèrement jaunies, rayures fines,
poussière et usure autour des vis. Le motif est tiré au hasard avec une graine fixe, donc
reproductible. Il est inséré entre deux marqueurs <!-- patina:start --> / <!-- patina:end -->
(remplacé s'il existe déjà), juste avant </svg>.

Rack (nanosvg) ne gère pas les filtres SVG : tout est fait de dégradés, de traits et d'opacités.
Usage : python3 tools/patina.py res/Marcel.svg [--seed 7] [--amount 1.0]
"""
import argparse, math, random, re

def patina(width, height, seed, amount, light, warm, soft):
    rnd = random.Random(seed)
    a = lambda x: f"{x * amount:.3f}"
    out = ['  <!-- patina:start (généré par tools/patina.py, ne pas éditer à la main) -->', '  <defs>',
           f'    <radialGradient id="patina-vignette" cx="0.5" cy="0.45" r="0.75">',
           f'      <stop offset="0.55" stop-color="#000000" stop-opacity="0"/>',
           f'      <stop offset="1" stop-color="#000000" stop-opacity="{a(0.22 * soft)}"/>',
           '    </radialGradient>',
           f'    <radialGradient id="patina-stain">',
           f'      <stop offset="0" stop-color="{warm}" stop-opacity="{a(0.10)}"/>',
           f'      <stop offset="1" stop-color="{warm}" stop-opacity="0"/>',
           '    </radialGradient>',
           f'    <radialGradient id="patina-rub">',
           f'      <stop offset="0" stop-color="{light}" stop-opacity="{a(0.10)}"/>',
           f'      <stop offset="1" stop-color="{light}" stop-opacity="0"/>',
           '    </radialGradient>',
           '  </defs>',
           f'  <rect x="0" y="0" width="{width}" height="{height}" fill="url(#patina-vignette)"/>']
    # Taches légèrement jaunies, grandes et très diffuses (leur flou peut brouiller le panneau : --soft 0 les retire)
    for _ in range(int(6 * soft + 0.5)):
        r = rnd.uniform(10, 22)
        out.append(f'  <circle cx="{rnd.uniform(0, width):.2f}" cy="{rnd.uniform(0, height):.2f}" r="{r:.2f}" fill="url(#patina-stain)"/>')
    # Usure autour des vis (là où les doigts et le tournevis passent)
    for x in (7.62, width - 7.62):
        for y in (2.54, height - 2.54):
            out.append(f'  <circle cx="{x:.2f}" cy="{y:.2f}" r="4.5" fill="url(#patina-rub)"/>')
    # Bords un peu frottés
    out.append(f'  <rect x="0.35" y="0.35" width="{width - 0.7:.2f}" height="{height - 0.7:.2f}" rx="0.6" fill="none" '
               f'stroke="{light}" stroke-opacity="{a(0.10)}" stroke-width="0.7"/>')
    # Rayures fines, surtout dans le sens de la hauteur, comme un panneau qu'on a sorti et rangé
    # Couleur et opacité sur chaque trait : certains moteurs SVG ignorent l'opacité si la couleur est héritée
    out.append('  <g fill="none">')
    for _ in range(int(20 * amount + 0.5)):
        x, y = rnd.uniform(1, width - 1), rnd.uniform(2, height - 2)
        length = rnd.uniform(1.5, 9)
        angle = math.radians(rnd.gauss(90, 25))
        x2, y2 = x + length * math.cos(angle), y + length * math.sin(angle)
        bend = rnd.uniform(-0.4, 0.4)
        mx, my = (x + x2) / 2 + bend, (y + y2) / 2
        out.append(f'    <path d="M{x:.2f},{y:.2f} Q{mx:.2f},{my:.2f} {x2:.2f},{y2:.2f}" stroke="{light}" stroke-linecap="round" stroke-width="{rnd.uniform(0.05, 0.1):.3f}" '
                   f'stroke-opacity="{a(rnd.uniform(0.03, 0.07))}"/>')
    out.append('  </g>')
    # Poussière : petits points clairs et sombres
    for _ in range(int(70 * amount + 0.5)):
        color = light if rnd.random() < 0.6 else "#000000"
        out.append(f'  <circle cx="{rnd.uniform(0, width):.2f}" cy="{rnd.uniform(0, height):.2f}" r="{rnd.uniform(0.04, 0.14):.3f}" '
                   f'fill="{color}" fill-opacity="{a(rnd.uniform(0.08, 0.22))}"/>')
    out.append('  <!-- patina:end -->')
    return "\n".join(out)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("svg")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--amount", type=float, default=1.0, help="0 = rien, 1 = discret, 2 = marqué")
    ap.add_argument("--light", default="#f2e6cc", help="couleur des rayures et de l'usure")
    ap.add_argument("--warm", default="#b08a4a", help="couleur des taches")
    ap.add_argument("--soft", type=float, default=1.0, help="dose des effets doux (vignettage, taches) : 0 = net")
    args = ap.parse_args()
    s = open(args.svg).read()
    width = float(re.search(r'width="([\d.]+)mm"', s).group(1))
    height = float(re.search(r'height="([\d.]+)mm"', s).group(1))
    s = re.sub(r"\n  <!-- patina:start.*?<!-- patina:end -->", "", s, flags=re.S)
    block = patina(width, height, args.seed, args.amount, args.light, args.warm, args.soft)
    s = s.replace("</svg>", block + "\n</svg>")
    open(args.svg, "w").write(s)
    print(f"patine ajoutée à {args.svg} (graine {args.seed}, dose {args.amount}, effets doux {args.soft})")
