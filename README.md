# Basic Human Tastes

Modules for [VCV Rack 2](https://vcvrack.com), for playing live and building generative patches.

| Module | Width | What it is |
|---|---|---|
| **Ernest** | 12 HP | Percussion oscillator with six ways to bend its pitch: kicks, toms, snares, zaps, metallic pings. Decay, bass boost with drive, ring modulation. |
| **Marcel** | 12 HP | 8-bit looper / delay with a variable memory clock that slowly forgets: erode, tone, freeze, reverse, reset, and a ring that shows the memory. |
| **Jules** | 18 HP | Six linked slopes that become cascading envelopes, polyrhythmic LFOs, slew limiters, chords, plucked notes or FM voices. Twelve modes, a chance control on the trigger cascade, a poly output. |
| **Odette** | 18 HP | Stereo repeater with eight nested sizes, from plucked strings and flangers to 40-second loops. Tone, blur, spread, bounce, clock sync, reverse, freeze, drift and ducking. |
| **Colette** | 20 HP | A murmuration: up to 32 voices fly in pitch space, gather, scatter and settle on the notes of a chord. Pull, wind, cohesion and scatter shape the flock; each bird that settles sends a trigger and its note. |
| **Fernand** | 18 HP | A migration sequencer: a map of islands, each one a chord, and a flock that travels between them. Wind rises on the way, the chord changes half way, the flock settles on arrival. Made to steer Colette, but its outputs work with anything. |

Manuals (PDF): [all modules](docs/Basic-Human-Tastes-manuals.pdf) ·
[Ernest](docs/Ernest-manual.pdf) · [Marcel](docs/Marcel-manual.pdf) · [Jules](docs/Jules-manual.pdf) · [Odette](docs/Odette-manual.pdf)

## Building

With the [Rack 2 SDK](https://vcvrack.com/manual/Building#Building-Rack-plugins) next to this folder (`../Rack-SDK`), or pointed to by `RACK_DIR`:

    make install

## Tests and tools

    make -C tests run                    # offline test benches (Jules, Odette, Colette, Fernand), linked to the SDK's libRack
    python3 tools/preview.py             # panel previews and label overlap check
    python3 tools/preview.py --clean     # clean panel renders for the manuals
    python3 docs/manuals.py              # rebuild the PDF manuals

## License

Code: GPL-3.0-or-later (`LICENSE`). Panel graphics, names and manuals: CC BY-NC-ND 4.0 (`LICENSE-graphics.md`).
