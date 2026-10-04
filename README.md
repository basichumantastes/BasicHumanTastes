# Basic Human Tastes

Modules for [VCV Rack 2](https://vcvrack.com), for playing live and building generative patches.

| Module | Width | What it is |
|---|---|---|
| **Ernest** | 12 HP | Percussion oscillator with six ways to bend its pitch: kicks, toms, snares, zaps, metallic pings. Decay, bass boost with drive, ring modulation. CV over shape, depth, rate and decay. |
| **Marcel** | 12 HP | 8-bit looper / delay with a variable memory clock that slowly forgets: erode, tone, freeze, reverse, reset, and a ring that shows the memory. |
| **Jules** | 18 HP | Six linked slopes that become cascading envelopes, polyrhythmic LFOs, slew limiters, chords, plucked notes or FM voices. Twelve modes, a chance control on the trigger cascade, a poly output. |
| **Odette** | 18 HP | Stereo repeater with eight nested sizes, from plucked strings and flangers to 40-second loops. Tone, blur, spread, bounce, clock sync, reverse, freeze, drift and ducking. |
| **Colette** | 20 HP | A murmuration: up to 32 voices fly in pitch space, gather, scatter and settle on the notes of a chord. Pull, wind, cohesion and scatter shape the flock; each bird that settles sends a trigger and its note. |
| **Gaston** | 36 HP | Master sequencer for drum voices: eight trigger lanes with per-step probability and four CV lanes, each with its own length (up to 64 steps), clock ratio and direction for polyrhythms. Built-in clock with swing, CDJ-style transport, pads with live recording, and a map of all twelve lanes. |
| **Gastounet** | 20 HP | Gaston's pattern memory, attached to its left: 4 banks of 16 patterns, Korg-style write and copy, next and random, quantized launch (now, beat, bar, 2 or 4 bars, or when track 1 loops) with restart or legato, and a song made of pattern rows. |

Manuals (PDF): [all modules](docs/Basic-Human-Tastes-manuals.pdf) ·
[Ernest](docs/Ernest-manual.pdf) · [Marcel](docs/Marcel-manual.pdf) · [Jules](docs/Jules-manual.pdf) · [Odette](docs/Odette-manual.pdf)

## Building

With the [Rack 2 SDK](https://vcvrack.com/manual/Building#Building-Rack-plugins) next to this folder (`../Rack-SDK`), or pointed to by `RACK_DIR`:

    make install

## Tests and tools

    make -C tests run                    # offline test benches (Jules, Odette, Colette, Lucienne, Gaston, Gastounet, Ernest), linked to the SDK's libRack
    python3 tools/preview.py             # panel previews and label overlap check
    python3 tools/preview.py --clean     # clean panel renders for the manuals
    python3 tools/gaston_panel.py        # Gaston's panel, layout header and preview (--preview)
    python3 tools/gastounet_panel.py     # the same for Gastounet
    python3 docs/manuals.py              # rebuild the PDF manuals

## License

Code: GPL-3.0-or-later (`LICENSE`). Panel graphics, names and manuals: CC BY-NC-ND 4.0 (`LICENSE-graphics.md`).
