#!/usr/bin/env python3
"""Manuels PDF des modules Basic Human Tastes (anglais, comme les panneaux).

Les images des panneaux viennent de `python3 tools/preview.py --clean` (docs/images/).
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
    text_w = frame_width - img_w - 8 * mm
    right = [Paragraph(p, BODY) for p in m["intro"]]
    right.append(heading("At a glance", accent))
    right += [Paragraph(b, BULLET, bulletText="•") for b in m["glance"]]
    intro = Table([[Image(img_path, width=img_w, height=img_h), right]], colWidths=[img_w + 8 * mm, text_w])
    intro.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                               ("RIGHTPADDING", (0, 0), (-1, -1), 0)]))
    story += [intro, PageBreak()]

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
        "Beyond the classic drum-machine voice it has a few modular extras: a pitch input, CV over the modulation, "
        "a modulation output and a free-running mode.",
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
            ("Screen", "The modulation shape and the base pitch."),
            ("PITCH", "Base pitch, from 20 Hz to 12 kHz. It starts at 55 Hz."),
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
            ("V/OCT", "Added to PITCH, 1 volt per octave."),
            ("DEPTH", "Added to the DEPTH knob; ±5 V covers the whole range."),
            ("RATE", "Added to the RATE knob; 1 V moves it a tenth of its travel."),
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

MODULES = [ERNEST, MARCEL, JULES, ODETTE]


def cover(frame_width):
    accent = colors.HexColor("#2a7f7a")
    title = ParagraphStyle("t", fontName="Michroma", fontSize=30, leading=36, textColor=INK)
    story = [Spacer(1, 40 * mm), Paragraph("BASIC HUMAN<br/>TASTES", title), Spacer(1, 6 * mm),
             Paragraph("Four modules for VCV Rack · manuals", ParagraphStyle("s", parent=BODY, fontSize=13, leading=18, textColor=GREY)),
             Spacer(1, 18 * mm)]
    rows = [(m["name"].upper(), m["tagline"]) for m in MODULES]
    story.append(rows_table(rows, accent, name_width=36 * mm, width=frame_width))
    story += [Spacer(1, 14 * mm), Paragraph(
        "Four voices for playing live and building generative patches: a percussion oscillator, a looper that "
        "forgets, six linked slopes and a stereo repeater.", BODY),
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
