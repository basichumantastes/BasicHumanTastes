#!/usr/bin/env python3
"""Manuels PDF des modules Basic Human Tastes (anglais, comme les panneaux).

Les images des panneaux viennent de `python3 tools/preview.py --clean` (docs/images/) ; celles de Lucienne, Gaston et
Gastounet, dont l'affichage est dessiné par le code, sont des captures de Rack (`Rack -u <dossier> -t 3`, dossier
utilisateur ne contenant que ce plugin).
Usage : python3 docs/manuals.py  ->  docs/<Module>-manual.pdf et docs/Basic-Human-Tastes-manuals.pdf
Le contenu suit le code des modules : à mettre à jour avec lui.
"""
import os
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                Image, PageBreak, KeepTogether, Flowable, CondPageBreak)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VERSION = "2.0.0"
DATE = "October 2026"

pdfmetrics.registerFont(TTFont("Michroma", os.path.join(ROOT, "res/fonts/Michroma-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Plex", os.path.join(HERE, "fonts/IBMPlexSans-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Plex-Bold", os.path.join(HERE, "fonts/IBMPlexSans-Bold.ttf")))
pdfmetrics.registerFont(TTFont("Plex-Italic", os.path.join(HERE, "fonts/IBMPlexSans-Italic.ttf")))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("Plex", normal="Plex", bold="Plex-Bold", italic="Plex-Italic", boldItalic="Plex-Bold")

INK = colors.HexColor("#1f1f22")
GREY = colors.HexColor("#6b6b70")
RULE = colors.HexColor("#d9d6cf")

BODY = ParagraphStyle("body", fontName="Plex", fontSize=9.5, leading=13.6, textColor=INK, spaceAfter=5)
SMALL = ParagraphStyle("small", parent=BODY, fontSize=8.2, leading=11.4, textColor=GREY)
CELL = ParagraphStyle("cell", parent=BODY, fontSize=8.8, leading=12.2, spaceAfter=0)
BULLET = ParagraphStyle("bullet", parent=BODY, leftIndent=10, bulletIndent=0, spaceAfter=2.5)


def heading(text, accent):
    return Paragraph(text.upper(), ParagraphStyle("h", fontName="Michroma", fontSize=10.5, leading=14,
                                                 textColor=accent, spaceBefore=12, spaceAfter=6))


def sub(text):
    return Paragraph(text, ParagraphStyle("sub", fontName="Plex-Bold", fontSize=9.8, leading=13, textColor=INK,
                                          spaceBefore=6, spaceAfter=3))


class Band(Flowable):
    """Bandeau de titre aux couleurs du module."""
    def __init__(self, name, tagline, bg, fg, accent, width):
        super().__init__()
        self.name, self.tagline, self.bg, self.fg, self.accent, self.width = name, tagline, bg, fg, accent, width
        self.height = 30 * mm

    def wrap(self, aw, ah):
        return self.width, self.height

    def draw(self):
        c = self.canv
        c.setFillColor(self.bg)
        c.roundRect(0, 0, self.width, self.height, 3 * mm, fill=1, stroke=0)
        c.setFillColor(self.fg)
        c.setFont("Michroma", 24)
        c.drawString(8 * mm, 15 * mm, self.name.upper())
        c.setFont("Plex", 10.5)
        c.drawString(8 * mm, 8 * mm, self.tagline)
        c.setFillColor(self.accent)
        c.setFont("Michroma", 7)
        c.drawRightString(self.width - 8 * mm, self.height - 8 * mm, "BASIC HUMAN TASTES")
        c.setFillColor(self.fg)
        c.setFont("Plex", 7.5)
        c.drawRightString(self.width - 8 * mm, 8 * mm, f"Manual · v{VERSION} · {DATE}")


def rows_table(rows, accent, name_width=34 * mm, width=174 * mm):
    data = [[Paragraph(n, ParagraphStyle("n", fontName="Michroma", fontSize=7.4, leading=10, textColor=accent)),
             Paragraph(d, CELL)] for n, d in rows]
    t = Table(data, colWidths=[name_width, width - name_width])
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 4.5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
        ("LEFTPADDING", (0, 0), (0, -1), 0),
    ]))
    return t


def grid_table(header, rows, accent, widths):
    head = [Paragraph(h, ParagraphStyle("th", fontName="Michroma", fontSize=7, leading=9.5, textColor=accent)) for h in header]
    data = [head] + [[Paragraph(str(c), CELL) for c in r] for r in rows]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.8, accent),
        ("LINEBELOW", (0, 1), (-1, -1), 0.4, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4.5),
        ("LEFTPADDING", (0, 0), (0, -1), 0),
    ]))
    return t


def module_story(m, frame_width):
    accent = colors.HexColor(m["accent"])
    story = [Band(m["name"], m["tagline"], colors.HexColor(m["bg"]), colors.HexColor(m["fg"]), colors.HexColor(m["band_accent"]), frame_width),
             Spacer(1, 7 * mm)]

    # Ouverture : panneau à gauche, présentation à droite
    img_path = os.path.join(HERE, "images", m["name"] + ".png")
    from PIL import Image as PILImage
    w, h = PILImage.open(img_path).size
    img_h = 132 * mm
    img_w = img_h * w / h
    right = [Paragraph(p, BODY) for p in m["intro"]]
    right.append(heading("At a glance", accent))
    right += [Paragraph(b, BULLET, bulletText="•") for b in m["glance"]]
    if img_w > frame_width * 0.62:
        # Panneau large (Gaston, Lucienne) : en pleine largeur, la présentation en dessous
        story += [Image(img_path, width=frame_width, height=frame_width * h / w), Spacer(1, 6 * mm)] + right
    else:
        text_w = frame_width - img_w - 8 * mm
        intro = Table([[Image(img_path, width=img_w, height=img_h), right]], colWidths=[img_w + 8 * mm, text_w])
        intro.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                                   ("RIGHTPADDING", (0, 0), (-1, -1), 0)]))
        story.append(intro)
    story.append(PageBreak())

    # Commandes, puis les tableaux (modes, tailles), puis entrées et sorties
    first, rest = m["sections"][0], m["sections"][1:]
    story += [heading(first[0], accent), rows_table(first[1], accent, width=frame_width)]
    for title, header, rows, widths in m.get("grids", []):
        story += [CondPageBreak(60 * mm), heading(title, accent), grid_table(header, rows, accent, [x * frame_width for x in widths])]
    for title, rows in rest:
        story += [CondPageBreak(30 * mm), heading(title, accent), rows_table(rows, accent, width=frame_width)]

    if m.get("how"):
        story.append(heading("How it works", accent))
        for t, paras in m["how"]:
            block = [sub(t)] + [Paragraph(p, BODY) for p in paras]
            story.append(KeepTogether(block))

    story.append(heading("Patch ideas", accent))
    for i, (t, p) in enumerate(m["patches"], 1):
        story.append(KeepTogether([sub(f"{i}. {t}"), Paragraph(p, BODY)]))

    story += [CondPageBreak(45 * mm), heading("Specifications", accent), rows_table(m["specs"], accent, width=frame_width),
]
    return story


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Plex", 7.5)
    canvas.setFillColor(GREY)
    canvas.drawString(18 * mm, 10 * mm, f"Basic Human Tastes · {doc.title_text}")
    canvas.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    canvas.restoreState()


def build(path, title, stories):
    doc = BaseDocTemplate(path, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm,
                          bottomMargin=18 * mm, title=title, author="Yoann Blanchard · Basic Human Tastes")
    doc.title_text = title
    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
    doc.addPageTemplates([PageTemplate(id="p", frames=[frame], onPage=footer)])
    doc.build(stories)
    print("->", os.path.relpath(path, ROOT))


# ---------------------------------------------------------------------------------------------
# Contenu
# ---------------------------------------------------------------------------------------------

ERNEST = dict(
    name="Ernest", tagline="Percussion oscillator with six ways to bend its pitch",
    bg="#26231f", fg="#f4ecd8", band_accent="#e0a930", accent="#a8761a",
    intro=[
        "Ernest is a single percussion voice built around an oscillator whose pitch can be bent six different ways. "
        "Hit it with a trigger and you get kicks, toms, snares, zaps and metallic pings. Leave the trigger unpatched "
        "and it plays continuously, which is handy for tuning.",
        "Beyond the classic drum-machine voice it has a few modular extras: a pitch input, CV over the modulation "
        "shape, depth and rate and over the decay, a modulation output and a free-running mode.",
    ],
    glance=[
        "Sine or triangle oscillator, 20 Hz to 12 kHz, with a 1 V/oct input",
        "Six pitch modulation shapes, from a falling saw to a pitch envelope",
        "Modulation from 0.1 Hz to 5 kHz: slow wobbles turn into new tone colours",
        "One-knob decay envelope, bass boost with drive, ring modulation",
        "12 HP",
    ],
    sections=[
        ("Controls", [
            ("Screen", "The modulation shape being played (knob plus TYPE input) and the base pitch."),
            ("PITCH", "Base pitch, from 20 Hz to 12 kHz. It starts at 55 Hz. With V/OCT patched, it transposes the "
                      "incoming note in semitones instead."),
            ("WAVE", "Lit: triangle, slightly brighter. Off: sine."),
            ("RETRIG", "Lit (default): every hit restarts the modulation, so every hit sounds the same. Off: the "
                       "modulation keeps running and each hit lands somewhere else in its cycle."),
            ("Shape selector", "The six icons around this knob: <b>saw down</b> (the pitch falls, again and again), "
                               "<b>square</b> (two pitches alternate), <b>triangle</b> (rises and falls), <b>random</b> "
                               "(a new random step each cycle), <b>noise</b> (bursts of noise in the pitch, for snares) "
                               "and <b>envelope</b> (one sweep down after each hit, for kicks and toms)."),
            ("DEPTH", "How far the pitch moves, from −100 % to +100 %. Centre: no modulation. Negative values flip "
                      "the direction. Gentle near the centre, up to ±8 octaves at the ends."),
            ("RATE", "Modulation speed, from 0.1 Hz to 5 kHz. With the envelope shape, how fast the sweep falls back. "
                     "Above about 20 Hz the modulation is heard as a new tone colour rather than movement."),
            ("DECAY", "Length of the volume envelope, from 3 ms to 3 s."),
            ("LEVEL", "Output level."),
            ("BASS", "Boosts the low end, then drives the sound into saturation as you turn it up."),
            ("RING", "Lit: the oscillator is multiplied by the signal at RING IN. Nothing comes out unless both are "
                     "sounding."),
        ]),
        ("Inputs", [
            ("TRIG", "Plays a hit: restarts the oscillator, both envelopes and, with RETRIG lit, the modulation. "
                     "The small light flashes on each hit. Unpatched, Ernest plays continuously."),
            ("V/OCT", "Patched, Ernest plays the note it receives, 1 volt per octave with 0 V = C4 as everywhere in "
                      "Rack, and the display shows the note. PITCH then transposes it in whole semitones from its "
                      "starting position, so the notes stay in tune."),
            ("TYPE", "Added to the shape selector, 1 V per shape: +1 V moves one icon clockwise, −1 V one icon back. "
                     "Feed it a sequencer CV to change the sound from hit to hit."),
            ("DEPTH", "Added to the DEPTH knob; ±5 V covers the whole range."),
            ("RATE", "Added to the RATE knob; 1 V moves it a tenth of its travel."),
            ("DECAY", "Added to the DECAY knob; 1 V moves it a tenth of its travel, roughly doubling or halving "
                      "the length."),
            ("RING IN", "The signal used by ring modulation (±5 V)."),
        ]),
        ("Outputs", [
            ("MOD", "The pitch modulation itself, after DEPTH: 5 V for 8 octaves. Use it to move other modules in "
                    "step with each hit."),
            ("OUT", "Audio, ±5 V at full LEVEL."),
        ]),
    ],
    how=[
        ("Which way the pitch moves", [
            "Saw down, square, triangle and envelope start from PITCH and move up from it (or down, with negative "
            "DEPTH), then come back. Random and noise move both ways around PITCH.",
            "The envelope jumps to full on every hit and falls back at a speed set by RATE. Noise is strongest at the "
            "start of each modulation cycle and fades towards its end, which gives snares their crack.",
        ]),
    ],
    patches=[
        ("Analog kick", "Ernest's starting settings: envelope shape, DEPTH 50 %, RATE at the centre, PITCH 55 Hz, "
                        "DECAY around two o'clock. Patch a clock into TRIG."),
        ("Snare", "Noise shape, WAVE lit, PITCH around 200 Hz, DEPTH 40 %, RATE far right, DECAY short, a little BASS."),
        ("Tuned toms", "Envelope shape, DEPTH 30 %, a short DECAY, and a pitch sequence into V/OCT."),
        ("Metallic ping", "Two Ernests triggered together. Patch the first OUT into the second RING IN and light RING "
                          "on the second. Raise RATE on either for inharmonic overtones."),
        ("Zap", "Envelope shape, DEPTH at 100 %, RATE low for a slow sweep, DECAY short."),
    ],
    specs=[
        ("Width", "12 HP"),
        ("Pitch", "20 Hz to 12 kHz, plus 1 V/oct input"),
        ("Modulation", "0.1 Hz to 5 kHz, up to ±8 octaves"),
        ("Decay", "3 ms to 3 s"),
        ("Levels", "Audio ±5 V. MOD output 5 V per 8 octaves. Trigger threshold about 1 V."),
    ],
)

MARCEL = dict(
    name="Marcel", tagline="8-bit looper that slowly forgets",
    bg="#1d2433", fg="#efe6d2", band_accent="#d9a441", accent="#a8781f",
    intro=[
        "Marcel records into a small 8-bit memory read by a clock you can bend. Slow the clock and the loop gets "
        "longer, lower and grittier; speed it up and it gets short, clean and high.",
        "And Marcel forgets. ERODE wears the loop down a little more on every pass and TONE darkens or thins it, "
        "until only a ghost of the sound is left. A ring at the top shows the memory as it fades.",
    ],
    glance=[
        "16,384 cells of 8-bit memory, clocked from 1 to 48 kHz: loops from 16 s down to 0.34 s",
        "Changing the speed bends the pitch of what is already recorded",
        "ERODE and TONE act on every pass, even on a frozen loop",
        "Reverse, reset, freeze, an end-of-loop trigger and a clock output",
        "12 HP",
    ],
    sections=[
        ("Controls", [
            ("Ring", "Each spoke is the loudness of one slice of the memory; the white dot is the play head. Gold while "
                     "recording, cream while frozen."),
            ("SPEED", "Memory clock, from 1 to 48 kHz. The loop lasts 16,384 clock ticks: 16 s fully left, 0.34 s fully "
                      "right (1.6 s at the start). Turning it while sound is in the memory changes its pitch."),
            ("INPUT", "Level of the incoming sound written into the memory."),
            ("FEEDBACK", "How much of the loop is written back on each pass, up to 110 %. Above 100 % the loop grows "
                         "until it saturates in 8 bits."),
            ("ERODE", "How much the memory forgets on every pass: a little volume, less resolution (from 8 bits down to "
                      "4), slices that fade to silence and soft crackles. Fully right, a frozen loop has nearly gone "
                      "after 4 passes; around 30 % it takes about a dozen."),
            ("PITCH", "Amount of the PITCH input. Fully right, 1 V per octave on the clock; left of centre inverts it."),
            ("TONE", "A filter inside the loop. Centre: no change. Left: each pass a little darker. Right: each pass a "
                     "little thinner. Its cutoff follows SPEED."),
            ("FREEZE", "Lit: nothing new is recorded. ERODE and TONE keep working, so a frozen loop still fades unless "
                       "ERODE is at zero and TONE at the centre."),
        ]),
        ("Inputs", [
            ("IN", "Sound to record (±5 V)."),
            ("PITCH", "Bends the memory clock, scaled by the PITCH knob."),
            ("ERODE", "Added to the ERODE knob; 10 V = 100 %."),
            ("REVERSE", "Plays and records backwards while the gate is high. The light next to it shows it."),
            ("RESET", "A trigger sends the play head back to the start of the loop."),
            ("TRIG", "Each trigger turns FREEZE on or off."),
            ("INPUT OPEN / CLOSE", "Two level controls for the input. A rising CV (0 to 10 V) opens OPEN and closes "
                                   "CLOSE. Unpatched, they let everything through."),
            ("FEEDBACK OPEN / CLOSE", "The same for the feedback."),
        ]),
        ("Outputs", [
            ("END", "A 10 V trigger each time the play head completes the loop, frozen or not. The light flashes."),
            ("CLOCK", "Square wave at the memory clock divided by 32 (±5 V): a tone that follows SPEED."),
            ("OUT", "The memory, ±5 V. Each value is held until the next clock tick, with no smoothing: that is where "
                    "the grain comes from."),
        ]),
    ],
    how=[
        ("One cell per tick", [
            "On each clock tick Marcel reads one cell, then writes the input plus the feedback back into it and moves "
            "on. The loop therefore always lasts exactly 16,384 ticks, and recorded sound plays back faster or slower "
            "as the clock changes.",
        ]),
        ("How forgetting works", [
            "Whatever comes round again passes through TONE and ERODE before it is written back. With ERODE above zero "
            "it loses a little volume and resolution, so quiet parts disappear first; now and then a slice fades to "
            "silence or a small crackle appears. The ring makes it visible: watch it hollow out.",
        ]),
    ],
    patches=[
        ("A voice that crumbles", "Microphone into IN, FEEDBACK 90 %. Record a phrase, press FREEZE, then raise ERODE "
                                  "to around 50 % and let it fall apart."),
        ("Varispeed tape", "A slow triangle LFO into PITCH, PITCH knob around 30 %, FEEDBACK 70 %."),
        ("Back and forth", "A slow square LFO into REVERSE."),
        ("Record one pass, play the next", "END into TRIG: the loop freezes after one pass and opens again on the next."),
        ("Hear the loop length", "END into the TRIG of an Ernest: one hit per pass."),
    ],
    specs=[
        ("Width", "12 HP"),
        ("Memory", "16,384 × 8 bits"),
        ("Clock", "1 to 48 kHz, plus CV"),
        ("Loop length", "0.34 to 16 s"),
        ("Levels", "Audio ±5 V. END 10 V, 1 ms. Gates and triggers about 1 V."),
    ],
)

JULES = dict(
    name="Jules", tagline="Six linked slopes: envelopes, LFOs, oscillators, chords",
    bg="#22352c", fg="#f3ecdc", band_accent="#e8765c", accent="#c4553d",
    intro=[
        "Jules is six slope generators that share one set of controls. RATE sets how fast the first one goes; SPREAD "
        "sets how the other five relate to it: all the same, or 2, 3, 4, 5 and 6 times faster, or slower.",
        "Depending on the range and the mode, the six slopes become cascading envelopes, polyrhythmic LFOs, six slew "
        "limiters, a chord of oscillators, plucked notes or FM voices. CHANCE adds controlled randomness to the way "
        "triggers travel across the six.",
    ],
    glance=[
        "6 slopes sharing RATE, SPREAD, RISE and CURVE",
        "CV range (envelopes and LFOs, 0 to 8 V) or AUDIO range (oscillators, ±5 V)",
        "12 modes on one selector, each one explained on the screen",
        "Triggers cascade from right to left, with a CHANCE control",
        "Six outputs, a mix and a 6-channel poly output",
        "18 HP",
    ],
    sections=[
        ("Controls", [
            ("Screen", "Seven bars (the six slopes and the mix), the mode name, and what TWEAK does in that mode."),
            ("RATE", "Speed of slope 1. CV range: from about 2 minutes to 8 ms per cycle, 1 Hz at the centre. AUDIO "
                     "range: about 12 Hz to 6 kHz, middle C at the centre."),
            ("SPREAD", "Speed of slopes 2 to 6 compared with slope 1. Centre: all the same. Right: slope N goes up to N "
                       "times faster (fully right ×2, ×3 … ×6, a harmonic series). Left: up to N times slower (an "
                       "undertone series). Finer near the centre."),
            ("FM", "Amount of the FM input. Right of centre: modulates the speed of all six slopes equally. Left of "
                   "centre: modulates SPREAD, so slope 1 is untouched and the fundamental stays solid. Centre: off."),
            ("RISE", "Share of each cycle spent rising, without changing its length. Left: instant rise, long fall "
                     "(saw). Centre: triangle. Right: long rise, instant fall (ramp)."),
            ("CURVE", "Bends the slopes without changing their timing. Centre: straight lines. Right: exponential, then "
                      "sine. Left: logarithmic, then square; fully left, RISE becomes the pulse width."),
            ("CV / AUDIO", "Range switch. CV: slow, outputs 0 to 8 V. AUDIO: fast, outputs ±5 V."),
            ("MODE", "Six positions, each with a CV and an AUDIO version: see the mode table."),
            ("TWEAK", "A second control for modes 2, 4 and 6. The small light shows when it is active; the screen says "
                      "what it does."),
            ("CHANCE", "Probability that a trigger passed along from the right reaches each slope. 100 % (default): every "
                       "slope fires. 50 %: each one flips a coin on every trigger. Triggers patched directly always pass."),
        ]),
    ],
    grids=[
        ("Modes", ["#", "CV range", "AUDIO range"], [
            ["1", "<b>ONE-SHOT</b>. Each trigger draws one rise and fall. Triggers that arrive while a slope is moving are ignored.",
                  "<b>IMPULSE</b>. The same at audio rate: patch an audio-rate clock into TRIG for impulse-train oscillators."],
            ["2", "<b>RETRIGGER</b>. Like ONE-SHOT, but TWEAK sets from when a moving slope accepts a new trigger: "
                  "−5 V at any time, 0 V after its peak, +5 V only once finished. It then restarts from zero.",
                  "<b>SUBHARMONIC</b>. Slope 1 runs freely and fires the other five at the end of each of its cycles. "
                  "TWEAK sets when they accept it. With SPREAD left: subharmonics, split tones and sync chaos."],
            ["3", "<b>GATE</b>. Rises while the gate is high, holds at the top, falls when the gate drops.",
                  "<b>PULSE</b>. The same with an audio-rate clock: oscillators whose shape follows the clock's pulse width."],
            ["4", "<b>SUSTAIN</b>. Rises, falls to a sustain level set by TWEAK (−5 V: 0 %, +5 V: 100 %), holds while the "
                  "gate is high, then falls. With a steady gate, the six outputs follow TWEAK at six speeds: six slew limiters.",
                  "<b>PLUCK</b>. Six oscillators, each through its own lowpass gate. Triggers pluck notes, gates hold them. "
                  "TWEAK sets the decay: about 300 ms at the centre, shorter to the right, longer to the left."],
            ["5", "<b>LOOP</b>. Six free-running LFOs. Triggers reset them to the start of their cycle.",
                  "<b>OSC</b>. Six oscillators; triggers hard-sync them. SPREAD fully right gives a major chord, fully left "
                  "a minor chord, across a few octaves."],
            ["6", "<b>BURST</b>. Slopes rest until triggered, then run a burst of cycles: TWEAK −4 V: 1, 0 V: 6, +5 V: 36. "
                  "Below −4 V triggers are ignored.",
                  "<b>FM</b>. Each oscillator gets its own sine modulator. TWEAK sets its ratio (×0.5 at −5 V, ×1 at 0 V, ×2 at "
                  "+5 V), the FM knob the depth (right: equal, left: more on the higher slopes), the FM input scales it."],
        ], [0.05, 0.475, 0.475]),
    ],
    how=[
        ("The trigger cascade", [
            "Each TRIG input that has no cable receives the signal of the nearest patched input to its right. One cable "
            "into TRIG 6 therefore drives all six slopes; a second cable into TRIG 3 splits them into two groups (6, 5, 4 "
            "and 3, 2, 1).",
            "CHANCE acts on that cascade only: every time a trigger travels to a slope through the cascade, the slope "
            "decides at random whether to take it. This is the quickest way to make six voices play a different "
            "combination on every beat while staying in time.",
        ]),
        ("The MIX output", [
            "In the CV range, MIX outputs the highest of the six slopes, each divided by its number (slope 3 at a third of "
            "its level, and so on): a complex modulation that would otherwise need a whole patch. In the AUDIO range it is "
            "simply the six summed and gently limited.",
        ]),
    ],
    patches=[
        ("Self-playing chords", "AUDIO range, MODE on PLUCK, SPREAD fully right. A clock into TRIG 6, CHANCE around 60 %. "
                                "MIX into a delay and a reverb: each beat plucks a different handful of chord notes."),
        ("Polyrhythmic LFOs", "CV range, LOOP, SPREAD a little right of centre. Six LFOs drift in and out of phase; send "
                              "them to filters, pans and levels."),
        ("Cascading envelopes", "CV range, ONE-SHOT, SPREAD to the left. One trigger into TRIG 6 gives six envelopes of "
                                "growing length."),
        ("Six slews from one random line", "CV range, SUSTAIN, a steady +10 V into TRIG 6 and a stepped random voltage into "
                                           "the TWEAK input: six smoothed versions of the same line, from sharp to lazy. "
                                           "Quantize a few of them for harmonies that arrive at different times."),
        ("FM bass", "AUDIO range, FM mode. A sequence into V/OCT, an envelope into the FM input, use output 1."),
    ],
    specs=[
        ("Width", "18 HP"),
        ("CV range", "Slope 1 from about 0.008 Hz to 128 Hz (up to ×6 with SPREAD)"),
        ("AUDIO range", "Slope 1 from about 12 Hz to 6 kHz, 1 V/oct"),
        ("Levels", "Outputs 0 to 8 V (CV) or ±5 V (AUDIO). Trigger threshold about 1 V. POLY: 6 channels."),
    ],
)
JULES["sections"] += [
    ("Inputs", [
        ("V/OCT", "Added to RATE, 1 volt per octave: the speed doubles with each volt."),
        ("SPREAD · RISE · CURVE", "Added to their knobs; ±5 V covers the whole range."),
        ("FM", "Frequency modulation source. In the AUDIO range a DC blocker keeps the pitch centred. In FM mode it becomes "
               "a depth control."),
        ("TWEAK", "Added to the TWEAK knob, ±5 V."),
        ("CHANCE", "Added to the CHANCE knob; 10 V = 100 %."),
        ("TRIG 1 to 6", "Trigger or gate for each slope, threshold about 1 V. Unpatched inputs take the signal from the "
                        "right (see the trigger cascade)."),
    ]),
    ("Outputs", [
        ("1 to 6", "Each slope: 0 to 8 V in the CV range, ±5 V in the AUDIO range."),
        ("MIX", "CV range: the highest of the six slopes, each divided by its number. AUDIO range: the six summed and gently "
                "limited, about ±7.5 V."),
        ("POLY", "The six slopes in one 6-channel cable."),
    ]),
]

ODETTE = dict(
    name="Odette", tagline="Stereo repeater, from plucked strings to forty-second loops",
    bg="#161618", fg="#f2efe8", band_accent="#6cc1b9", accent="#2a7f7a",
    intro=[
        "Odette repeats sound. SIZE picks a range of repeat times, from a couple of milliseconds, where repeats turn into "
        "strings and flangers, to forty seconds, where they become loops. TIME moves smoothly inside that range and bends "
        "the pitch like tape.",
        "Everything is written into one long memory, so jumping from a small size to a large one brings back what was "
        "played earlier. TONE and BLUR shape the repeats a little more on every pass.",
    ],
    glance=[
        "8 nested sizes, from 1.3 ms to 42 s",
        "Smooth, pitch-bending TIME; clean jumps between sizes",
        "Clock sync with musical ratios, reverse, non-destructive freeze",
        "Stereo SPREAD and BOUNCE, tape-like DRIFT, and DUCK for live input",
        "18 HP",
    ],
    sections=[
        ("Controls", [
            ("Screen", "The eight sizes as a strip, the current one lit, and the current repeat time (or the clock ratio "
                       "when CLOCK is patched)."),
            ("SIZE", "Range of repeat times; see the size table. Changing size jumps straight to the new time without "
                     "bending the pitch."),
            ("TIME", "Time between repeats within the size; to the right, longer. Turning it while repeats are sounding "
                     "bends their pitch, like tape. With CLOCK patched it picks a ratio instead (×2 fully right, 1/1 at the "
                     "centre, /2 fully left) and changes jump cleanly."),
            ("FEEDBACK", "Number of repeats, from one to endless and slightly beyond; saturation keeps it in check. While "
                         "frozen, it chooses where in the memory the frozen loop starts."),
            ("TONE", "Character of the repeats, applied again on every pass. Left: dark and warm, like an oil-can or "
                     "bucket-brigade echo. Around three o'clock: neutral (default). Right: thin and crisp."),
            ("BLUR", "Smears the repeats across the stereo field. Past half way, the smear is fed back into the loop and "
                     "builds up over time."),
            ("SPREAD", "Left and right repeat at different times, up to an octave apart in each direction."),
            ("MIX", "Balance between the dry sound and the repeats. With MIX CV patched, the knob scales the CV."),
            ("DRIFT", "A slow random wobble of the repeat time, different on each side, like tape."),
            ("DUCK", "Lowers the repeats while sound is coming in and brings them back when it stops (fast attack, "
                     "250 ms release). Keeps a voice or an instrument clear."),
            ("SIZE · TIME · TONE", "The three small knobs: amount and direction of the matching CV inputs."),
            ("REVERSE", "Lit: repeats play backwards. In the smallest sizes, reversing tiny slices becomes a buzzy "
                        "distortion."),
            ("FREEZE", "Lit: nothing new comes in and the memory is left untouched. SIZE, TIME, FEEDBACK (as start point), "
                       "TONE and BLUR keep acting on what you hear, without changing what is stored."),
            ("BOUNCE", "Lit: the feedback crosses sides, so the repeats alternate left and right (ping-pong)."),
        ]),
    ],
    grids=[
        ("Sizes", ["Size", "Repeat time", "Typical use"], [
            ["1", "1.3 to 20 ms", "Plucked strings (with BEND), flanger"],
            ["2", "20 to 82 ms", "Chorus, slapback, doubling"],
            ["3", "82 to 327 ms", "Short echo"],
            ["4", "163 to 653 ms", "Echo (default)"],
            ["5", "0.33 to 1.3 s", "Long echo"],
            ["6", "0.65 to 2.6 s", "Phrases"],
            ["7", "1.3 to 5.2 s", "Phrases"],
            ["8", "2.6 to 42 s", "Loops"],
        ], [0.12, 0.3, 0.58]),
    ],
    how=[
        ("One memory for all sizes", [
            "Odette writes everything into one 64-second memory per side. The sizes are windows of different lengths onto "
            "that memory, so a short repeat played in size 2 is still there when you move to size 8, and repeats from "
            "smaller sizes leave traces in the larger ones.",
        ]),
        ("Clock sync", [
            "With a clock in CLOCK (0.25 to 50 Hz), Odette measures its period and places the 1/1 ratio in the size that "
            "suits it best. TIME then steps through thirteen ratios from /2 to ×2, and other sizes multiply or divide by "
            "powers of two. Changes jump in time without any pitch bend.",
        ]),
    ],
    patches=[
        ("Tape echo", "SIZE 4, FEEDBACK 60 %, TONE around eleven o'clock, DRIFT 30 %, BLUR 20 %."),
        ("Plucked string", "SIZE 1, TIME fully left, FEEDBACK 85 %, MIX at the centre. Send a short noise burst or an "
                           "Ernest hit into IN, and a pitch sequence into BEND."),
        ("Stereo chorus", "SIZE 2, SPREAD a little off centre, FEEDBACK 30 %, a slow triangle LFO, turned down, into BEND."),
        ("Freeze and explore", "Play into small sizes for a while, press FREEZE, move to size 8, then sweep FEEDBACK to "
                               "travel through what was recorded."),
        ("A voice on stage", "Microphone into IN, SIZE 5, FEEDBACK 50 %, DUCK 70 %: the echoes stay out of the way while "
                             "the voice speaks and bloom in the pauses."),
        ("Back and forth", "PULSE into REVERSE, sizes 3 to 5: the repeats turn round on every repeat."),
    ],
    specs=[
        ("Width", "18 HP"),
        ("Repeat times", "1.3 ms to 42 s in 8 sizes"),
        ("Memory", "64 s per side"),
        ("Clock", "0.25 to 50 Hz"),
        ("Levels", "Audio ±5 V. PULSE 10 V, 1 ms. REVERSE, FREEZE and CLOCK react above about 2.5 V."),
    ],
)
ODETTE["sections"] += [
    ("Inputs", [
        ("SIZE · TIME · TONE", "Scaled by their small knobs; 5 V covers the whole range."),
        ("FEEDBACK · BLUR", "Added to their knobs; 5 V covers the whole range."),
        ("MIX", "0 to 8 V, scaled by the MIX knob."),
        ("REVERSE · FREEZE", "Each gate turns the matching button on or off."),
        ("CLOCK", "Locks the repeats to a clock; TIME picks the ratio (see Clock sync)."),
        ("BEND", "Bends the repeat time. In size 1 it follows 1 V/oct, to play the repeats as a string. In other sizes it "
                 "adds about 2 ms per volt, for chorus and vibrato, inverted on the right when SPREAD is off centre."),
        ("IN L · IN R", "Audio inputs. IN R copies IN L when it has no cable."),
    ]),
    ("Outputs", [
        ("OUT L · OUT R", "Stereo output, ±5 V."),
        ("PULSE", "A trigger on every repeat, left or right. The light next to TIME flashes with it."),
    ]),
]

COLETTE = dict(
    name="Colette", tagline="A murmuration of voices that settles on a chord",
    bg="#2a2440", fg="#f1e7d8", band_accent="#f2b48f", accent="#b0613a",
    intro=[
        "Colette is a flock of up to 32 birds flying in pitch space. Each bird is a voice: its height is its pitch, its "
        "place from left to right is its position in the stereo field. The notes of a chord, repeated over several "
        "octaves, are perches the birds are drawn to.",
        "A settled bird sings a pure note with a light vibrato. A bird in flight is quieter and breathier, like the sound "
        "of wings. Four forces shape the flock: PULL draws the birds to the chord, WIND stirs them up, COHESION holds them "
        "together and SCATTER keeps them apart. The chord comes apart into a cloud and forms again. Each bird that settles "
        "sends a trigger and its note.",
    ],
    glance=[
        "2 to 32 voices, from C2 up to 1 to 5 octaves of sky",
        "9 harmonies, from fifths to the harmonic series, plus FREE",
        "Root changes are voice-led: each bird glides to the nearest note of the new chord",
        "GUST sends the whole flock up, FREEZE holds it still",
        "A trigger and a 1 V/oct note each time a bird settles, stereo out through a small space",
        "20 HP",
    ],
    sections=[
        ("Controls", [
            ("Sky", "The flock as it flies. Up is pitch, from C2 at the bottom to the top of RANGE; left to right is the "
                    "stereo position. The perches (the chord notes) are thin lines. Birds in flight are cream dots with a "
                    "short trail; settled birds are peach, with a halo. The harmony in use is written top right."),
            ("PULL", "How strongly the birds are drawn to the chord notes and how firmly they hold on once settled. "
                     "Default 55 %. Birds only settle when PULL is above 5 % and more than 1.2 times WIND. Fully right, "
                     "only WIND and GUST make a settled bird leave; turning it down makes settled birds take off."),
            ("WIND", "Turbulence. It pushes flying birds around and makes settled birds take off now and then: about "
                     "once every 30 s per bird at the default 30 %, every 3 s at 100 %. It also widens the vibrato of "
                     "settled birds and makes it irregular. Near a chord note, PULL wins over WIND as it is turned up."),
            ("COHESION", "How tightly the flock holds together: each bird is drawn to the middle of the flock, in pitch "
                         "and in stereo, and matches the flock's movement. Default 45 %. A settled bird more than an "
                         "octave from the middle of the flock may leave to rejoin it, unless PULL is fully right."),
            ("SCATTER", "How much the birds keep their distance. Flying birds push away from other flying birds less "
                        "than about a semitone away, so they avoid beating unisons. Settled birds do not push, so several "
                        "can land on the same note. A settled bird with a neighbour within about a semitone may leave, "
                        "unless PULL is fully right. Default 40 %."),
            ("HARMONY", "Which notes the birds can settle on: see the harmony table. Default MAJOR 9."),
            ("ROOT", "Key of the chord, from −12 to +12 semitones, in steps of one semitone. Default 0 (C). Only the "
                     "note name counts: +12 and −12 give the same chord as 0. Changing it moves the perches, and each "
                     "settled bird glides to the nearest note of the new chord."),
            ("RANGE", "Height of the sky, from 1 to 5 octaves above C2 (C3 to C7). Default 3 (C2 to C5). Birds bounce "
                      "gently off the bottom and the top."),
            ("BIRDS", "Number of voices, from 2 to 32. Default 12. New birds start flying from a random height and fade "
                      "in; removed birds fade out. The overall level stays about the same whatever the number."),
            ("TIMBRE", "Fully left, a pure sine. Turning right adds the 2nd and 3rd harmonics, from flute to reed. It "
                       "also adds more breath to birds in flight. Default 30 %."),
            ("BLOOM", "How slowly voices swell and fade, as birds settle, take off, arrive or leave: from 20 ms fully "
                      "left to about 3 s fully right, 0.25 s at the default 50 %."),
            ("SPACE", "A small stereo space after the voices. Fully left, dry. Turning right raises the mix (up to 65 % "
                      "wet), the size and the tail. Default 50 %."),
            ("GUST", "Every settled bird takes off and stays in the air for 1 to 2.5 s; flying birds get a kick. The "
                     "button lights briefly."),
            ("FREEZE", "Lit: the flock holds still. Every bird keeps its pitch and position and goes on sounding. A gust "
                       "received while frozen is released when FREEZE is let go."),
        ]),
        ("Inputs", [
            ("V/OCT", "Added to ROOT, 1 volt per octave (0 V = C). Only the note name counts, so octave jumps change "
                      "nothing. Feed it a chord sequence: the birds glide to the nearest notes of each new chord."),
            ("HARMONY", "Selects the harmony, 1 V per step: 0 V FIFTHS, 4 V MAJOR 9, 9 V FREE. Patched, it replaces the "
                        "knob."),
            ("PULL · WIND · COHESION · SCATTER", "Added to their knobs; 10 V covers the whole range."),
            ("GUST", "A trigger sends the flock up, like the button. Threshold about 1 V."),
            ("FREEZE", "Holds the flock still while the gate is high (above 1 V). The FREEZE light shows it too."),
        ]),
        ("Outputs", [
            ("LAND", "A 10 V, 1 ms trigger each time a bird settles. The small light next to it flashes."),
            ("NOTE", "The pitch of the last bird that settled, 1 V/oct with 0 V = C4: C2 is −2 V, the top of a 5-octave "
                     "sky +3 V. It holds until the next bird settles."),
            ("OUT L · OUT R", "Stereo output, soft-limited to ±5 V."),
        ]),
    ],
    grids=[
        ("Harmonies", ["CV", "Harmony", "Perches (with ROOT on C)"], [
            ["0 V", "FIFTHS", "C G"],
            ["1 V", "MAJOR", "C E G"],
            ["2 V", "MINOR", "C Eb G"],
            ["3 V", "SUS", "C D G"],
            ["4 V", "MAJOR 9", "C D E G B (default)"],
            ["5 V", "MINOR 9", "C D Eb G Bb"],
            ["6 V", "PENTATONIC", "C D E G A"],
            ["7 V", "WHOLE TONE", "C D E F# G# A#"],
            ["8 V", "HARMONICS", "The harmonic series of the root, starting on the root in the lowest octave: root, "
                                 "octave, fifth, two octaves, third and so on, up to the top of the sky. Partials such "
                                 "as the 7th are out of tune with the piano, as they should be."],
            ["9 V", "FREE", "No perches. The birds never settle and LAND stays silent."],
        ], [0.1, 0.22, 0.68]),
    ],
    how=[
        ("Settling", [
            "A bird settles when it is within a quarter of a semitone of a chord note, almost still, PULL is above 5 % "
            "and more than 1.2 times WIND, and its flight time is over: 0.3 to 0.8 s after an ordinary take-off, 1 to "
            "2.5 s after a gust. At the default PULL of 55 %, nothing settles once WIND goes past about 45 %.",
            "Settled birds leave at random, more often as WIND rises. Below full PULL they also leave sooner when PULL is "
            "low, when they sit far from the flock (COHESION) or on a crowded note (SCATTER).",
        ]),
        ("Chord changes", [
            "The birds keep their real pitch when ROOT, V/OCT or HARMONY change; the perches move under them. A settled bird whose "
            "note is no longer in the chord glides to the nearest new note without flapping: no breath, full level. "
            "Common notes stay where they are and the others move by a tone or a semitone, like good voice leading. "
            "When the glide ends, the bird settles again and sends LAND, if PULL and WIND allow it.",
        ]),
        ("The sound of a bird", [
            "A settled or gliding bird sings at full level. A settled bird adds its own vibrato, 3.5 to 6.5 Hz, a few "
            "cents deep, wider with WIND. A bird in flight is quieter the faster it moves (down to 30 %) and mixes in "
            "breath: noise filtered around its pitch, stronger with speed and with TIMBRE.",
        ]),
    ],
    patches=[
        ("A chord that breathes", "The starting settings: MAJOR 9, 12 birds. Raise BLOOM to 70 % and SPACE to 60 %. "
                                  "The flock gathers on the chord, a bird leaves now and then and comes back."),
        ("Voice-led progression", "A stepped sequence into V/OCT (C, F, A, G for example), PULL 80 %, WIND 10 %. On "
                                  "each step the birds slide to the nearest notes of the new chord."),
        ("Melody from the flock", "LAND into the TRIG of an Ernest and NOTE into its V/OCT: every bird that settles "
                                  "plays its note on the drum."),
        ("Waves", "WIND knob at 50 % and a slow ±5 V triangle LFO into the WIND input. The flock lifts off as the wind "
                  "rises and settles again as it drops."),
        ("Lift every two bars", "One pulse every two bars into GUST, PULL 80 %, WIND 10 %: the chord dissolves into a "
                                "cloud and forms again. BLOOM fully right softens each landing."),
        ("Changing harmony", "A random voltage from 0 to 7 V, stepped every few bars, into HARMONY."),
    ],
    specs=[
        ("Width", "20 HP"),
        ("Voices", "2 to 32"),
        ("Pitch", "C2 (65.4 Hz) up to C3 to C7, set by RANGE"),
        ("Harmonies", "10 positions, selectable by CV at 1 V per step"),
        ("Levels", "Audio soft-limited to ±5 V. LAND 10 V, 1 ms. NOTE 1 V/oct, C2 = −2 V. GUST and FREEZE react above "
                   "about 1 V."),
    ],
)

GASTON = dict(
    name="Gaston", tagline="Master sequencer: eight trigger tracks, four CV tracks, one clock",
    bg="#121c32", fg="#ece3cf", band_accent="#c8a25c", accent="#8c6a28",
    intro=[
        "Gaston is the master sequencer of the patch, designed to drive Ernest drum voices. It has eight trigger "
        "tracks with a probability on every step, four CV tracks for pitches and modulation, a built-in clock with "
        "swing, and a transport that works like a DJ's CDJ player: CUE and PLAY/PAUSE.",
        "Every track has its own length, clock ratio and direction, so polyrhythms and polymeters come from a few "
        "simple settings. You edit one track at a time on a ribbon of 16 steps. Below it, the map shows all twelve "
        "tracks as they play, and a square flashes white only when a trigger actually leaves.",
    ],
    glance=[
        "8 trigger tracks with per-step probability, 4 CV tracks with per-step voltage",
        "Each track: its own LENGTH (1 to 64 steps), RATIO (12, from ÷4 to x4) and direction",
        "Built-in clock, 30 to 300 BPM, swing, optional vintage 96 PPQN timing, or an external clock",
        "CDJ-style CUE and PLAY/PAUSE, eight pads with live recording, mutes",
        "A map of all twelve tracks with progress rails",
        "Pattern memories and song mode with the Gastounet expander",
        "36 HP",
    ],
    sections=[
        ("Controls", [
            ("Clock screen", "The tempo, to a tenth of a BPM. With CLOCK IN patched it shows the measured tempo, and "
                             "EXT instead of BPM. The swing value sits above the SWING knob; with vintage timing on, it "
                             "shows the rounded value under a small 96."),
            ("TEMPO", "Internal clock, from 30 to 300 BPM in quarter notes (default 120). Gaston counts in sixteenths: "
                      "four master steps per beat. Ignored while CLOCK IN is patched."),
            ("SWING", "From 50 % (straight, default) to 75 %. Delays the second sixteenth of each pair: at "
                      "66 % it lands two thirds of the way through the pair. Only the binary ratios swing (see the "
                      "ratio table), and CLOCK OUT swings with them."),
            ("REC", "Arms pad recording; the red light shows it. While the transport plays, each pad hit is also written "
                    "into its track. The pads turn red-orange and the screen reads TRIGS · REC."),
            ("CUE", "Stopped: hold it to play from the start, release it to go back to the start and stop. Playing: back "
                    "to the start and stop. Hold CUE and press PLAY/PAUSE to keep playing when you let go. Lit while held."),
            ("PLAY / PAUSE", "Starts or pauses. A pause keeps the position and the next press resumes from there. Lit while "
                             "playing. Gaston always loads paused, at the start."),
            ("Track screen", "The selected track (TRACK 1 to 8, or CV A to D) and its kind: TRIGS, or the CV range with "
                             "FREE or SEMITONES. Then its LENGTH, RATIO, DIR, the STEP being played, and the PAGE shown out "
                             "of the pages the track uses (2/3)."),
            ("LENGTH", "Number of steps the selected track plays, 1 to 64 (default 16). Steps past "
                       "it are kept, just not played."),
            ("RATIO", "Speed of the selected track against the master step: twelve positions from ÷4 to x4 (default x1). "
                      "See the ratio table."),
            ("DIR", "Each press moves the selected track to the next direction: FWD, REV, PING-PONG, RANDOM. The arrow on "
                    "the button (a question mark for RANDOM) and the readout show it. PING-PONG does not repeat the end steps; RANDOM "
                    "picks any step within LENGTH each time."),
            ("SLEW", "CV tracks only. Glide between steps, 0 to 100 %. At 0, instant jumps. Above, an exponential glide "
                     "whose time constant goes from 2 ms to 1 s (about 45 ms at 50 %)."),
            ("±5 V · 0–10 V", "CV tracks only. Output range of the selected track (default ±5 V). The bars keep their place "
                              "when you switch: their voltages are rescaled."),
            ("FREE · SEMI", "CV tracks only. FREE (default): any voltage. SEMI: each step rounds to the nearest semitone "
                            "(1/12 V) and the readouts show notes, 0 V = C4."),
            ("PAGE", "Four buttons for the four pages of 16 steps (1–16, 17–32, 33–48, 49–64). You turn pages by hand: "
                     "Gaston never does it for you. Pages past LENGTH are dimmed but can still be opened and edited. When "
                     "the selected track is playing on another page, a dot blinks on that page's button."),
            ("SELECT", "Twelve buttons: round 1 to 8 for the trigger tracks, square A to D for the CV tracks. Picks the "
                       "track shown on the screen and the ribbon; LENGTH, RATIO and SLEW jump to its values. On a CV track "
                       "the whole step block turns verdigris. A muted track's number is dimmed."),
            ("PADS", "One pad per trigger track. A hit always sends a trigger to the track's output, even when the track "
                     "is muted. With REC armed and the transport playing, it also writes the hit into the track."),
            ("MUTE", "One switch per trigger track, lit when muted. Immediate: the track keeps moving but its steps send "
                     "nothing."),
            ("Lights", "Above each SELECT button. Trigger tracks: a flash on every trigger that leaves, from the sequence "
                       "or a pad. CV tracks: a flash on every active step."),
            ("CV readouts", "Under the CV select buttons: a small meter and the voltage each CV output is sending right now, "
                            "after SLEW, or the note when the track is set to SEMI."),
        ]),
        ("Edit ribbon and map", [
            ("Step numbers", "Above the ribbon, the numbers of the 16 steps of the page shown (17 to 32 on page 2, and so "
                             "on), in groups of four. The number of the step being played lights up."),
            ("Trigger step", "Click the square to turn a step on or off. A step turned on starts at 100 %. The step being "
                             "played glows, and flashes when it fires. Steps past LENGTH are dimmed but stay editable."),
            ("Probability", "The thin gauge under each lit step: its chance of firing, from 1 to 100 %. Drag it up or down; "
                            "about 14 mm of mouse travel (twice the height of a step) covers the whole range. No number, no "
                            "snapping. Empty and dotted on a step that is off."),
            ("CV bar", "On a CV track each step is a bar, drawn from a centre line in ±5 V or from the bottom in 0–10 V. "
                       "Click in the bar area to set the voltage at that height, then drag to adjust it."),
            ("CV readout", "The strip at the bottom of each CV step shows its value: +2.3 in ±5 V, 7.5 in 0–10 V, or a note "
                           "such as Eb4 in SEMI. Clicking it does not change the value; dragging from it adjusts the value "
                           "without a jump. Dimmed on an inactive step."),
            ("Activation row", "The thin row under each CV step. Solid: the step sets its voltage (default). Dotted: the "
                               "step is inactive and the output keeps the voltage of the last active step (sample and "
                               "hold). Click to switch."),
            ("Ctrl-click", "Ctrl-click (Cmd-click on Mac) on any step, on any page, makes it the last one: LENGTH jumps to "
                           "that step."),
            ("MAP", "All twelve tracks, one row each (1 to 8, then A to D), on the page shown and in line with the ribbon. "
                    "Trigger rows: steps that are on are lit, paler as their probability drops. CV rows: active steps are "
                    "lit, brighter the further they are from the centre. Steps past LENGTH are not drawn. Click a row to "
                    "select its track."),
            ("Progress rail", "A thin rail behind each row runs from the start of the cycle to the step being played, with "
                              "a bright edge, and drops back when the track loops. In REV it runs from the right, in "
                              "PING-PONG it follows the direction, in RANDOM there is none."),
            ("White flash", "A map square turns white only when a trigger really leaves: the probability draw passed and "
                            "the track is not muted. Pad hits and CV tracks do not flash on the map."),
        ]),
        ("Right-click menus", [
            ("Ribbon menu", "Right-click on the ribbon for the menu of the selected track. Its title names the track."),
            ("Clear track", "Trigger track: all steps off, back to 100 %. CV track: all steps at the centre value and "
                            "active. LENGTH, RATIO and DIR are kept."),
            ("Randomize", "Over all 64 steps. Trigger track: about 30 % of the steps on, at 100 %. CV track: a random "
                          "voltage on every step, all active."),
            ("Shift one step", "Left or right: rotates the steps within LENGTH, the last one wrapping round to the first, "
                               "probabilities and voltages included."),
            ("Copy · paste track", "Steps and settings (LENGTH, RATIO, DIR and, for CV tracks, range, quantize and SLEW). "
                                   "Pastes only onto a track of the same kind, and works between all the Gastons of the "
                                   "patch."),
            ("Copy · paste page", "The 16 steps of the page shown, to paste onto another page or another track of the "
                                  "same kind."),
            ("Module menu", "Right-click on the panel. The Clock section holds the four settings below, saved with the "
                            "patch."),
            ("CLOCK IN resolution", "Pulses per master step expected at CLOCK IN: 1 per step (4 PPQN, default), 2 per "
                                    "step (8 PPQN), 24 PPQN or 48 PPQN."),
            ("CLOCK OUT resolution", "The same four choices for CLOCK OUT."),
            ("Vintage timing", "Vintage timing (96 PPQN), off by default. See How it works."),
            ("Realign all tracks", "Never (default), every 16, 32 or 64 master steps (1, 2 or 4 bars): all tracks go back "
                                   "to step 1 together, so polymeters meet again."),
        ]),
        ("Inputs", [
            ("CLOCK", "External clock, replacing TEMPO; the resolution is set in the menu. Gaston measures the pulses, "
                      "shows the tempo with EXT and moves smoothly between them without running past the next one, so "
                      "faster ratios fall between pulses. Pulses are ignored while paused; after a return to the start, "
                      "the next pulse plays step 1. SWING still applies."),
            ("PLAY", "A trigger toggles play and pause, like the PLAY/PAUSE button."),
            ("RESET", "A trigger sends every track back to step 1 without stopping or starting the transport. RESET OUT "
                      "fires."),
            ("PADS", "Polyphonic, up to 8 channels: channel N plays, and records, like pad N."),
        ]),
        ("Outputs", [
            ("CLOCK", "10 V pulses on the swung grid while playing, at the resolution chosen in the menu (one per sixteenth "
                      "by default). Each pulse lasts 5 ms, or half the interval at fast rates."),
            ("RUN", "10 V while the transport plays, CUE included."),
            ("RESET", "A 10 V, 2 ms pulse each time Gaston goes back to the start: CUE, RESET IN, or the end of a song "
                      "with Gastounet."),
            ("OUT 1 to 8", "A 10 V, 10 ms trigger for each step that fires, and for each pad hit."),
            ("POLY", "The eight trigger tracks in one 8-channel cable."),
            ("A to D", "The four CV tracks, ±5 V or 0–10 V, after SLEW."),
        ]),
    ],
    grids=[
        ("Ratios", ["RATIO", "One step lasts", "At 120 BPM", "Swing"], [
            ["÷4", "A quarter note (4 master steps)", "500 ms", "Binary, but every step falls on an unswung sixteenth"],
            ["÷3", "A dotted eighth (3 master steps)", "375 ms", "Straight"],
            ["÷2", "An eighth note (2 master steps)", "250 ms", "Binary, but every step falls on an unswung sixteenth"],
            ["x2/3", "A dotted sixteenth (2 steps for 3 master steps)", "188 ms", "Straight"],
            ["x3/4", "An eighth-note triplet (3 per beat)", "167 ms", "Straight"],
            ["x1", "A sixteenth, the master step (default)", "125 ms", "Swings"],
            ["x5/4", "A quintuplet sixteenth (5 per beat)", "100 ms", "Straight"],
            ["x4/3", "A dotted thirty-second (4 steps for 3 master steps)", "94 ms", "Straight"],
            ["x3/2", "A sixteenth-note triplet (6 per beat)", "83 ms", "Straight"],
            ["x2", "A thirty-second note", "63 ms", "Swings"],
            ["x3", "A thirty-second triplet (12 per beat)", "42 ms", "Straight"],
            ["x4", "A sixty-fourth note", "31 ms", "Swings"],
        ], [0.1, 0.42, 0.14, 0.34]),
    ],
    how=[
        ("One track at a time", [
            "Gaston has twelve tracks but one ribbon. SELECT chooses the track you edit; the ribbon shows 16 of its "
            "steps, on the page chosen with PAGE, and LENGTH, RATIO, DIR and SLEW act on it. The map keeps all twelve in "
            "view on the same page, so you can watch the others while editing one.",
        ]),
        ("Master steps, ratios and lengths", [
            "Everything is counted in master steps, the sixteenths of the clock. A track at x1 moves one step per master "
            "step, at x3/2 three steps for every two, at ÷4 one step per beat. All tracks start together on step 1 and "
            "keep counting from there, so a 5-step track against a 16-step one, or a x3/4 track against a x1 one, drifts "
            "and meets again on its own schedule. Realign all tracks, in the menu, brings them back to step 1 every 1, 2 "
            "or 4 bars.",
            "Probability is drawn each time a lit step comes round: a 50 % step plays about every other pass.",
        ]),
        ("Recording from the pads", [
            "With REC armed and the transport playing, a pad hit writes a step at 100 % on its track. A hit in the first "
            "half of a step goes on that step. A hit in the second half goes on the next step, which then stays silent on "
            "that pass, since you have just heard it. Recording only adds steps: nothing is erased. In RANDOM the hit "
            "always goes on the current step. There is no step-by-step recording.",
        ]),
        ("CV tracks", [
            "Each step holds a voltage and sends it when the track reaches it. An inactive step sends nothing new, so the "
            "previous voltage carries on: turn a few steps off to hold notes longer. CV tracks have their own LENGTH, "
            "RATIO and DIR, independent from the trigger tracks, which makes it easy to lay a 7-note melody over a "
            "16-step rhythm.",
        ]),
        ("Vintage timing", [
            "Turned on in the menu, it rounds every event, swing included, to a 96 PPQN grid (24 ticks per sixteenth) and "
            "adds a random delay of 0 to 0.8 ms to each trigger, clock pulse and CV change. It recreates the feel of the classic "
            "hardware samplers. Swing then moves in notches of about 2 %.",
        ]),
        ("Gastounet: memories and song", [
            "Attach the Gastounet expander to Gaston's left side to store the working pattern in 64 memories (4 banks of "
            "16) and chain them into a song. Gaston then shows PATTERN A03 at the top of its panel (SONG A03 in song "
            "mode), followed by EDITED once the pattern differs from its memory. Mutes, tempo and swing are not part of "
            "a pattern.",
            "The memories live inside Gaston and are saved with it, so they stay there if Gastounet is removed. "
            "Initializing Gaston clears them too. Gastounet has its own manual.",
        ]),
    ],
    patches=[
        ("A drum kit", "Four Ernests on OUT 1 to 4: kick, snare, hats, tom. Put track 3 at x2 for thirty-second hats, a "
                       "few of its steps between 40 and 70 %, and SWING around 56 %."),
        ("Polymeter that comes home", "Track 1 at 16 steps, track 2 at 5, track 3 at 7 with RATIO x3/4. Set Realign all "
                                      "tracks to every 64 steps: the parts drift for four bars, then start together again."),
        ("Bass line", "CV A in ±5 V and SEMI into an Ernest's V/OCT, track 1 into its TRIG. Give CV A a LENGTH of 7 "
                      "against 16 for track 1, turn a few steps inactive to hold notes, and add a little SLEW for glides."),
        ("Slow modulation", "CV B in 0–10 V and FREE, RATIO ÷4, SLEW around 70 %, into Marcel's ERODE input: the loop "
                            "forgets more on some beats than on others."),
        ("Finger drumming", "Rack's MIDI-Gate module, its first eight gates merged into one cable with Merge, into PADS "
                            "IN. Arm REC, press PLAY/PAUSE and play from a pad controller: each hit lands on the nearest step."),
        ("Audition, then launch", "While stopped, hold CUE to hear the pattern from the top and release it to go back. "
                                  "Press PLAY/PAUSE while holding CUE to launch it for real."),
    ],
    specs=[
        ("Width", "36 HP"),
        ("Tracks", "8 trigger tracks with per-step probability (1 to 100 %), 4 CV tracks; up to 64 steps each, in 4 "
                   "pages of 16"),
        ("Clock", "30 to 300 BPM, sixteenth-note master step; external clock at 4, 8, 24 or 48 PPQN"),
        ("Swing", "50 to 75 %. Vintage timing: 96 PPQN grid, 0 to 0.8 ms of random delay per event"),
        ("Ratios", "12 per track, from ÷4 to x4"),
        ("Levels", "Triggers 10 V, 10 ms. CV ±5 V or 0–10 V. CLOCK OUT 10 V, up to 5 ms. RUN 10 V gate. RESET OUT "
                   "10 V, 2 ms. Inputs react above about 1 V. POLY: 8 channels."),
    ],
)

GASTOUNET = dict(
    name="Gastounet", tagline="Pattern memories and song chaining for Gaston",
    bg="#141e35", fg="#ece3cf", band_accent="#d4ae66", accent="#8a6a2c",
    intro=[
        "Gastounet gives Gaston a memory. Attached to its left, it stores 64 patterns in four banks, switches "
        "between them in time with the music and chains them into a song.",
        "It is only a control surface: the patterns and the song live inside Gaston and are saved with it. Gaston "
        "always plays one working pattern; Gastounet writes it into a slot, loads another one in its place, or "
        "follows a list of song rows.",
    ],
    glance=[
        "64 patterns: 4 banks of 16, A01 to D16",
        "WRITE and COPY, armed then aimed at a slot",
        "Changes on the next step, beat, bar, 2 or 4 bars, or when track 1 loops",
        "RESTART or LEGATO on every change",
        "A song of up to 64 rows, 1 to 64 bars each",
        "Everything is stored in Gaston",
        "20 HP, to the left of Gaston only",
    ],
    sections=[
        ("Controls", [
            ("Screen", "Top line: PATTERN or SONG, the pattern loaded in Gaston (for example A03), EDITED in orange "
                       "when Gaston's tracks differ from that slot, and BAR · BEAT counted from the start. Below, a "
                       "status line: what is armed or waiting (WRITE: PICK A SLOT, NEXT: A05 IN 3 BEATS, ROW 2 · BAR "
                       "3/8...) or, when nothing is, the LAUNCH and RESTART / LEGATO settings. Then four rows of the "
                       "song, dimmed in PATTERN mode. Detached, it reads ATTACH TO THE LEFT OF A GASTON."),
            ("ROWS", "Three buttons that act on the song row selected on the screen. <b>+ ROW</b> inserts a 4-bar row "
                     "after it, with the same pattern (in an empty song, the loaded pattern). <b>DUP</b> copies it "
                     "just below. <b>− ROW</b> deletes it."),
            ("PATTERN", "Pattern mode (default): the slot buttons, NEXT, RANDOM and the PATTERN input change patterns. "
                        "Also turns SONG REC off."),
            ("SONG", "Song mode: Gaston follows the song rows. Pressed while playing, the song starts from row 1 on the "
                     "next bar; stopped, row 1 is loaded at once."),
            ("SONG REC", "Lights red-orange. Each slot button then adds a 4-bar row at the end of the song. Turning it "
                         "on also enters SONG mode; press it again to stop adding rows."),
            ("END: LOOP / STOP", "What happens after the last row. LOOP (default): back to row 1. STOP: Gaston "
                                 "stops and returns to the start."),
            ("A · B · C · D", "Choose the bank shown on the 16 slot buttons, also used by NEXT, RANDOM and the PATTERN "
                              "input. On its own, changing bank loads nothing."),
            ("01 to 16", "The slots of the shown bank. In PATTERN mode a press asks for that pattern, which waits for "
                         "the LAUNCH grid; stopped, it loads at once. With WRITE or COPY armed, a press picks the slot. "
                         "In SONG mode they only add rows, with SONG REC lit."),
            ("WRITE", "Lights red-orange. The next slot pressed receives Gaston's working pattern and becomes the "
                      "loaded slot. Press WRITE again to cancel."),
            ("COPY", "Lights ivory. Press the source slot, then the destination. Press COPY again to cancel."),
            ("NEXT", "Asks for the next filled slot of the shown bank, after the loaded one or after the one already "
                     "waiting, so repeated presses step forward. With no filled slot, simply the next slot."),
            ("RANDOM", "Asks for a filled slot of the shown bank, other than the loaded one, at random. Nothing happens "
                       "if there is none."),
            ("LAUNCH", "When a requested pattern takes over: see the launch table. Default 1 BAR."),
            ("RESTART · LEGATO", "RESTART (default): on a change, every track starts again from step 1. LEGATO: the "
                                 "tracks keep their position and cycle, only their content changes."),
            ("Right-click menu", "Clear slot (the loaded one), Clear bank (the shown one), Clear all patterns, Clear "
                                 "song. They act at once, without confirmation, and leave Gaston's tracks as they are."),
        ]),
    ],
    grids=[
        ("Launch", ["LAUNCH", "The new pattern starts", "Longest wait at 120 BPM"], [
            ["NOW", "On the next step", "125 ms"],
            ["1 BEAT", "On the next beat (every 4 steps)", "0.5 s"],
            ["1 BAR", "On the next bar (every 16 steps). Default.", "2 s"],
            ["2 BARS", "Every 32 steps", "4 s"],
            ["4 BARS", "Every 64 steps", "8 s"],
            ["TRACK 1", "When Gaston's track 1 comes back to the start of its cycle, at its own LENGTH and DIVISION",
             "2 s for 16 steps at ×1"],
        ], [0.16, 0.56, 0.28]),
        ("Slot buttons", ["Slot looks", "Meaning"], [
            ["Bright number, dot below", "Filled"],
            ["Dim number, no dot", "Empty"],
            ["Brass", "Loaded in Gaston"],
            ["Brass, blinking slowly", "Loaded, and Gaston's tracks have changed since (EDITED)"],
            ["Glowing, blinking fast", "Waiting for the launch grid"],
            ["Red-orange border on every slot", "WRITE is armed"],
            ["Ivory", "The COPY source"],
        ], [0.4, 0.6]),
    ],
    how=[
        ("The memories live in Gaston", [
            "Gastounet only sends its buttons and inputs to Gaston. The 64 slots, the song, the loaded slot, the shown "
            "bank, the mode and the LAUNCH, RESTART / LEGATO and END settings are stored in Gaston and saved with "
            "the patch. WRITE and COPY arming, SONG REC and a waiting change are not saved.",
            "Detach Gastounet and Gaston keeps playing its working pattern, ignores the song and keeps the memories. "
            "Attach it again, or attach any Gastounet, and they are all back. With Gastounet attached, Gaston shows "
            "the loaded slot at the top of its panel, for example PATTERN A03 · EDITED.",
        ]),
        ("Working pattern and slots", [
            "A pattern is all twelve tracks of Gaston: steps, probabilities, voltages and active steps, with LENGTH, "
            "DIVISION, direction, range, quantization and slew. Mutes, tempo, swing and Gaston's menu settings are not "
            "part of it.",
            "Gaston always plays its working pattern, and you edit it there. Loading another slot replaces it without "
            "warning: write it first to keep your changes. Asking again for the loaded slot brings back what was "
            "written. An empty slot loads an empty pattern. Writing never interrupts playback.",
        ]),
        ("WRITE and COPY", [
            "Both work the same way: arm, then aim. While armed you can change bank first, to write or copy "
            "into another bank. Copying a slot onto itself cancels; copying an empty slot empties the destination. "
            "COPY does not touch Gaston's working pattern.",
        ]),
        ("Launching", [
            "Patterns have no length of their own: LAUNCH decides when a change happens, on a grid counted from the "
            "start of the transport. Only one change waits at a time; asking for another slot replaces it. While "
            "Gaston is stopped, changes happen at once. RESTART and LEGATO apply to song rows too.",
        ]),
        ("Song mode", [
            "A song is a list of up to 64 rows, each a slot and a number of bars (1 to 64). A bar is always 16 steps, "
            "and rows change on bar lines. The playing row is shown bright, with an arrow and a bar that fills as it "
            "plays. Rows can be edited while the song plays.",
            "In SONG mode the slot buttons, NEXT, RANDOM and the PATTERN input do not change patterns; WRITE and COPY "
            "still work. CUE and Gaston's RESET input bring the song back to row 1. With END: STOP, Gaston stops after "
            "the last row, returns to the start and loads row 1 again.",
        ]),
        ("Building a song", [
            "Press SONG REC, then slots in order: each adds a 4-bar row. Then click a row on the screen to select it "
            "and drag vertically on its slot name (any of the 64, A01 to D16) or on its length. The mouse wheel "
            "changes the length of the selected row, Shift and the wheel its slot. The list shows four rows and "
            "follows the selection: clicking the last visible row scrolls down, the first scrolls up. Rows that point "
            "to an empty slot are dimmed.",
        ]),
    ],
    patches=[
        ("Live set", "Write a groove and its variations into A01 to A04. Keep LAUNCH on 1 BAR and press the next "
                     "slot a little before the bar line: it blinks until it takes over, exactly on the downbeat."),
        ("Fill every fourth bar", "Write a groove in A01 and a fill in A02. SONG REC, press 01 then 02, set the "
                                  "first row to 3 BARS and the second to 1 BAR. END: LOOP."),
        ("Endless shuffle", "Fill a bank, LAUNCH on 4 BARS, END into RANDOM, then press RANDOM once. Every change "
                            "sends a trigger that asks for the next one: a new pattern from the bank every 4 bars."),
        ("Changes off the bar line", "Write patterns whose track 1 has a LENGTH of 12, then choose TRACK 1 and "
                                     "LEGATO. Each requested pattern lands when track 1 completes its 12 steps, and "
                                     "the other tracks keep their place."),
        ("Mark the sections", "END into the TRIG of an Ernest with a long DECAY: one hit on each new pattern or "
                              "song row."),
    ],
    specs=[
        ("Width", "20 HP, attached to the left of Gaston"),
        ("Memories", "4 banks of 16 patterns (A01 to D16), stored in Gaston"),
        ("Song", "Up to 64 rows of 1 to 64 bars; 16 steps per bar"),
        ("Launch", "NOW, 1 BEAT, 1 BAR, 2 BARS, 4 BARS, TRACK 1"),
        ("Levels", "PATTERN 0 to 10 V, 0.625 V per slot. NEXT, RANDOM and RESET react above about 1 V. END 10 V, "
                   "10 ms."),
    ],
)
GASTOUNET["sections"] += [
    ("Inputs", [
        ("PATTERN", "0 to 10 V across the 16 slots of the shown bank, 0.625 V per slot. A slot is asked for only when "
                    "the voltage moves into a different one, then waits for LAUNCH like a button press. Changing bank "
                    "with a steady voltage asks for the same number in the new bank. Ignored in SONG mode."),
        ("NEXT", "A trigger does what the NEXT button does."),
        ("RANDOM", "A trigger does what the RANDOM button does."),
        ("RESET", "SONG mode only: back to row 1, on the next bar while playing, at once when stopped. It does not "
                  "move Gaston's transport."),
    ]),
    ("Outputs", [
        ("END", "A 10 V, 10 ms trigger each time a pattern is loaded into Gaston: a launched change (even of the same "
                "slot), a slot picked while stopped, each new song row, and the end of a song set to END: STOP. "
                "CUE does not fire it."),
    ]),
]

LUCIENNE = dict(
    name="Lucienne", tagline="Six cores in a ring, tuned by ear, on a sagging power rail",
    bg="#1b1e2e", fg="#ece2cf", band_accent="#d08c5c", accent="#a65d30",
    intro=[
        "Lucienne is six analog-style oscillator cores tied in a ring. Each core is an integrator and a comparator, the "
        "circuit behind most triangle-and-square oscillators. You tune the six by ear, one knob each, above a common "
        "fundamental. SERRAGE then couples them: first they snap onto just intervals, then they modulate each other, "
        "then the ring turns chaotic.",
        "COURANT starves their shared power supply: the pitch sags, the sound clips, voices cut out and come back. An "
        "internal random register opens and closes six low-pass gates; two resonant filters and a halo follow. Two slow, "
        "drifting LFOs move across a dial of twelve destinations.",
        "The panel is in French. The tables below quote each label as printed, with its English sense the first time.",
    ],
    glance=[
        "6 triangle/square cores, each tuned from −12 to +36 semitones above a common fundamental",
        "SERRAGE: free, locked to just intervals, cross FM, chaos",
        "COURANT starves a shared rail: pitch sags, the sound clips, voices drop out",
        "Vactrol-style low-pass gates opened by an 8-step random register, or struck by TRIG",
        "Two resonant filters (24 dB ladder, 12 dB state-variable), series or parallel, then a halo",
        "Two drifting LFOs, each crossfaded across 12 destinations, including each other",
        "56 HP",
    ],
    sections=[
        ("Controls", [
            ("Screen", "The knot, in the middle of the six voices. Each voice is a dot that swells and brightens with its "
                       "gate. The links of the ring thicken as SERRAGE tightens and tremble when the ring turns chaotic. "
                       "The circle at the centre is the rail: it shrinks as it starves and flickers with the chaos."),
            ("1 to 6", "Tuning of each voice, from −12 to +36 semitones above the fundamental set by HAUTEUR. The knobs "
                       "are continuous: tune by ear. Voice 1, at the top, is the reference the others lock onto. At the "
                       "start: 0, +7.02 (fifth), +12, +15.86, +19.02 and +26.04 semitones."),
            ("SERRAGE (grip)", "How tightly the ring is coupled, 20 % at the start. At 0 the voices run free. Up to 50 % "
                               "each voice is pulled more and more strongly onto the nearest just interval with voice 1, "
                               "and its phase locks. From 35 % each voice is also frequency-modulated by the one before "
                               "it in the ring, harder and harder, until the ring turns chaotic near 100 %."),
            ("ÉCART (stretch)", "Scales the whole chord. 0: every voice on the fundamental (unison). 50 % (default): the "
                                "chord as tuned. 100 %: every interval doubled. SERRAGE still pulls the result onto just "
                                "intervals."),
            ("COURANT (current)", "The voices' power supply, 100 % (clean) at the start. Turned down, the rail starts "
                                  "lower and sags more as the gates open: the pitch falls (about 3.5 semitones flat at 0 "
                                  "before any load, more under load), the voices clip harder, a 50 Hz hum and sudden "
                                  "drops creep in. It also drives the filters harder and damps their resonance. When the "
                                  "rail gets too low, voices cut out, voice 6 first and voice 1 last, and return when it "
                                  "recovers: the stack stutters."),
            ("SOUFFLE (breath)", "Shape of the gates, from plucked to pad, 80 % at the start. Rise time from about 6 ms "
                                 "to 18 s (3.6 s at the start); fall from about 80 ms to 20 s, slowing as the gate "
                                 "closes, like a vactrol. Below 15 % the gates do not hold: each opening is a pluck. "
                                 "From 55 % they stay fully open as long as the register keeps them open."),
            ("FRAPPE (strike)", "Strikes by hand: every voice the register keeps open restarts its attack. The light "
                                "flashes. Same as a trigger at TRIG."),
            ("HAUTEUR (pitch)", "The fundamental, from −1 to +3 octaves above C2: 32.7 Hz to 523 Hz. +0.5 (about 92 Hz) "
                                "at the start. V/OCT adds to it; the filters follow it at half rate."),
            ("TIMBRE", "Waveform of all six voices, 20 % at the start. 0: triangle. Around 45 %: square. Above: the "
                       "triangle is folded, harder and harder up to 100 %. It also opens the gates' own low-pass a little."),
            ("DÉRIVE (drift)", "Slow instability, 30 % at the start. Each voice wanders on its own, over a few seconds, "
                               "typically a quarter of a semitone at 100 %; the whole module drifts together more slowly, "
                               "about 12 cents at 100 %. 0: steady."),
            ("HALO", "A diffuse, slowly modulated wash, like a dense reverb, 50 % at the start. 0: dry. Turning it up raises "
                     "both its level and the length of its tail; at 100 % the dry sound is down to 55 %."),
            ("FRÉQ A", "Cutoff of filter A, a saturating 24 dB/octave ladder low-pass, from 30 Hz to 16.5 kHz; about "
                       "4.7 kHz at the start. The value shown is exact with HAUTEUR at +0.5 and nothing in V/OCT. The "
                       "right channel sits 3 % higher, for width."),
            ("RÉSO A", "Resonance of filter A, 15 % at the start. Near 100 % it self-oscillates (less so on a starved rail)."),
            ("ROUTAGE (routing)", "How the filters are wired. 0 (default): series, A then B. 50 %: parallel, A and B "
                                  "mixed. 100 %: parallel, with B as a band-pass. It crossfades in between."),
            ("FRÉQ B", "Cutoff of filter B, a 12 dB/octave state-variable filter, from 30 Hz to 16.5 kHz; about 8.8 kHz "
                       "at the start. Same tracking as FRÉQ A."),
            ("RÉSO B", "Resonance of filter B, 10 % at the start. It rings strongly at the top, kept round by a soft "
                       "saturation."),
            ("CADENCE", "Speed of the register, from 0.05 Hz (a step every 20 s) to 12.8 Hz; about 0.46 Hz at the start, "
                        "a step every 2.2 s. A cable in HORLOGE replaces it."),
            ("DENSITÉ (density)", "Share of the voices the register opens. 100 % (default): all six always open, a "
                                  "drone. 50 %: each voice open about half the steps. 0: all closed."),
            ("BOUCLE (loop)", "Chance that the register takes a new random value on each step, 30 % at the start. 0: the "
                              "same 8 values come round forever, an 8-step loop. 100 %: new values all the time."),
            ("MÉMOIRE (memory)", "Lets the register move the pitches, 0 at the start. From about 25 % to 50 %, more and "
                                 "more voices jump an octave up or down on each step, so the chord stays just. Above "
                                 "50 %, free offsets join in, up to ±3.5 semitones at 100 %. Each change glides in 6 ms."),
            ("VITESSE (rate)", "LFO speed, from one cycle every 20 minutes to 30 Hz. At the start, A about 0.03 Hz (a 30 s "
                               "cycle) and B about 0.09 Hz (11 s). Each LFO also wanders by a few percent on its own, so "
                               "the two never stay in step."),
            ("FORME (shape)", "LFO waveform, crossfading through sine (0), triangle (25 %), rising ramp (50 %), smoothed "
                              "random (75 %) and stepped random (100 %)."),
            ("Destination dial", "The large knob ringed by the twelve marks (see the table). On a mark, the LFO goes to "
                                 "that destination; between two marks it is shared between both at constant power. At "
                                 "the start, A sits on FLT A and B on HAUT. The tooltip names the destination, or the "
                                 "pair. The arc shows where the LFO lands and how wide; the dot pulses with the LFO and "
                                 "dims as PROFONDEUR nears zero."),
            ("LARGEUR (width)", "0 (default): the LFO reaches one mark, or the two either side of the pointer. Turned up, "
                                "it spills onto more and more neighbours, weaker with distance; at 100 % it covers nearly "
                                "the whole ring."),
            ("PROFONDEUR (depth)", "Amount, from −100 % to +100 %; negative values invert. Gentle near the centre: 50 % "
                                   "gives 37.5 % of the full swing. At the start, A 25 % and B 12 %."),
            ("ÉVENTAIL (fan)", "Phase offset from voice to voice on HAUT, ÉCART, DENS and TIMB: at 100 % the six voices "
                               "are spread evenly over a whole cycle. On FLT A and FLT B it offsets the right channel "
                               "from the left, up to half a cycle. Other destinations ignore it. At the start, A 30 % and "
                               "B 60 %."),
        ]),
    ],
    grids=[
        ("LFO destinations", ["Mark", "Destination", "Full swing", "ÉVENTAIL"], [
            ["HAUT", "Pitch of each voice", "±1.5 semitones", "per voice"],
            ["ÉCART", "ÉCART", "±20 %", "per voice"],
            ["SERR", "SERRAGE", "±35 %", "—"],
            ["COUR", "COURANT", "±45 %", "—"],
            ["SOUF", "SOUFFLE", "±40 %", "—"],
            ["DENS", "DENSITÉ", "±45 %", "per voice"],
            ["HALO", "HALO", "±35 %", "—"],
            ["FLT B", "FRÉQ B", "±30 % (about ±2.7 octaves)", "left / right"],
            ["FLT A", "FRÉQ A", "±30 % (about ±2.7 octaves)", "left / right"],
            ["TIMB", "TIMBRE", "±35 %", "per voice"],
            ["VIT·B · VIT·A", "Speed of the other LFO", "±3 octaves", "—"],
            ["DST·B · DST·A", "Pointer of the other LFO", "±3 marks", "—"],
        ], [0.17, 0.3, 0.3, 0.23]),
    ],
    how=[
        ("Reading the destination table", [
            "Marks are listed clockwise, as on the dial. Full swing is the change with the LFO at its peak, PROFONDEUR at "
            "±100 % and the pointer right on the mark; it adds to the knob and its CV input. On LFO A the last two marks "
            "read VIT·B and DST·B; on LFO B, VIT·A and DST·A.",
        ]),
        ("Tuning by ear", [
            "Voice 1 is the reference. Every other voice measures its interval to voice 1 (after ÉCART, MÉMOIRE and "
            "DÉRIVE), folds it into one octave and looks for the nearest just interval: unison and octave, the fifth "
            "(3:2), the fourth (4:3), thirds and sixths (5:4, 6:5, 5:3, 8:5), then 7:4, 9:8, 15:8, 9:5, 7:5 and 16:15.",
            "Simpler ratios catch from further away. With SERRAGE at 50 % or more, the octave reaches 0.8 semitone either "
            "side and 16:15 only 0.24. Inside half of that zone the voice snaps exactly; towards its edge it lets go "
            "gently. Outside, it stays where you tuned it, and you hear it beat against voice 1.",
            "The voices also pull each other's phase: each gets a small kick whenever a neighbour flips, so a locked "
            "voice stays locked instead of sliding. Above 35 %, each voice's speed follows the triangle of the one before "
            "it (6 drives 1, 1 drives 2, and so on): cross FM, then chaos.",
        ]),
        ("The power rail", [
            "The six voices share one supply. With COURANT at 100 % it sits at 10 V on the RAIL output and barely moves "
            "(at most about 25 cents of pitch with all six gates open). Turned down, it starts lower (5.5 V at 0) and "
            "sags much more as the gates open. Everything follows it: the pitch, the clipping inside each voice, the "
            "filters' drive and resonance.",
            "Each voice has its own cut-out threshold, from about 4.5 V for voice 6 down to 3.3 V for voice 1. When a "
            "voice cuts out the load falls, the rail recovers and the voice returns: a starved patch stutters on its "
            "own. Some gain is given back as the rail drops, so the dying stack stays audible.",
        ]),
        ("The register and the gates", [
            "The register holds 8 random values between 0 and 1. On each step they all move one place along; the oldest "
            "comes back as the newest unless BOUCLE replaces it. Voice N reads place N and is open while its value is "
            "below DENSITÉ. Since the values move along, a pattern travels round the voices, one voice per step.",
            "On each step every open voice is struck again: low SOUFFLE plucks on every step, high SOUFFLE holds a "
            "drone. The gates are low-pass gates: closing, they get darker as well as quieter. Each Lucienne, and each "
            "Initialize, starts from its own random state, so two copies never play quite alike.",
        ]),
        ("Two LFOs on one dial", [
            "Each LFO has a pointer on a ring of twelve marks, and each mark gets a share that falls with its distance "
            "from the pointer. Past the last mark the ring wraps round to the first (through the DEST input or the other "
            "LFO). The last two marks reach into the other LFO: A can speed up or slow down B, or swing B's pointer "
            "round the dial, and B can do the same to A. Two LFOs aimed at each other never settle.",
        ]),
    ],
    patches=[
        ("Just drone", "SERRAGE 12 %, SOUFFLE 90 %, HALO 60 %, DÉRIVE 35 %, TIMBRE 15 %, the rest at the start. Leave it "
                       "running: the chord breathes with LFO A on filter A while LFO B makes the voices waver."),
        ("Tune a fifth by ear", "SERRAGE at 0, voices 3 to 6 at 0. Turn voice 2 slowly around +7 semitones and listen "
                                "to the beating. Raise SERRAGE to 40 %: as soon as it is close, it snaps onto the fifth "
                                "and stays."),
        ("Brownout", "SERRAGE 25 %, SOUFFLE 85 %, TIMBRE 35 %, HALO 40 %. Bring COURANT from 100 % down to 0 over a "
                     "minute: the chord sags, hums, clips, then stutters as voices drop out. Watch RAIL on a scope."),
        ("Pinched knot", "SOUFFLE 8 %, CADENCE 62 % (about 1.5 Hz), DENSITÉ 50 %, BOUCLE 15 %, MÉMOIRE 40 %, SERRAGE "
                         "60 %, TIMBRE 55 %, HAUTEUR +1.2, HALO 35 %: a plucked, looping figure that jumps octaves."),
        ("Vowel", "ROUTAGE 50 %, FRÉQ A and FRÉQ B near the middle, RÉSO A 65 %, RÉSO B 70 %, TIMBRE 45 %. LFO A halfway "
                  "between FLT B and FLT A, PROFONDEUR 80 %, ÉVENTAIL 50 %: the two peaks cross slowly, differently "
                  "left and right."),
        ("Knotted LFOs", "Both dials on VIT, LARGEUR 35 %, PROFONDEUR 70 %, VITESSE A 55 % and B 50 %. Each LFO speeds "
                         "up and slows down the other, and spills onto TIMB and DST."),
        ("Struck", "A clock every 1.5 s into TRIG, SERRAGE 35 %, RÉSO A 50 %. SOUFFLE 10 % for plucks, 50 % for swells "
                   "that rise and fade."),
        ("A sequence on the side", "PAS into a quantizer and on to another oscillator, TOP into the TRIG of an Ernest: "
                                   "the register plays them in time with the gates."),
    ],
    specs=[
        ("Width", "56 HP"),
        ("Voices", "6 integrator-comparator cores, 4× oversampled, each −12 to +36 semitones"),
        ("Fundamental", "32.7 Hz to 523 Hz (C1 to C5), plus V/OCT"),
        ("Filters", "A: 24 dB ladder low-pass. B: 12 dB state-variable, low-pass or band-pass. Both 30 Hz to 16.5 kHz."),
        ("Register", "8 steps, 0.05 to 12.8 Hz or external clock"),
        ("LFOs", "2, one cycle every 20 minutes to 30 Hz (one an hour to 40 Hz with CV), 12 destinations"),
        ("Levels", "Audio and voice outputs ±5 V. PAS 0 to 5 V. TOP 10 V, 5 ms. RAIL 0 to 10 V. LFO OUT ±5 V. CV "
                   "inputs: 10 V = full knob travel. TRIG about 1 V, HORLOGE 1.5 V."),
    ],
)
LUCIENNE["sections"] += [
    ("Inputs", [
        ("V/OCT", "Added to HAUTEUR, 1 V per octave. 0 V leaves HAUTEUR where it is (it does not follow Rack's 0 V = C4). "
                  "The filters follow at half rate. Lucienne is not a precision oscillator: DÉRIVE and the rail bend the "
                  "pitch."),
        ("TRIG", "While patched, the gates no longer hold: each rising edge (about 1 V) makes every voice the register "
                 "keeps open rise and fall back, shaped by SOUFFLE. The register still picks which voices answer "
                 "(DENSITÉ) and still drives MÉMOIRE."),
        ("ÉCART · SERRAGE · COURANT · SOUFFLE · HALO · DENSITÉ", "Added to their knobs; 10 V covers the whole range."),
        ("FILTRE A · FILTRE B", "Added to FRÉQ A and FRÉQ B; 10 V covers the whole range, about 1.1 V per octave."),
        ("HORLOGE (clock)", "Replaces CADENCE: the register steps on each rising edge, above 1.5 V, ready again below "
                            "0.5 V."),
    ]),
    ("Outputs", [
        ("1 to 6", "Each voice on its own, after its gate and before the filters and the halo, ±5 V softly limited."),
        ("PAS (step)", "The newest register value, 0 to 5 V, changing on each step: a stepped random voltage that loops "
                       "when BOUCLE is low. It is the value voice 1 reads, so voice 1 is open while PAS is below "
                       "DENSITÉ × 5 V."),
        ("TOP (tick)", "A 10 V, 5 ms trigger on each register step, internal or from HORLOGE."),
        ("RAIL", "The power rail, 0 to 10 V: 10 V when clean, lower as COURANT is turned down and as the gates open."),
        ("L · R", "Stereo output, ±5 V. The voices are spread across the field, odd voices to the left, even voices to "
                  "the right."),
    ]),
    ("LFO jacks", [
        ("IN", "Replaces the LFO with any signal: ±5 V for the full swing, up to ±7.5 V. VITESSE, FORME and ÉVENTAIL "
               "then have no effect; the dial, LARGEUR and PROFONDEUR still apply."),
        ("VITESSE", "Added to the VITESSE knob, 1 V per octave."),
        ("DEST", "Turns the dial: 10 V moves the pointer from the first mark to the last. Beyond, it wraps round."),
        ("OUT", "The LFO itself, ±5 V, before PROFONDEUR and at voice 1's phase. A copy of IN when IN is patched."),
    ]),
]

MODULES = [ERNEST, MARCEL, JULES, ODETTE, COLETTE, LUCIENNE, GASTON, GASTOUNET]


NUMBERS = {4: "Four", 5: "Five", 6: "Six", 7: "Seven", 8: "Eight", 9: "Nine", 10: "Ten"}


def cover(frame_width):
    accent = colors.HexColor("#2a7f7a")
    title = ParagraphStyle("t", fontName="Michroma", fontSize=30, leading=36, textColor=INK)
    story = [Spacer(1, 40 * mm), Paragraph("BASIC HUMAN<br/>TASTES", title), Spacer(1, 6 * mm),
             Paragraph(f"{NUMBERS[len(MODULES)]} modules for VCV Rack · manuals", ParagraphStyle("s", parent=BODY, fontSize=13, leading=18, textColor=GREY)),
             Spacer(1, 18 * mm)]
    rows = [(m["name"].upper(), m["tagline"]) for m in MODULES]
    story.append(rows_table(rows, accent, name_width=36 * mm, width=frame_width))
    story += [Spacer(1, 14 * mm), Paragraph(
        "Modules for playing live and building generative patches: a percussion oscillator, a looper that forgets, "
        "six linked slopes, a stereo repeater, a murmuration of voices, a ring of six analog-style cores, and a "
        "sequencer with its pattern memory.", BODY),
        Spacer(1, 30 * mm), Paragraph(f"Version {VERSION} · {DATE} · Yoann Blanchard", SMALL), PageBreak()]
    return story


if __name__ == "__main__":
    width = A4[0] - 36 * mm
    for m in MODULES:
        build(os.path.join(HERE, f"{m['name']}-manual.pdf"), f"{m['name']} manual", module_story(m, width))
    full = cover(width)
    for i, m in enumerate(MODULES):
        full += module_story(m, width)
        if i < len(MODULES) - 1:
            full.append(PageBreak())
    build(os.path.join(HERE, "Basic-Human-Tastes-manuals.pdf"), "Module manuals", full)
