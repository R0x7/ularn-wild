# Part 13 — Wild Edition app features: radio/synth, trading cards, photo mode, weather and glitch effects

This part specifies everything the Android (Kotlin) side of the Wild Edition adds on top of the base app: the Dungeon Radio (a software synthesizer, a text song notation, ten songs, fifteen sound effects, the rules that pick the track, and the Radio dialog); the monster trading cards (the 69-card list, rarities, the album and its storage, the drop popup, the album dialog and the pixel-art card renderer); the Polaroid photo mode (capture, composition, saving, sharing, auto-photo); the weather and day/night rendering pass; the Glitch Realm rendering pass; and the two small decoders that turn exported pet and bounty fields into side-panel text.

It leans on these other parts:

* **Part 12** for the C side of every feature: when the game emits each event, how the weather clock and weather state advance, what the scroll of segfault does to gameplay, how pets and bounties work. Rules that live in C are only summarised here where the app's behaviour depends on them.
* **Part 09** for the shared-state block (the exported fields, the event ring buffer, the feature-flag word).
* **Part 10** for the settings dialog, start screen and statistics dialog that host the entries described here, and **Part 11** for the game view, HUD and side panel that host the drawing passes.
* **Parts 14 and 15** for the tile sheet. This part only names tile ids: monster tiles 0–63, Wild monster tiles 384–387, the pet heart 397, weather tiles 400–407, glitch tiles 408–415, and the sticker tiles 81, 104, 105, 113–116.

Conventions: "dp" is a density-independent pixel, "sp" a scaled text pixel. Colours are written as (red, green, blue) in 0–255 or as hexadecimal AARRGGBB. The C random helper rnd(n) is uniform over 1..n and rund(n) uniform over 0..n−1 (confirmed in the game's header: rnd(x) = rand() mod x + 1, rund(x) = rand() mod x).

---

## 1. Feature flags, settings and where things are wired in

### 1.1 The feature mask

Twelve Wild features each have one bit. The same numbering is used by the C side and the app.

| Bit | Value | Feature | Settings checkbox label |
|---|---|---|---|
| 0 | 0x001 | Radio | "Dungeon Radio: chiptune music and sound effects" |
| 1 | 0x002 | Cards | "Monster trading cards drop from kills" |
| 2 | 0x004 | Vending | "Vending machines, junk food, the rubber duck and the Roomba" |
| 3 | 0x008 | Hats | "Hats" |
| 4 | 0x010 | Coin | "LarnCoin at the bank (and the Tax Auditor)" |
| 5 | 0x020 | Glitch | "The Glitch Realm (scroll of segfault)" |
| 6 | 0x040 | Fishing | "Fishing at fountains" |
| 7 | 0x080 | Pets | "Mysterious eggs hatch into pets" |
| 8 | 0x100 | Perks | "Choose a perk on every level-up" |
| 9 | 0x200 | Weather | "Weather and day/night on the surface" |
| 10 | 0x400 | Photo | "Polaroid photo mode" |
| 11 | 0x800 | Bounty | "Town bounty board: WANTED posters with gold rewards" |

All twelve together are 0xFFF.

There are two copies of the mask and the app uses them differently. This matters:

* **The app setting** (preferences file `settings`, integer key `wildFlags`, default 0xFFF). It is handed to the game process in the environment variable `ULARN_WILD` when a game starts; a new character stores it, a saved character keeps its own.
* **The game's own mask**, exported in the shared state (low 16 bits of the Wild flags word; the high 16 bits are the perk bits).

Which copy each app feature consults:

| App feature | Consults | Consequence |
|---|---|---|
| Radio (music, effects, tape unlocks, menu entry) | App setting, read live | Turning the checkbox off silences the radio at the next 250 ms tick even mid-game. While it is off, tape events are ignored, so a tape picked up then is not remembered by the app. |
| Cards (album entries, popups, menu entries) | App setting, read live; and the game only emits card events when its own Cards bit is on | Both must be on for cards to be collected. |
| Photo mode (menu entry, button, auto-photo) | App setting, read live | |
| Weather rendering and HUD icon | The game's own mask | Follows the character, not the checkbox. |
| Glitch rendering | Neither: active whenever the exported "glitch turns left" value is above zero | |
| Pet and bounty panel sections | Neither: shown whenever the exported species field is above zero | |

**Forward-compatible default.** A second integer key `wildKnown` (default 0x7FF) records which bits existed when the mask was last saved. Reading the mask returns (stored mask OR (0xFFF AND NOT known)) AND 0xFFF, so a feature added after the mask was last written reads as switched on. Writing the mask stores (value AND 0xFFF) and sets `wildKnown` to 0xFFF. The default 0x7FF exists because masks saved before the Bounty bit existed covered only bits 0–10.

### 1.2 All settings belonging to this part

Preferences file `settings`:

| Key | Type | Default | Meaning | Label in the Settings dialog |
|---|---|---|---|---|
| `wildFlags` | int | 0xFFF | Feature mask (above) | one checkbox per feature, labels in the table above |
| `wildKnown` | int | 0x7FF | Bits known when the mask was saved | (none) |
| `cardToasts` | bool | true | Show the drop popup | "Card drop toasts: show each card as it drops" |
| `reduceGlitch` | bool | false | Reduced glitch effects | "Reduce glitch effects (no shaking or colour cycling)" |
| `reduceWeather` | bool | false | Reduced weather effects | "Reduce weather effects (no drifting rain or snow, no lightning flashes)" |
| `autoPhoto` | bool | true | Photograph the final scene | "Auto-photo on death / victory" |
| `radioMusic` | int 0–100 | 60 | Music volume percent (clamped on write) | Radio dialog slider "Music" |
| `radioSfx` | int 0–100 | 80 | Sound-effect volume percent (clamped on write) | Radio dialog slider "Sound effects" |

In the Settings dialog these sit under the bold header "Wild features (new games start with these; a saved game keeps its own)": first the twelve feature checkboxes in bit order, then the four extra checkboxes in the order Card drop toasts, Reduce glitch effects, Reduce weather effects, Auto-photo. Each checkbox writes its preference immediately when toggled. The two "reduce" options are pushed into the game view when the dialog's OK button is pressed (and at start-up), together with the other view settings.

Other storage owned by this part:

| Where | Key / path | Content |
|---|---|---|
| Preferences file `radio` | `unlocked` (int, default 0) | Bit i set = bonus track i unlocked |
| Preferences file `cards` | `album` (string) | The album as JSON (section 7.4) |
| App-private files directory | `photos/last.png` | The newest Polaroid (section 9.8) |
| Shared gallery | `Pictures/Ularn Wild/ularn-wild-<millis>.png` | Saved photos |

None of these is tied to a save slot: tapes, cards and the last photo are shared by every character and survive across runs.

### 1.3 Menu entries and buttons

* **Start screen**, between "Statistics & achievements" and "Edit controls", only while the Cards setting is on: a pill button "Card album (N/69)" where N is the number of different cards owned at the moment the start screen is built.
* **In-game menu** (the plain list dialog). After the base entries, in this order, each only if its app setting is on: "Card album", "Radio", "Take a photo".
* **On-screen control action** "Take a photo (Wild Edition photo mode)" (accessibility label "Take a photo") can be assigned to any button in the control editor. Pressing it takes a photo only while a game is running; with photo mode switched off it silently does nothing.

### 1.4 Events

The game appends events (type, a, b, c) to a ring buffer in shared memory (Part 09). The app polls the buffer every time the game reports a change, always delivering to listeners on the main thread. On the first poll (or whenever the sequence number has gone backwards) it only synchronises and delivers nothing; if it has fallen more than one ring's length behind, only the newest ring's worth is delivered. Lost events are acceptable because they drive only sound, popups and the like. Listeners are, in registration order: the card collector, the radio, the photo mode, and a small listener that makes the screen flash for lightning (weather event with c = 1).

Event types used in this part:

| Id | Event | a, b, c |
|---|---|---|
| 1 | Kill | monster id; cell x; cell y |
| 2 | Level up | new character level |
| 3 | New level | dungeon level index |
| 4 | Take | object id; argument |
| 5 | Player hit | damage |
| 6 | Gold | amount |
| 7 | Spell | spell id |
| 8 | Died | |
| 9 | Won | |
| 100 | Radio tape | bonus track index |
| 110 | Card drop | monster id; 1 if holo |
| 120 | Vending | 0 buy, 1 kick, 2 topple, 3 Roomba ate something |
| 130 | Hat | hat object id, or 0 when taken off |
| 140 | Coin | 0 price tick, 1 news, 2 rug pull, 3 auditor |
| 150 | Glitch | 1 start, 0 end |
| 160 | Fishing | 0 cast, 1 catch, 2 kraken, 3 treasure map |
| 170 | Pet | 0 hatch, 1 attack, 2 died, 3 follow, 4 grew |
| 180 | Perk | perk index |
| 190 | Weather | new state; hour; c = 1 for a lightning strike |

---

## 2. The synthesizer

Nothing is pre-recorded. Songs and effects are synthesized sample by sample while they play, in the style of the NES sound chip: two-level pulse waves, a stepped triangle, and shift-register noise.

### 2.1 Output and threading

* Format: 44,100 Hz, mono, signed 16-bit PCM, a streaming audio track with usage "game" and content type "music".
* Render block: 1,024 samples (about 23 ms). The device buffer is the larger of the platform's minimum buffer and 4,096 bytes (2,048 samples), kept small so an effect follows its event closely.
* A dedicated thread named "ularn-radio", at audio priority, loops: render one block of floats from the mixer, convert each to an integer by multiplying by 32,000 and truncating, and write the block (blocking). Starting when already running is a no-op. Stopping sets a flag, interrupts the thread and waits up to 500 ms; the thread then pauses, flushes, stops and releases the track. A thread left over from a quick stop-then-start notices it is no longer the current one and ends. Any audio error is logged and the thread gives up (the radio is then silent until something starts it again).
* The mixer's public operations (render, change song, start an effect, query the song, query silence) are mutually exclusive: the audio thread renders while the main thread changes songs and starts effects.

### 2.2 Oscillator

One oscillator has a waveform, a frequency in Hz and a phase in [0, 1). Each sample the phase advances by frequency ÷ sample rate.

| Waveform | Output for the new phase p |
|---|---|
| Pulse 12.5% | +1 while p < 0.125, else −1 |
| Pulse 25% | +1 while p < 0.25, else −1 |
| Pulse 50% | +1 while p < 0.5, else −1 |
| Triangle | t = 4p − 1 for p < 0.5, else 3 − 4p; output = floor(8t) ÷ 8, limited to [−1, 1]. That is a 16-level staircase, not a smooth ramp. |
| Noise | A 15-bit shift register, initial value 1, initial output +1. The phase accumulates without wrapping; each time it reaches 1 it is reduced by 1 and the register is clocked once (several times per sample if the frequency exceeds the sample rate): new bit = (bit 0 XOR bit 1); register = (register shifted right by one) with the new bit placed in bit 14; output = +1 if bit 0 of the new register is set, else −1. The output holds between clocks. So for noise, "frequency" means clock rate: higher is brighter and hissier. |

For pulse and triangle the phase wraps modulo 1 before the output is computed. "Resetting" an oscillator sets its phase to 0 and leaves the shift register alone.

### 2.3 Envelope

A linear attack / decay / sustain / release envelope with level in [0, 1] and five stages (off, attack, decay, sustain, release). Times are in seconds; N(t) below means max(1, t × sample rate).

* Note on: adopt the given parameters and enter attack. The level is **not** reset, so a new note starting while the previous one is still sounding rises from wherever the level is.
* Attack: level increases by 1 ÷ N(attack) per sample; on reaching 1 it is set to 1 and decay begins.
* Decay: level decreases by (1 − sustain) ÷ N(decay) per sample; on reaching the sustain level it is set to it and the stage becomes sustain, or off if the sustain level is 0.
* Sustain: level holds.
* Note off: if not already off, enter release. Release: level decreases by 1 ÷ N(release) per sample (the release time is the time from full level to zero, so a release from a lower level is proportionally shorter); at 0 the envelope is off.

Presets (attack s, decay s, sustain level, release s):

| Name | Attack | Decay | Sustain | Release | Use |
|---|---|---|---|---|---|
| Pluck | 0.003 | 0.12 | 0.45 | 0.06 | Bright plucked leads and arpeggios |
| Lead | 0.005 | 0.08 | 0.70 | 0.08 | The default for any voice that names none |
| Pad | 0.03 | 0.20 | 0.60 | 0.15 | Soft held harmony |
| Bass | 0.002 | 0.05 | 0.90 | 0.03 | Triangle bass |
| Kick | 0.001 | 0.12 | 0 | 0.02 | Drum hit |
| Snare | 0.001 | 0.10 | 0 | 0.02 | Drum hit |
| Hat | 0.001 | 0.03 | 0 | 0.01 | Drum hit |

### 2.4 Sequencer (one per playing song)

A step is a sixteenth note: step seconds = 60 ÷ bpm ÷ 4, and step samples = sample rate × step seconds (kept as a fraction, not rounded). Each voice of the song gets a channel with its own oscillator, envelope, step index, "next step at" sample position, "gate off at" position and a per-sample pitch multiplier (the sweep, normally 1). A sample counter starts at 0. For every output sample, each channel does, in this order:

1. If the channel is not finished and the counter has reached "next step at", trigger the next step (below).
2. If a gate-off position is pending and the counter has reached it, release the envelope and clear the pending position.
3. If the envelope is off, contribute 0 (the oscillator does not advance).
4. If the sweep is not 1, multiply the oscillator frequency by it, never letting it fall below 30 Hz.
5. Contribute oscillator sample × envelope level × the voice's volume.

The channel contributions are summed, multiplied by the gain the mixer passes in, and added into the buffer.

Triggering a step:

1. If the index is past the last step: in a non-looping song, mark the channel finished, release the envelope and stop; in a looping song, go back to the first step. Each voice loops its own pattern independently, so a one-bar drum loop runs under a four-bar melody.
2. Take the step. Remember the old "next step at" as the start, then add (step length × step samples) to "next step at". Reset the sweep to 1.
3. By kind of step:
   * **Rest**: release the envelope, clear any pending gate-off, done.
   * **Kick**: waveform triangle at 180 Hz with a per-sample sweep of 0.25 raised to the power 1 ÷ (0.08 × sample rate), that is, two octaves down in 80 ms (then held at the 30 Hz floor); kick envelope.
   * **Snare**: noise clocked at 7,000 Hz; snare envelope.
   * **Hi-hat**: noise clocked at 16,000 Hz; hat envelope.
   * **Pitched note**: the voice's own waveform; frequency = the note's frequency, times 8 if the voice's waveform is noise; the voice's envelope.
4. For everything but a rest, schedule the gate-off at start + 0.85 × (step length × step samples). The 15% gap is what makes repeated notes audible as separate notes.

A non-looping song is finished when every channel is finished and every envelope is off.

### 2.5 Effect voices

An effect is a list of one or more segments played back to back. A segment has a waveform, a start frequency f0, an end frequency f1, a duration in milliseconds, and a volume (default 1). When a segment begins, the waveform is set, the oscillator phase is reset, and the segment length in samples is max(1, ms × sample rate ÷ 1000, integer division). For sample t of a segment of length L, with x = t ÷ L:

* frequency = f0 × (f1 ÷ f0)^x (a glide that is linear in pitch);
* edge = max(1, 3 × sample rate ÷ 1000) samples, that is 3 ms; fade = min(1, t ÷ edge, (L − t) ÷ edge);
* output = oscillator × segment volume × (1 − 0.6x) × fade. So each segment falls to 40% of its level by its end and has 3 ms anti-click ramps at both ends.

After the last segment the voice is idle.

### 2.6 Mixer

* **Music bus**: at most two sequencers, the current one and one fading out. Asking for the song that is already current (same song object) does nothing. Otherwise the current sequencer becomes the fading one (any earlier fading one is dropped at once), a fresh sequencer is created for the new song (none for silence) starting at its first step, and the fade position is reset.
* **Cross-fade**: 0.3 seconds, linear, with one gain per render block. At the start of a block, if the fade position is below the fade length, f = position ÷ length, the incoming gain is f and the outgoing gain 1 − f, and the position advances by the block size; otherwise the incoming gain is 1 and the fading sequencer is discarded.
* **Effect bus**: 4 effect voices (the polyphony limit). A new effect takes the first idle voice; if all are busy it steals the one that started longest ago.
* **Final sample** = music sum × 0.22 × music volume + effect sum × 0.30 × effect volume, clamped to [−1, 1]. Music volume and effect volume are the settings percentages divided by 100. The constants give head-room: four music voices at full level, or four effects, stay under 1.
* **Silent** means: no current song or the current one has finished, and nothing is fading, and no effect voice is busy. The radio stops the audio thread when the mixer is silent.

Polyphony in total: each song has two to four monophonic voices; during a cross-fade two songs sound at once; plus up to four effects.

---

## 3. Song notation

A song has a name, a tempo in quarter-note beats per minute, a list of voices and a loop flag (default looping). A voice has a waveform, a pattern, a volume (default 1) and an envelope preset (default Lead). A song's length is that of its longest voice.

A pattern is written as text, tokens separated by whitespace. A step is a sixteenth note; a bar of 4/4 is 16 steps.

| Token | Meaning |
|---|---|
| a note name, e.g. `c4`, `f#3`, `bb2` | A note lasting the current default length |
| `name:N`, e.g. `c4:6` | The same lasting N steps (N ≥ 1); works for notes, rests and drums |
| `-` or `r` | A rest |
| `k`, `s`, `h` | Kick, snare, hi-hat (meant for the noise voice) |
| `lN`, e.g. `l4` | Set the default length to N steps (N ≥ 1); makes no sound |
| `[` … `]N` | Play the enclosed group N times (N ≥ 1); a bare `]` means twice; groups nest |
| `\|` | Bar line; ignored, for the reader only |

Rules:

* The default length starts at 1. A length change applies to everything written after it in the text, inside or outside groups; lengths are fixed when a step is read, so repeating a group repeats the lengths it was read with.
* `[` and `]N` are tokens even when glued to a neighbouring note with no space.
* Note names: a letter a–g (either case), then any number of `#` (each +1 semitone) or lower-case `b` (each −1), then an octave number 0–8. So `bb4` is B-flat 4 and `cb4` is one semitone below C4. A name with no octave, an octave outside 0–8 or any other letter is invalid.
* Pitch number (MIDI) = 12 × (octave + 1) + letter offset + accidentals, with offsets c 0, d 2, e 4, f 5, g 7, a 9, b 11. So C4 = 60 and A4 = 69.
* Frequency = 440 × 2^((pitch number − 69) ÷ 12) Hz: equal temperament, A4 = 440 Hz (C4 ≈ 261.626 Hz, E2 ≈ 82.407 Hz).
* Anything malformed is an error at construction time: an unknown note, a zero or non-numeric length, a zero repeat count, an unmatched bracket.

Constraints every song must satisfy (enforced by tests): at least ten songs exist; every song has non-zero length and lasts under 60 seconds; in a looping song the song length is a whole number of bars and a whole multiple of every voice's length (so the parts stay in step); the two stingers do not loop and last under 5 seconds.

---

## 4. The songs

Ten songs. In the tables, "len" is the voice's pattern length in bars (16 steps each), "vol" its volume. Note durations are named musically: sixteenth = 1 step, eighth = 2, quarter = 4, half = 8, whole = 16.

Lists used elsewhere: the **regular** tracks, in order, are Town, Shallows, Depths, Volcano, Shop. The **bonus** (cassette) tracks, by index, are 0 Elevator Music, 1 Surf's Up, 2 Boss Fight; the indices must match the C side's track numbers.

### 4.1 Town

132 bpm, C major, 4 bars (16 beats, about 7.3 s), looping. Bright, bouncy, welcoming overworld tune.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 25% | 0.8 | Pluck | 4 | Arpeggio melody in eighths |
| Harmony | Pulse 12.5% | 0.35 | Pad | 4 | Quarter-note two-note rocking figure |
| Bass | Triangle | 1.0 | Bass | 4 | Root–fifth eighths |
| Drums | Noise | 0.5 | (drum) | 1 | Eighth-note rock beat |

* Lead: bar 1 climbs and falls through a C major arpeggio in eighths starting on E5 (third, fifth, high root, fifth, third, fifth) and lands on a quarter-note high C6. Bar 2 is the same shape one step up, outlining D minor and landing on A5. Bar 3 is all eighths: half a bar of the C arpeggio then half a bar of an F arpeggio. Bar 4 is the cadence: a quarter-note G5, two eighths stepping up through the G chord (B5, D6), and a half-note C6.
* Harmony: quarter notes rocking between C5 and E5 (bar 1), C5 and F5 (bar 2), C5–E5–C5–F5 (bar 3), then B4, D5 and a half-note C5.
* Bass: eighth notes alternating root and fifth: C for bar 1, F for bar 2, half a bar each of C and F in bar 3, then half a bar of G followed by quarter-note C and G.
* Drums, one bar of eighths: kick, hat, snare, hat, kick, kick, snare, hat.

### 4.2 Shallows

110 bpm, A minor (harmonic minor, with a G-sharp), 4 bars (16 beats, about 8.7 s), looping. Cautious and mysterious; the first dungeon levels.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 50% | 0.55 | Lead | 4 | Slow quarter-note melody |
| Arpeggio | Pulse 12.5% | 0.25 | Pluck | 4 | Continuous sixteenth-note broken chords |
| Bass | Triangle | 1.0 | Bass | 4 | Root–fifth eighths |
| Drums | Noise | 0.35 | (drum) | 1 | Quarter notes, no kick |

* Harmony: A minor for two bars, F major for one, E major for one.
* Lead: bar 1 walks up the A minor triad in quarters and steps back (A4, C5, E5, D5); bar 2 a dotted-quarter C5, an eighth B4 and a half-note A4; bar 3 quarters over F (F4, A4, C5, B4); bar 4 a dotted-half G-sharp 4 (the leading tone, left hanging) and a quarter rest.
* Arpeggio: a four-note sixteenth figure (root, third, fifth, third) repeated: eight times on A minor, four times on F major, four times on E major.
* Bass: eighths alternating root and fifth under each chord (A2–E3, F2–C3, E2–B2).
* Drums, one bar of quarters: hat, hat, snare, hat.

### 4.3 Depths

80 bpm, D minor, 8 bars (32 beats, 24 s), looping. Slow, hollow and ominous; the long middle of the dungeon.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 50% | 0.5 | Pad | 8 | Half-note melody |
| Arpeggio | Pulse 12.5% | 0.25 | Pad | 8 | Quarter-note broken chords |
| Bass | Triangle | 1.0 | Bass | 8 | One whole note per bar |
| Drums | Noise | 0.45 | (drum) | 1 | Sparse heartbeat |

* Chords by bar (arpeggio voice, root–third–fifth–third in quarters in the octave below the lead): D minor, D minor, B-flat, B-flat, D minor, D minor, G minor, A major.
* Bass whole notes by bar: D, A, B-flat, G, D, F, G, A (so the second bar of each pair puts a different chord tone in the bass).
* Lead: mostly half notes, with two quarters at the start of bars 2, 6 and 7. Bars 1–4 drift downward from D5/F5 to a dotted-half G4; bars 5–7 restate the opening, climb to A5 and sink back to D5; bar 8 is a whole-note C-sharp 5, the leading tone over the A chord, unresolved until the loop restarts on D.
* Drums, one bar: a kick on beat 1, nothing on beat 3, a hi-hat on beat 4.

### 4.4 Volcano

168 bpm, centred on E with a flattened second and a tritone (E, F and B-flat dominate: a Phrygian/Locrian colour), 4 bars (16 beats, about 5.7 s), looping. Fast, tense, relentless.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 25% | 0.6 | Pluck | 4 | Sixteenth-note riff |
| Harmony | Pulse 12.5% | 0.3 | Lead | 4 | Quarter-note tritone leaps |
| Bass | Triangle | 1.0 | Bass | 4 | One-bar eighth ostinato |
| Drums | Noise | 0.5 | (drum) | 1 | Busy sixteenth-flavoured beat |

* Lead: a two-bar phrase played twice. Bar 1: a four-sixteenth cell (E5, F5, E5, down to B-flat 4) twice, then a second cell (E5, F5, G5, F5) twice. Bar 2: the first cell twice, then eighths B4 and C5 and a quarter B-flat 4.
* Harmony: every bar the same four quarters: E4, B-flat 4, F4, B4.
* Bass: every bar the same eight eighths: a pedal low E broken up by an octave jump on the third eighth, the F above on the fifth and the B-flat on the seventh.
* Drums, one bar (16 steps): kick (eighth), hat, hat (sixteenths), snare (eighth), hat, kick (sixteenths), kick (eighth), hat (eighth), snare (eighth), hat (eighth).

### 4.5 Shop

144 bpm, F major, 4 bars (16 beats, about 6.7 s), looping. A cheerful jingle. Three voices only.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 25% | 0.7 | Pluck | 4 | Bouncy eighth-note melody |
| Bass | Triangle | 1.0 | Bass | 4 | Walking quarters |
| Drums | Noise | 0.45 | (drum) | 1 | Boom-chick |

* Lead: bar 1 is F5, an eighth rest, A5, F5, a quarter-note C6, then A5 and F5 (the rest on the second eighth gives the hop). Bar 2 is the same figure one step up (G5, rest, B-flat 5, G5, D6, B-flat 5, G5). Bar 3 is straight eighths, a falling F arpeggio then a falling C-seventh shape. Bar 4 resolves: quarter F5, quarter C5, half-note F5.
* Bass, quarters: F–C–F–C; G–D–C–E; F–A–C–low C; F–C and a half-note F.
* Drums, one bar: kick (quarter), snare (quarter), two eighth kicks, snare (quarter).

### 4.6 Game Over (stinger)

100 bpm, not looping, 6 beats (24 steps, 3.6 s). Two voices in parallel: a sad chromatic slide.

| Voice | Wave | Vol | Envelope | Role |
|---|---|---|---|---|
| Lead | Pulse 50% | 0.6 | Lead | Three falling quarters then a long note |
| Bass | Triangle | 1.0 | Bass | The same line a fifth-plus-octave below |

Both voices play three quarter notes descending by semitones and then hold a fourth, a semitone lower again, for three beats: the lead G4, F-sharp 4, F4, E4; the bass C3, B2, B-flat 2, A2.

### 4.7 Victory (stinger)

150 bpm, not looping, 6 beats (24 steps, 2.4 s). A C major fanfare.

| Voice | Wave | Vol | Envelope | Role |
|---|---|---|---|---|
| Lead | Pulse 25% | 0.7 | Lead | Rising arpeggio, a breath, then the held top note |
| Harmony | Pulse 12.5% | 0.35 | Pad | The third (E5) held, struck twice, three beats each |
| Bass | Triangle | 1.0 | Bass | C3, G3, C3 in quarters, then C4 held three beats |
| Drums | Noise | 0.4 | (drum) | Eighths: kick, hat, snare, hat, kick, hat, then a final snare |

Lead: eighths C5, E5, G5, C6, an eighth rest, G5, then C6 held for three beats.

### 4.8 Elevator Music (bonus 0)

100 bpm, B-flat major, 4 bars (16 beats, 9.6 s), looping. Laid-back lounge / bossa pastiche.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 12.5% | 0.6 | Lead | 4 | Thin, lazy melody with rests |
| Comping | Pulse 50% | 0.2 | Pluck | 4 | Off-beat stabs |
| Bass | Triangle | 1.0 | Bass | 4 | Syncopated root–fifth |
| Drums | Noise | 0.3 | (drum) | 1 | Hats with a light backbeat |

* Chords: B-flat, F, E-flat, F (one bar each).
* Lead: bars 1 and 2 share a rhythm (eighth, eighth rest, two eighths, eighth rest, eighth, then a quarter): bar 1 circles D5–F5–D5, drops through C5 to a quarter B-flat 4; bar 2 is the same a step lower (C5–E-flat 5–C5, B-flat 4, quarter A4). Bar 3 runs up a B-flat major-seventh arpeggio in eighths (B-flat 4, D5, F5, A5) then two quarters down (G5, F5). Bar 4 descends: quarter E-flat 5, quarter D5, half-note C5.
* Comping: quarter rests on beats 1 and 3, single quiet notes on beats 2 and 4 (D4 and F4; E-flat 4 and A4; G4 twice; A4 twice).
* Bass, eighths with rests on the second and fifth eighth of the bar: root, rest, fifth, root, rest, fifth, root, fifth. Bar 4 breaks the pattern and ends on a half-note F.
* Drums, one bar: hat (eighth), two sixteenth hats, snare (eighth), hat (eighth), and the same again. The snare falls on beats 2 and 4; there is no kick.

### 4.9 Surf's Up (bonus 1)

176 bpm, E minor, 4 bars (16 beats, about 5.5 s), looping. Surf-rock: a tremolo-picked "guitar" line over a galloping bass. Three voices.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 25% | 0.6 | Pluck | 4 | Rapid repeated sixteenths |
| Bass | Triangle | 1.0 | Bass | 4 | Eighth-note root–root–fifth–octave |
| Drums | Noise | 0.5 | (drum) | 1 | The Town beat, faster |

* Lead: each of the first three bars is four sixteenths on one note then four on the note a step below, played twice: E5 and D-sharp 5 (bar 1), G5 and F-sharp 5 (bar 2), A5 and G5 (bar 3). Bar 4 is a half-note B5 followed by a half-bar of silence.
* Bass: a four-eighth cell (root, root, fifth, octave) four times on E, twice on A, twice on B.
* Drums, one bar of eighths: kick, hat, snare, hat, kick, kick, snare, hat.

### 4.10 Boss Fight (bonus 2)

150 bpm, C minor with a tritone and a chromatic run, 4 bars (16 beats, 6.4 s), looping. Urgent and dramatic. Three voices.

| Voice | Wave | Vol | Envelope | Len | Role |
|---|---|---|---|---|---|
| Lead | Pulse 50% | 0.6 | Lead | 4 | Stabbing eighth riff |
| Bass | Triangle | 1.0 | Bass | 4 | Sixteenth-note octave gallop |
| Drums | Noise | 0.5 | (drum) | 1 | Sixteenths with a snare fill |

* Lead: bar 1 is four eighths (C5, C5, E-flat 5, C5) answered by quarters F-sharp 5 and G5. Bar 2 repeats the four eighths, answered by quarters B4 and A-flat 4. Bar 3 is a chromatic descent in eighths from C6 down to F5. Bar 4 is a half-note G5 and a half-note G4.
* Bass: a four-sixteenth cell (root, root, octave above, root) eight times on C, four times on A-flat, four times on G.
* Drums, one bar of sixteenths: kick, hat, snare, hat; kick, kick, snare, hat; kick, hat, snare, hat; kick, snare, snare, snare.

---

## 5. Track selection and the radio's behaviour

### 5.1 The picker's inputs

Every 250 ms on the main thread, while the activity is resumed, the radio builds a "scene" from the latest state snapshot:

* **Level index**: 0 is the town, 1–15 the dungeon, 16–20 the volcano.
* **Text page**: the display is in full-screen text mode (stores, inventory, help and so on).
* **Bank text** and **shop text**: only evaluated on a text page. All rows of the text screen are joined into one string. Bank text is true if it contains "Bank of Ularn". Shop text is true if it contains any of "Thrift Shoppe", "College of Ularn", "Trading Post", "Revenue Service", "McDope". (These match the welcome banners of the DND store, the college, the trading post, the revenue office and the dealer's pad, and the bank's two banners. Any other text page that happens to contain one of the phrases, such as a store's refusal mentioning the Revenue Service, counts too.)
* **Boss visible**: only evaluated on the map. True if any cell of the exported map tile grid, over the whole level and not just the part on screen, holds a tile id from 56 to 63 (the demon lords, the demon prince and the God of Hellfire).
* **End state**: the exported end state if it is non-zero (1 died, 2 won, 3 quit; set when the final score is shown), otherwise the radio's own memory of a Died or Won event, which arrives earlier.

### 5.2 Sticky shop state

The picker keeps two flags, "in bank" and "in shop". When the scene is not a text page both are cleared. On a text page, bank text sets "in bank"; otherwise shop text sets "in shop"; otherwise the flags keep their value. A store spreads over several pages that do not all repeat its name, so once a store page has been seen the store's music continues until the map returns. An ordinary text page (inventory, help) opened from the map sets neither flag and keeps the level's music.

### 5.3 Priority order

The first rule that matches wins:

1. End state "died": **Game Over**.
2. End state "won": **Victory**.
3. Any other non-zero end state (quit): silence.
4. Boss visible and bonus track 2 unlocked: **Boss Fight**.
5. A fishing event happened less than 20,000 ms ago and bonus track 1 is unlocked: **Surf's Up**.
6. In bank and bonus track 0 unlocked: **Elevator Music** (also in the level-8 branch office).
7. In bank or in shop: **Shop**.
8. By level index: 0 or below **Town**; 1–4 **Shallows**; 5–15 **Depths**; 16 and above **Volcano**.

Without its tape, a bonus rule is skipped and the later rules apply (a boss on level 14 gets Depths, the bank gets Shop).

The fishing timer is set to "now + 20,000 ms" by every fishing event except the treasure-map kind (a = 3, read on dry land), and the track is re-evaluated at once.

### 5.4 The player's pick

The Radio dialog can set a chosen song. While a song is chosen it replaces the automatic choice, except that rules 1–3 (end states) always win. The automatic picker still runs every tick so its sticky flags stay current. The choice is kept in memory only: it survives from one game to the next while the app stays open and is forgotten when the activity is recreated.

### 5.5 Start, stop, pause

Each tick:

1. If the Radio setting is off or the activity is not resumed: ask the mixer for silence and stop the audio thread at once (no audible fade).
2. Otherwise note whether a game is running. On the transition from not running to running (a new game), clear the remembered end event and the remembered last level.
3. If a game is running, the song is the result of sections 5.1–5.4 (silence if there is no state yet). If no game is running, the song is the current one only if it is a non-looping stinger (so Game Over or Victory plays out over the start screen); otherwise silence, which fades the music out.
4. Give the song to the mixer. Start the audio thread if the mixer is not silent, stop it if it is.

When the app goes to the background the tick stops and the audio thread is stopped directly, without changing the mixer's song; on return the same sequencer continues from where it was. Died and Won events also trigger an immediate re-evaluation so the stinger starts without waiting for the tick.

### 5.6 Cassette tapes

On a tape event (and only while the Radio setting is on): set bit a in the `unlocked` preference if a is 0, 1 or 2; play the cassette effect; and show a long toast "New radio track: <name> (menu → Radio)". Unlocked tapes persist for every later game. (The tapes lie on dungeon levels 2 and 6 and volcano level 2; see Part 12.)

### 5.7 The Radio dialog

A dialog titled "Dungeon Radio" with one button, "Close", and a scrollable column (padding 20 dp left and right, 8 dp top). Buttons keep their written capitalisation. From top to bottom:

1. A 16 sp text: "Now playing: <song name>" (or "Now playing: nothing"), then on a second line "(follows the dungeon)" if nothing is chosen, or "(your pick)". The song shown is the chosen one, else the mixer's current one. The text is refreshed only when one of the dialog's buttons is pressed, not continuously.
2. Button "Next track". The cycle is: follow the dungeon, Town, Shallows, Depths, Volcano, Shop, then each unlocked bonus track in index order, then back to follow the dungeon. Pressing moves the choice one place along.
3. Button "Follow the dungeon": clears the choice.
4. Bold header "Cassette tapes". Then for each bonus track in index order: if unlocked, a button "▶ <name>" that chooses that track; otherwise a 70%-opacity line "???  (a tape still hidden in the dungeon)" (two spaces after the question marks). The list reflects the tapes held when the dialog was opened.
5. Bold header "Volume". A label "Music: N%" over a slider 0–100, then a label "Sound effects: N%" over a slider 0–100. Dragging stores the value, applies it to the mixer at once and updates the label. Releasing the sound-effects slider plays the Gold effect as a sample (if the radio is on and the app is in the foreground).
6. A small (12 sp, 70% opacity) note: "Turn the radio off in Settings → Wild features."

Choosing a track takes effect immediately.

---

## 6. Sound effects

Effects play only while the Radio setting is on and the activity is resumed. Starting one also starts the audio thread if it was stopped. Each is at most 150 ms long in total. For noise segments the frequencies are shift-register clock rates. "→" marks a glide; a single number is a steady pitch. Volume is 1.0 unless given.

| Effect | Segments, in order | Total |
|---|---|---|
| Hit | noise 1800→500 Hz, 90 ms, vol 0.9; triangle 110→70 Hz, 40 ms | 130 ms |
| Kill | pulse 50% 520→130 Hz, 70 ms; noise 3000→800 Hz, 70 ms, vol 0.8 | 140 ms |
| Take | pulse 25% 660 Hz, 40 ms; pulse 25% 990 Hz, 60 ms | 100 ms |
| Gold | pulse 50% 988 Hz, 45 ms, vol 0.8; pulse 50% 1319 Hz, 90 ms, vol 0.8 | 135 ms |
| Stairs down | triangle 600→150 Hz, 140 ms | 140 ms |
| Stairs up | triangle 150→600 Hz, 140 ms | 140 ms |
| Spell | pulse 12.5% 400→1600 Hz, 120 ms, vol 0.9 | 120 ms |
| Level up | pulse 25% 523, 659, 784 Hz for 35 ms each, then 1047 Hz for 40 ms | 145 ms |
| Cassette | noise 9000 Hz, 20 ms, vol 0.6; pulse 50% 784 Hz, 50 ms; pulse 50% 1047 Hz, 70 ms | 140 ms |
| Card | pulse 12.5% 1568 Hz 40 ms, 2093 Hz 40 ms, 2637 Hz 50 ms, all vol 0.8 | 130 ms |
| Clunk | triangle 120→60 Hz, 80 ms; noise 1500→500 Hz, 60 ms, vol 0.7 | 140 ms |
| Splash | noise 4000→1200 Hz, 140 ms, vol 0.8 | 140 ms |
| Chirp | pulse 25% 1200→1800 Hz, 60 ms, vol 0.7; pulse 25% 1500→2200 Hz, 60 ms, vol 0.7 | 120 ms |
| Glitch | noise 20000→200 Hz, 60 ms; pulse 12.5% 3000→100 Hz, 80 ms | 140 ms |
| Pop | pulse 50% 300→900 Hz, 60 ms, vol 0.8 | 60 ms |

Which event plays what:

| Event | Effect |
|---|---|
| Kill | Kill |
| Level up | Level up |
| New level | Compared with the level index of the previous New-level event of this game: none if there was no previous one or it is the same level; Stairs up if the new index is lower; Stairs down if higher |
| Take | Take |
| Player hit | Hit |
| Gold | Gold |
| Spell | Spell |
| Died, Won | None (the stinger plays on the music bus) |
| Radio tape | Cassette |
| Card drop | Card (even when card collecting is off in the app) |
| Vending (any kind) | Clunk |
| Hat (put on or taken off) | Pop |
| Coin | None for a = 0 (the silent price tick), otherwise Gold |
| Glitch (start and end) | Glitch |
| Fishing | Gold for a = 3 (treasure map), otherwise Splash |
| Pet (any kind) | Chirp |
| Perk (any kind) | Level up |
| Weather | None |
| Anything else | None |

The photo shutter click reuses the Take effect (section 9.5).

---

## 7. Trading cards: data and rules

### 7.1 Drop rules

The drop decision is made by the game (C side) at each kill, only when the game's Cards bit is on:

1. Ignore monster ids outside 1..(monster count − 1).
2. The kill has already been counted. If this is not the first kill of this species by this character (count above 1) and rnd(100) is greater than 15, there is no drop. So the first kill always drops, later kills drop 15% of the time.
3. Holo if rnd(100) is 5 or less: 5% of drops.
4. Emit a Card event with a = monster id, b = 1 for holo, else 0.

The per-species kill counts belong to the character and are saved with it (Part 12), so every new character's first kill of each species drops a card again.

On the app side, for each Card event: if the Cards setting is off, do nothing. Otherwise add the card to the album with the current run id and the wall-clock time, save the album immediately, and, if "Card drop toasts" is on, queue a popup.

### 7.2 The card list

There are 69 cards, one per monster id 1–69 in the game's monster order; id 0 ("unseen attacker") is not a card. The collector number printed on a card is its monster id. The card table mirrors the game's monster table (name, level, armour class, damage, hit points, experience); the cards only display these numbers.

**Rarity rule**, first match wins:

1. Ids 61–69 (the demon lords, the demon prince, the God of Hellfire): **Legendary**.
2. The six dragons (ids 25, 48, 49, 53, 54, 56) and the six one-of-a-kind characters (gnome king 45, water lord 47, null pointer 57, fountain kraken 58, tax auditor 59, roomba 60): **Epic**.
3. Otherwise by monster level: 1–3 **Common**, 4–6 **Uncommon**, 7 and above **Rare**.

Counts: 17 Common, 15 Uncommon, 16 Rare, 12 Epic, 9 Legendary.

**Art tile rule**: ids 1–56 use sheet tile id − 1 (tiles 0–55); ids 57–60 use tiles 384–387; id 61 uses tile 56; ids 62–69 use tile id − 6 (tiles 56–63). Demon lord types I and II therefore share tile 56, as in the game.

| No. | Name | LV | AC | DAM | HP | EXP | Tile | Rarity |
|---|---|---|---|---|---|---|---|---|
| 1 | lemming | 1 | 0 | 0 | 0 | 1 | 0 | Common |
| 2 | gnome | 1 | 10 | 1 | 2 | 2 | 1 | Common |
| 3 | hobgoblin | 1 | 13 | 2 | 3 | 2 | 2 | Common |
| 4 | jackal | 1 | 7 | 1 | 1 | 1 | 3 | Common |
| 5 | kobold | 1 | 15 | 1 | 1 | 1 | 4 | Common |
| 6 | orc | 2 | 15 | 3 | 5 | 2 | 5 | Common |
| 7 | snake | 2 | 10 | 1 | 3 | 1 | 6 | Common |
| 8 | giant centipede | 2 | 13 | 1 | 2 | 2 | 7 | Common |
| 9 | jaculi | 2 | 9 | 1 | 2 | 1 | 8 | Common |
| 10 | troglodyte | 2 | 10 | 2 | 5 | 3 | 9 | Common |
| 11 | giant ant | 2 | 8 | 1 | 5 | 4 | 10 | Common |
| 12 | floating eye | 3 | 8 | 2 | 7 | 2 | 11 | Common |
| 13 | leprechaun | 3 | 3 | 0 | 15 | 40 | 12 | Common |
| 14 | nymph | 3 | 3 | 0 | 20 | 40 | 13 | Common |
| 15 | quasit | 3 | 5 | 3 | 14 | 10 | 14 | Common |
| 16 | rust monster | 3 | 5 | 0 | 18 | 20 | 15 | Common |
| 17 | zombie | 3 | 12 | 3 | 9 | 7 | 16 | Common |
| 18 | assassin bug | 4 | 4 | 3 | 23 | 13 | 17 | Uncommon |
| 19 | bugbear | 4 | 5 | 4 | 24 | 33 | 18 | Uncommon |
| 20 | hell hound | 4 | 5 | 2 | 20 | 33 | 19 | Uncommon |
| 21 | ice lizard | 4 | 11 | 3 | 19 | 23 | 20 | Uncommon |
| 22 | centaur | 4 | 6 | 4 | 25 | 43 | 21 | Uncommon |
| 23 | troll | 5 | 9 | 5 | 55 | 250 | 22 | Uncommon |
| 24 | yeti | 5 | 8 | 4 | 45 | 90 | 23 | Uncommon |
| 25 | white dragon | 5 | 4 | 5 | 65 | 1000 | 24 | Epic |
| 26 | elf | 5 | 3 | 3 | 25 | 33 | 25 | Uncommon |
| 27 | gelatinous cube | 5 | 9 | 3 | 24 | 43 | 26 | Uncommon |
| 28 | metamorph | 6 | 9 | 3 | 32 | 40 | 27 | Uncommon |
| 29 | vortex | 6 | 5 | 4 | 33 | 53 | 28 | Uncommon |
| 30 | ziller | 6 | 15 | 3 | 34 | 33 | 29 | Uncommon |
| 31 | violet fungus | 6 | 12 | 3 | 39 | 90 | 30 | Uncommon |
| 32 | wraith | 6 | 3 | 1 | 36 | 300 | 31 | Uncommon |
| 33 | forvalaka | 6 | 3 | 5 | 55 | 270 | 32 | Uncommon |
| 34 | lama nobe | 7 | 14 | 7 | 36 | 70 | 33 | Rare |
| 35 | osequip | 7 | 4 | 7 | 36 | 90 | 34 | Rare |
| 36 | rothe | 7 | 15 | 5 | 53 | 230 | 35 | Rare |
| 37 | xorn | 7 | 6 | 7 | 63 | 290 | 36 | Rare |
| 38 | vampire | 7 | 5 | 4 | 55 | 950 | 37 | Rare |
| 39 | invisible stalker | 7 | 5 | 6 | 55 | 330 | 38 | Rare |
| 40 | poltergeist | 8 | 1 | 8 | 55 | 430 | 39 | Rare |
| 41 | disenchantress | 8 | 3 | 1 | 57 | 500 | 40 | Rare |
| 42 | shambling mound | 8 | 13 | 5 | 47 | 390 | 41 | Rare |
| 43 | yellow mold | 8 | 12 | 4 | 37 | 240 | 42 | Rare |
| 44 | umber hulk | 8 | 6 | 7 | 67 | 600 | 43 | Rare |
| 45 | gnome king | 9 | −1 | 10 | 120 | 3000 | 44 | Epic |
| 46 | mimic | 9 | 9 | 7 | 57 | 100 | 45 | Rare |
| 47 | water lord | 9 | −10 | 15 | 155 | 15000 | 46 | Epic |
| 48 | bronze dragon | 9 | 5 | 9 | 90 | 4000 | 47 | Epic |
| 49 | green dragon | 9 | 4 | 4 | 80 | 2500 | 48 | Epic |
| 50 | purple worm | 9 | −1 | 13 | 130 | 15000 | 49 | Rare |
| 51 | xvart | 9 | −2 | 14 | 100 | 1000 | 50 | Rare |
| 52 | spirit naga | 10 | −20 | 15 | 100 | 20000 | 51 | Rare |
| 53 | silver dragon | 10 | −4 | 10 | 110 | 10000 | 52 | Epic |
| 54 | platinum dragon | 10 | −7 | 15 | 150 | 25000 | 53 | Epic |
| 55 | green urchin | 10 | −5 | 12 | 95 | 5000 | 54 | Rare |
| 56 | red dragon | 10 | −4 | 13 | 120 | 14000 | 55 | Epic |
| 57 | null pointer | 8 | 2 | 7 | 60 | 400 | 384 | Epic |
| 58 | fountain kraken | 7 | 4 | 6 | 70 | 300 | 385 | Epic |
| 59 | tax auditor | 6 | 6 | 0 | 40 | 50 | 386 | Epic |
| 60 | roomba | 4 | 8 | 1 | 25 | 30 | 387 | Epic |
| 61 | type I demon lord | 12 | −40 | 20 | 150 | 50000 | 56 | Legendary |
| 62 | type II demon lord | 13 | −45 | 25 | 200 | 75000 | 56 | Legendary |
| 63 | type III demon lord | 14 | −50 | 30 | 250 | 100000 | 57 | Legendary |
| 64 | type IV demon lord | 15 | −55 | 35 | 300 | 125000 | 58 | Legendary |
| 65 | type V demon lord | 16 | −60 | 40 | 350 | 150000 | 59 | Legendary |
| 66 | type VI demon lord | 17 | −65 | 45 | 400 | 175000 | 60 | Legendary |
| 67 | type VII demon lord | 18 | −70 | 50 | 450 | 200000 | 61 | Legendary |
| 68 | demon prince | 19 | −90 | 80 | 1000 | 500000 | 62 | Legendary |
| 69 | God of Hellfire | 20 | −120 | 100 | 32767 | 1000000 | 63 | Legendary |

**Card title**: the stored name split on spaces, each word given a capital first letter, except that the word "of" and words that are entirely upper-case are left alone. So "gnome king" becomes "Gnome King", "type I demon lord" becomes "Type I Demon Lord" and "God of Hellfire" is unchanged. Pet and bounty lines (section 12) use the stored lower-case name instead.

### 7.3 Flavour text ("lore")

Each card has one line of flavour text. The 69 lines must all be non-blank and all different (tested). Format and tone: one to three very short sentences, 29 to 69 characters in all (about 46 on average), written as a dry one-line joke about the monster. The jokes lean on a pun on the name, a deadpan remark on its game mechanic (stealing gold, rusting armour, draining levels, the gold it carries), mock product copy, or office humour for the demons. They have to fit in three wrapped lines of small italic type on the card. Examples, quoted exactly:

| Card | Flavour text |
|---|---|
| lemming | "Followed the crowd. The crowd was going to the volcano." |
| gnome | "Short, furious, and 30 gold richer than you." |
| hobgoblin | "A goblin with a hobby. The hobby is stabbing." |
| kobold | "AC 15, HP 1. All shell, no snail." |
| floating eye | "Don't hit it. Don't look at it. Don't even think about it." |
| leprechaun | "Your gold called. It's somewhere else now." |
| rust monster | "Thinks your plate mail is a crunchy snack." |
| troll | "Regenerates. Bring fire, or a very long afternoon." |
| white dragon | "The weakest dragon. Which is still a dragon." |
| invisible stalker | "This card is blank. Don't worry about it." |
| mimic | "Are you sure this is a card? Count your fingers." |
| platinum dragon | "Premium dragon. Premium pain. No refunds." |
| null pointer | "Points at nothing. Hurts like everything. (core dumped)" |
| roomba | "It has mapped the dungeon. It has mapped you." |
| type IV demon lord | "Middle management of the underworld. Loves meetings." |
| God of Hellfire | "The last boss. The last card. Frame it, then run." |

The fields of one card record are therefore: id, name, level, armour class, damage, hit points, experience, tile id, flavour text; rarity and title are derived.

### 7.4 The album

An album entry per owned card holds: monster id; count of copies found (holo ones included); how many of those were holo; the run id of the run that found the first copy (0 if unknown); the wall-clock time in milliseconds of the first copy (0 if unknown). A card is "holo" in the album if its holo count is above zero.

**Adding a drop** (monster id, holo flag, run id, time): ids outside 1–69 are rejected with no change. Otherwise create the entry if it is missing (recording run id and time), note "new holo" = this drop is holo and the entry had no holo before, increase the count, increase the holo count if holo, and report: the monster, whether this drop was holo, "new card" (the entry did not exist), "new holo", and the new count.

**Derived values**: collected = number of entries; holo collected = entries with a holo; complete = collected ≥ 69.

**Storage format** (the `album` string): a JSON array of objects, sorted by monster id, with no whitespace, each object written with exactly these integer fields in this order: `m` monster id, `n` count, `h` holo count, `r` first-seen run id (may be negative), `t` first-seen time in milliseconds. An empty album is `[]`.

**Reading** is deliberately forgiving and never fails: a missing, blank or unparsable string gives an empty album. Every brace-delimited object without nested braces is examined; inside it every quoted-name-colon-integer pair is collected. Objects with no `m`, or with an `m` that is not a card id, are skipped. A missing `n` is 1 and `n` is at least 1; `h` defaults to 0 and is clamped to 0..n; `r` and `t` default to 0. If an id occurs twice the later object wins.

The album is loaded once when the app's card component is created and written back after every drop.

### 7.5 Filters and the count line

Two filters combine. The rarity filter is "all" or one rarity. The ownership filter is one of:

| Filter | Label | Keeps |
|---|---|---|
| All | "All cards" | every card |
| Found | "Found" | cards owned |
| Holo | "Holo" | cards owned with at least one holo copy |
| Missing | "Not found yet" | cards not owned |

The filtered list is always in monster-id order.

The count line is computed over the cards the rarity filter selects (call their number T, of which F are owned and H owned with a holo), worded by the ownership filter, and prefixed by "<Rarity label>: " when a rarity is selected:

| Ownership filter | Text |
|---|---|
| All | "F/T collected", followed by " · H holo" only when no rarity is selected |
| Found | "no cards found yet" if F = 0, else "F/T collected" |
| Holo | "no holo cards yet" if H = 0, else "H holo of F collected" |
| Missing | "none left to find!" if F = T, else "(T − F) of T still to find" |

Examples: "0/69 collected · 0 holo", "Common: 2/17 collected", "Common: 1 holo of 2 collected", "Legendary: no cards found yet", "66 of 69 still to find", "Epic: none left to find!". The separator before the holo count is a middle dot with a space on each side.

---

## 8. Trading cards: what the player sees

### 8.1 Rarity colours

| Rarity | Label | Frame | Frame light | Frame dark | Pips |
|---|---|---|---|---|---|
| Common | "Common" | FF8C8C8C | FFC8C8C8 | FF4A4A4A | 1 |
| Uncommon | "Uncommon" | FF4E9A3C | FF8FD46E | FF24501A | 2 |
| Rare | "Rare" | FF3D6FC4 | FF86AEF0 | FF1C3566 | 3 |
| Epic | "Epic" | FF8E44C0 | FFC98AF0 | FF46205E | 4 |
| Legendary | "Legendary" | FFD98A1E | FFFFD36A | FF6E3F08 | 5 |
| (not found yet) | "???" | FF55545C | FF7E7D88 | FF2C2B31 | 0 |

### 8.2 Card rendering

A card is laid out on a grid of 160 × 232 "card pixels" and rendered to a bitmap at an integer scale s from 1 to 8 device pixels per card pixel, so frames and art stay crisp. Rectangles below are (left, top, right, bottom) in card pixels with the right and bottom edges exclusive. Text is drawn anti-aliased at the final resolution; text sizes are given in card pixels (multiply by s). Unless stated, text is bold monospace. A card is drawn in one of three states: known (owned), known and holo, or unknown (not found yet).

Steps, in drawing order:

1. **Outline with clipped corners**: black over (1, 0, 159, 232) and over (0, 1, 160, 231), which leaves the four corner pixels transparent.
2. **Bevelled frame**: frame colour over (2, 2, 158, 230). Light colour along the top (2, 2, 158, 4) and left (2, 2, 4, 230). Then dark colour along the bottom (2, 228, 158, 230) and right (156, 2, 158, 230). An unknown card uses the grey "not found" colours for frame, light and dark throughout.
3. **Inner panel**: black over (7, 7, 153, 225), then the panel colour FF1B1A21 over (8, 8, 152, 224).
4. **Name bar**: dark colour over (10, 10, 150, 27) with a one-pixel line of frame colour at (10, 26, 150, 27). The level, "LV<level>" or "LV?" when unknown, is drawn at size 8 in the light colour, right-aligned to x = 147 with its baseline at y = 21. The name, the card title or "???" when unknown, is drawn in white, left-aligned at x = 13, baseline y = 22, starting at size 10 and shrinking in steps of 0.5 (not below 6) until it fits in 130 card pixels minus the width of the level text.
5. **Art box**: the 128 × 128 square (16, 30, 144, 158) with a one-pixel border of light colour around it (15, 29, 145, 159). Its backdrop is scanlines: alternating one-pixel rows, even rows the dark colour mixed 35% towards black, odd rows mixed 55% towards black (a per-channel linear mix, truncated).
6. **Holo foil** (holo cards only), clipped to the art box: 33 diagonal lines, one for each d from −128 to 128 in steps of 8, each from (16 + d, 158) up and right to (16 + d + 128, 30), 3 card pixels wide, not anti-aliased, cycling through six colours in order at alpha 56 of 255: FF5A5A, FFC94A, 7CF06A, 4AD8FF, 8A7CFF, FF6AE0.
7. **The monster**: its 32 × 32 tile from the tile sheet (16 tiles per row; column = tile id mod 16, row = tile id ÷ 16) enlarged four times into the art box with nearest-neighbour sampling. On an unknown card every opaque pixel of the tile is replaced by the near-black FF09090C, giving a silhouette. If the sheet or the tile cannot be loaded the box stays empty.
8. **Holo badge** (holo cards only): a black rectangle (110, 33, 141, 44) in the art box's top-right corner containing "★HOLO" at size 7, colour FFFFE070, centred on x = 125.5 with baseline y = 41.
9. **Rarity line**: five 4 × 4 pips at x = 12, 18, 24, 30, 36 and y = 163 to 167. The first n are the light colour, where n is the rarity's pip count (0 when unknown), and the rest the cell colour FF2A2833. Then at size 7: the rarity label in capitals in the light colour (or "???" in the grey light colour), left-aligned at x = 44 with baseline y = 168; and the collector number in FF9A98A6, right-aligned to x = 148 on the same baseline, in the form "No.07/69" (the id padded to two digits, a slash, the card total). The collector number is shown even on unknown cards.
10. **Stats**: four cells, 32 wide and 24 high, at x = 12, 46, 80, 114 and y = 172 to 196, in the cell colour with a one-pixel dark line along the top. Each has a label at size 6 in FF8D8B99 centred in the cell with baseline y = 180 ("AC", "DAM", "HP", "XP") and a value at size 9 in white with baseline y = 192. Armour class and damage are shown as plain numbers. Hit points and experience are compacted: a value of 1,000,000 or more as whole millions with "M" ("1M"), a value of 10,000 or more as whole thousands with "k" ("15k", "32k" for 32767), anything smaller in full ("5000"). Unknown cards show "?" in all four.
11. **Flavour text**: italic serif at size 7, colour FFC9C6D3, centred on x = 80. The text is word-wrapped greedily to a width of 136 card pixels (a word that is too long on its own stays on its own line) and cut to at most three lines. The first baseline is at y = 204 + 3.5 × (3 − number of lines) and lines are 8 apart, which centres one or two lines in the space of three. Unknown cards show "Not found yet. Go and hit things."

Rendered bitmaps are cached (up to 16 MB, least recently used evicted) by monster, known/unknown, holo and scale. To show a card at a given pixel width, the scale chosen is the smallest integer that makes the bitmap at least that wide (width ÷ 160 rounded up, limited to 1–8), so views only ever shrink the bitmap slightly.

### 8.3 Showing a card, and the holo shimmer

A card view fits the bitmap into its area, centred, keeping the 160:232 aspect ratio (its own preferred height is its width × 232 ÷ 160). Enlarging uses nearest-neighbour sampling; shrinking uses smooth filtering.

For a holo card two animated layers are drawn over the bitmap every animation frame, clipped to the card. Let w and h be the card's on-screen width and height and t = (uptime in ms mod 3000) ÷ 3000, a three-second loop.

* **Rainbow sheen**: a diagonal linear gradient (running from the top-left towards the bottom-right over a distance of 0.9w in both x and y) through FF4D4D, FFC23D, 6CF05A, 3DD6FF, 7A6CFF, FF5AD8 and back to FF4D4D, mirrored outside that span; drawn with "screen" blending at alpha 70 of 255. Its origin is the card's top-left corner displaced by 1.8w × t in both x and y, so the rainbow drifts along the diagonal and loops seamlessly.
* **Glint**: a narrow diagonal band (gradient span 0.25w in x and y) from transparent through white at alpha 0x88 back to transparent, "screen" blended, with its origin displaced from the top-left corner by g × (w + h) ÷ 2 in both x and y, where g = −0.4 + 1.8t. It sweeps from just outside the top-left corner to beyond the bottom-right once per loop.

### 8.4 The drop popup

Shown only when "Card drop toasts" is on. Popups appear one at a time at the top centre of the app window, 56 dp from the top. Up to four drops wait in a queue; if a fifth arrives while four are waiting the oldest waiting one is discarded.

* Appearance: a horizontal pill with a 10 dp corner radius, fill colour (22, 22, 26) at alpha 235, a 2 dp border in the card's rarity frame colour, 8 dp elevation, and padding 8 dp left, 6 dp top and bottom, 14 dp right. It contains a thumbnail of the card 40 dp wide (58 dp tall) drawn as the album now holds it, then, 10 dp to the right, the text in bold 15 sp, white, or gold (255, 214, 90) when this drop is holo.
* Text: if the drop is a new card or the first holo of a card, "New card: <Title>"; otherwise "Card: <Title> (xN)" with N the new count. When this drop is holo, " ★ HOLO" is inserted directly after the title in either form ("New card: Lemming ★ HOLO", "Card: Lemming ★ HOLO (x3)"). The same text is announced to accessibility services.
* Animation: it fades in over 180 ms while sliding down 16 dp into place. 2,600 ms after it appears for a new card or new holo, or 1,700 ms otherwise, it fades out over 220 ms, is removed, and the next queued popup is shown.
* Tapping it opens that card's full-screen view.

### 8.5 The album dialog

A dialog titled "Card album" with one button, "Close". Content, top to bottom, with 16 dp side padding:

1. The count line (15 sp), section 7.5.
2. Two drop-down lists side by side, equal width: rarity ("All rarities", "Common", "Uncommon", "Rare", "Epic", "Legendary"; accessibility description "Filter by rarity") and ownership ("All cards", "Found", "Holo", "Not found yet"; description "Filter by found or not found"). Changing either rebuilds the grid and the count line. Both start on their first entry.
3. Only if the album is complete when the dialog opens, a gold (255, 200, 80) line: "Complete set! Gotta kill 'em all - and you did."
4. A scrolling grid whose height is 62% of the screen height. Columns are 92 dp wide and as many as fit, spread evenly; 10 dp between rows, at least 6 dp between columns, 6 dp padding above and below.

Each grid cell is a card view 92 dp wide (133 dp tall) above a centred 12 sp caption. Owned cards are drawn in full, with the shimmer if holo; cards not yet found are drawn in the unknown state: grey frame, black silhouette, "???" for the name and rarity, "LV?", "?" in the stats, the stand-in flavour line, but with the real collector number. Captions: "???" if not owned, "xN" if owned, "★ xN" if owned with at least one holo copy (N is the total count).

Accessibility description of a cell: for an unowned card "Card <id>: not found yet"; for an owned card "<Title>, <Rarity label>" then ", holo" if it has a holo, then ", owned N. Level L, armour A, damage D, hit points H, experience E. <flavour text>".

Tapping a cell opens the full-screen view.

### 8.6 Full-screen card view

A full-screen black dialog with no title bar; background (8, 8, 10) at alpha 240. Centred: the card, as wide as the smaller of 86% of the screen width and the width at which its height would be 72% of the screen height; rendered at the scale section 8.2 chooses for that width; shimmering if holo. Below it, 14 dp down, centred light-grey 14 sp text:

* not owned: "Not found yet";
* owned: "Owned: N", followed by " (H holo)" if H > 0, and on a new line "First found <date>" (the device's default date format) if the first-seen time is known;
* then in both cases a blank line and "Tap to close".

Tapping anywhere closes it.

---

## 9. Photo mode

### 9.1 Taking a photo

"Take a photo" (menu entry or control button):

1. If the Photo setting is off, do nothing.
2. Capture and compose the picture (9.2–9.4). If there is nothing to capture, show a short toast "Nothing to photograph right now" and stop.
3. Flash and click (9.5).
4. Keep a private copy as the "last photo" (9.8), save to the gallery (9.6) and show the result dialog (9.7).

### 9.2 Capture source

The photo is not a screenshot. The map is re-rendered into an off-screen bitmap from the same cells the game view last drew (the visible window of the map at the current zoom and camera position):

* Capture is possible only if the tile sheet is loaded, the view has drawn the map at least once, and the display is in map mode (not a full-screen text page).
* Tile size: 64 px per cell (twice the 32 px source tiles), reduced in steps of 8 px, never below 8, until the longer side of the bitmap is at most 2,560 px.
* The bitmap is filled with the map background colour; then for each cell, in order: the map tile; the player's hat tile on the player's cell (only when the player sprite is being shown and a hat is worn); the cell's overlay tile if any; the pet heart (tile 397) on the pet's cell.
* If the weather pass is active (section 10), the rain, snow or stars are drawn for each cell using the current animation frame, and the day/night tint is multiplied over the whole bitmap.
* Not included: the player cursor, the Glitch Realm corruption (tiles, slicing, hue), the HUD, the minimap, the danger overlay, the message area.

### 9.3 Caption and date

Caption = "<who> the <title> — <where>, turn <turn>", with an em dash:

* who: the character's name, trimmed. It comes from the options text of the running game (the text after "name:" up to the next comma or line end), falling back to the name in Settings; if empty, "An adventurer".
* title: the character's current level title, trimmed (for example "novice explorer"). If empty, " the <title>" is left out.
* where: from the short level name: empty or "H" gives "home"; a name starting with "V" gives "the volcano, <name>"; anything else gives "level <name>".
* turn: the game's turn counter.

Examples: "Ann the novice explorer — home, turn 0"; "Bob the knight — level 7, turn 1234"; "An adventurer — the volcano, V2, turn 5".

The date line is the current date in the device locale's long date format (for example "November 20, 2026").

### 9.4 Polaroid composition

For a captured map of w × h px (each treated as at least 1), all in px of the final image, where round() is to the nearest integer:

| Quantity | Formula |
|---|---|
| side border (also the top border) | max(16, round(0.05 × max(w, h))) |
| bottom border (the "chin") | round(4.2 × side) |
| frame width | w + 2 × side |
| frame height | h + side + bottom |
| photo rectangle | left = top = side; right = side + w; bottom = side + h |
| caption text size | 0.30 × bottom |
| date text size | 0.20 × bottom |
| caption baseline y | h + side + round(0.45 × bottom) |
| date baseline y | h + side + round(0.78 × bottom) |
| caption and date left x | side + side ÷ 2 (integer division) |
| room for the caption | w − side |

There is no rotation of the frame itself. Drawing order:

1. Fill the frame with the paper colour (244, 241, 232), an off-white.
2. Draw the photo into the photo rectangle without smoothing.
3. Vignette over the photo: a radial gradient centred on the photo's centre with radius equal to half the photo's diagonal; fully transparent from the centre out to 55% of the radius, then darkening to black at alpha 120 of 255 at the corners.
4. A thin inner edge: the photo rectangle outlined in black at alpha 60, line width max(1, side ÷ 16).
5. Caption: italic serif (the "handwritten" look), anti-aliased, ink colour (43, 43, 58), left-aligned. If it is wider than the room at the nominal size, the size is scaled down by room ÷ measured width, but never below half the nominal size (a very long caption may then run past the photo's right edge).
6. Date: same typeface, dim ink (110, 108, 118), at the date size, on the date baseline.
7. Stickers.

**Stickers.** One to three pixel-art stickers straddle corners of the photo. They are chosen by a small deterministic generator seeded with the capture time in milliseconds (the same value used for the date):

* State s = seed XOR 0x5DEECE66D (64-bit). next(n): s = s × 6364136223846793005 + 1442695040888963407 (64-bit wrap-around); the result is (s shifted right 33 bits, unsigned) mod n.
* count = 1 + next(3). Sticker size = max(32, 2 × side) px square. first = next(4).
* The photo's corners are numbered 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left. Sticker i (0-based) sits at corner (first + i) mod 4, so stickers occupy consecutive corners going clockwise and never share one.
* Its centre is the corner moved a quarter of the sticker size towards the inside of the photo in both x and y, so three quarters of its width and of its height lie over the photo and the rest overhangs the border.
* Its tile: for the first sticker only, if the player's sprite tile is known, draw next(2); a result of 0 selects the player's sprite (the tile currently shown on the player's cell). In every other case the tile is entry next(7) of this list: 113, 114, 115, 116 (the four gems: diamond, ruby, emerald, sapphire), 105 (potion), 104 (scroll), 81 (gold pile).
* Its rotation = next(31) − 15 degrees (−15 to +15).
* The draws happen in exactly this order: count, first, then per sticker the optional player draw, the optional list draw, the rotation.

Each sticker is drawn rotated about its centre: first a soft shadow, a rounded rectangle that is the sticker square grown by pad = size ÷ 10 on every side and shifted by pad ÷ 3 right and down, black at alpha 70, corner radius pad; then a white rounded rectangle, the square grown by pad, corner radius pad; then the tile, unsmoothed, filling the square. A sticker whose tile is unavailable is skipped.

### 9.5 Flash and click

* **Flash**: a white layer over the entire app window whose alpha animates from 230 to 0 over 350 ms, then is removed.
* **Click**: if the Radio setting is on and the sound-effects volume is above 0, the radio's Take effect is played as the shutter sound. Otherwise the system tone generator plays its "acknowledge" tone on the music stream at volume 70 for 120 ms (released 400 ms later); if that is unavailable the camera is silent.

### 9.6 Saving to the gallery

File name: "ularn-wild-<milliseconds since 1970 at save time>.png". Format: PNG. Album folder: "Ularn Wild" inside the shared Pictures directory.

* **Android 10 (API 29) and later**: no permission is needed. Insert a media-store image record with the display name, MIME type "image/png", relative path "Pictures/Ularn Wild" and the "pending" flag set; write the PNG to it; clear the pending flag. If the record cannot be created the save fails; if writing fails the record is deleted and the save fails.
* **Android 7–9 (API 24–28)**: the legacy storage-write permission is required (declared in the manifest only up to API 28). If it is not yet granted, the composed photo is held and the permission is requested (request code 4710); when the answer arrives, the photo is saved if granted, and the result dialog is shown either way. Saving creates the folder if needed, writes the file, and registers it with the media store (display name, MIME type and file path).
* Any error makes the save fail quietly; the dialog then says the photo was not saved.

### 9.7 Result dialog and sharing

A dialog showing the composed photo (scaled to fit, 12 dp padding on the left, top and right; accessibility description "Your photo"):

* Title "Saved to Pictures/Ularn Wild" if the save succeeded, otherwise "Photo (not saved to the gallery)".
* Button "OK". If the save succeeded, also a "Share" button.
* "Share" opens the system share sheet titled "Share photo" for the saved image (type image/png, read access granted to the receiving app) with the accompanying text "A snapshot from Ularn Wild". If no app can take it, a short toast "No app to share with".

### 9.8 Auto-photo and the "last photo"

* On a Died or Won event, if the Photo setting and "Auto-photo on death / victory" are both on, the final scene is captured and composed exactly as above (nothing happens if the map is not showing at that moment). There is no flash, no click and no dialog. The picture becomes the last photo; and if saving needs no question (Android 10 or later, or the permission was already granted) it is saved to the gallery, with a short toast "Photo of your final moment saved to Pictures/Ularn Wild" on success. On Android 7–9 without the permission it is kept only as the last photo.
* **Last photo**: every photo, manual or automatic, is also written as a PNG to the app-private file `photos/last.png`, replacing the previous one. It is not per save slot.
* The Statistics & achievements dialog shows it, when the file exists, under a header "Last photo" placed after the "Current game" section and before the achievements list, as a full-width image (accessibility description "Your last photo"). It is decoded at reduced resolution: the sampling factor doubles while the image width divided by twice the factor is still at least the screen width.

---

## 10. Weather and day/night rendering

The game keeps the hour and the weather (Part 12) and exports one packed integer: the hour (0–23) in the low byte and the weather state in the next byte. States: 0 clear, 1 rain, 2 snow, 3 storm. An hour is 50 turns, a day 1,200 turns, and a new game starts at 08:00: hour = (turn ÷ 50 + 8) mod 24 with integer division. Night is hours 21, 22, 23 and 0–5.

### 10.1 When the pass is active

Once per drawn frame the pass decides whether it is active: the player is on level index 0 (the surface), the display is in map mode, and the game's own Weather bit is set. When inactive nothing in this section is drawn, including the HUD icon, so sun and moon are only ever seen on the surface.

When active it takes from the snapshot: the hour (clamped to 0–23), the state, and a "minute" fraction for blending the tint within the hour: (turn mod 50) ÷ 50 if the hour computed from the turn counter equals the hour the game sent, otherwise 0 (the two can briefly disagree because the turn counter is only exported when the status line is repainted).

The animation frame number is uptime in ms ÷ 100 (10 frames per second), or, in reduced mode, the game turn number. While not in reduced mode, and while it is raining, snowing or storming or a flash is showing, the view schedules a redraw every 100 ms.

Every "random" choice below is a deterministic hash of the cell and the frame, so a frame redrawn for another reason looks the same. The hash H(a, b, c) of a 64-bit a and two 32-bit integers (c defaults to 0) is, in 64-bit wrap-around arithmetic: h = a × 0x9E3779B97F4A7C15 + b × 0x5BD1E995 + c × 0x27D4EB2D; h = h XOR (h shifted right 31, unsigned); h = h × 0x7FB5D329728EA185; h = h XOR (h shifted right 27, unsigned); result = the top 31 bits of h (h shifted right 33, unsigned), a non-negative integer. The Glitch pass uses the same function. An equivalent well-mixed hash gives the same look.

### 10.2 Day/night tint

A colour is multiplied over the whole map rectangle (multiply blend: each channel of the map is scaled by the tint channel ÷ 255), after all tiles and weather tiles are drawn. White is skipped. Key colours:

| Name | Colour |
|---|---|
| Day | (255, 255, 255) |
| Night | (153, 160, 214) |
| Dawn | (255, 212, 178) |
| Dusk | (250, 186, 160) |

Key colour for each hour (mixes are per-channel linear, rounded to nearest):

| Hour | Key colour | Value |
|---|---|---|
| 0–4 | Night | (153, 160, 214) |
| 5 | halfway Night to Dawn | (204, 186, 196) |
| 6 | Dawn | (255, 212, 178) |
| 7 | halfway Dawn to Day | (255, 234, 217) |
| 8–16 | Day | (255, 255, 255) |
| 17 | 40% of the way Day to Dusk | (253, 227, 217) |
| 18 | Dusk | (250, 186, 160) |
| 19 | 40% of the way Dusk to Night | (211, 176, 182) |
| 20 | 80% of the way Dusk to Night | (172, 165, 203) |
| 21–23 | Night | (153, 160, 214) |

The tint at a given moment is the key colour of the current hour mixed towards the key colour of the next hour (hour 23 wraps to 0) by the minute fraction; then every channel is raised to at least 153 (60% of 255). So the map is never darkened by more than 40% in any channel; the deepest darkening is the red channel at night, exactly at that limit. The curve is: no tint from 08:00 until 17:00, warming through the 17:00 hour into a dusky orange-pink at 18:00, cooling to deep blue by 21:00, blue all night, warming from 05:00 to a peach dawn at 06:00, and clearing to white by 08:00.

The tint never animates and is unaffected by reduced mode.

### 10.3 HUD sun and moon

While the pass is active, the second HUD line ("Lv … · Exp … · Home") is followed by an icon: tile 406 (sun) by day, tile 407 (moon) at night. The icon is a square one HUD line high, placed 4 dp after the end of the line's text (or, if the text is too long, pushed no further right than leaves room for it); the text's available width is reduced by the icon's width plus 4 dp. Weather state does not change the icon.

### 10.4 Rain and snow

After all map cells are drawn, each visible cell gets a weather step based on its minimap class (low five bits of the exported per-cell category; a monster flag in the top bit is ignored).

Only cells whose class is exactly "floor" (open ground) can carry precipitation; walls, buildings, items, features and unexplored cells never do.

1. No precipitation in clear weather.
2. h = H(7, 131x + 3, y). The cell is wet if h mod odds = 0, with odds = 5 in reduced mode, else 2 in a storm, else 3. So about a third of floor cells in rain or snow, half in a storm, a fifth when reduced. Which cells are wet depends only on position, not on time: the pattern of wet cells is fixed and only the streaks inside them move.
3. Tile = base + ((h ÷ odds + frame ÷ 4) AND 1), base 400 for rain and storm (tiles 400 and 401), 402 for snow (tiles 402 and 403). Each wet cell therefore flips between the two variants every 4 frames (0.4 s), half the cells out of phase with the other half.
4. Drift, with size = the cell's height in px and phase = H(11, x, y) mod size:
   * snow: dy = (frame × size ÷ 24 + phase) mod size (one cell height every 24 frames, 2.4 s); dx = round(sin((frame + phase) ÷ 6) × size ÷ 10), a gentle sway of a tenth of a cell;
   * rain: step = size ÷ 4 px per frame; storm: step = size ÷ 3 (integer division, at least 1); dy = (frame × step + phase) mod size; dx = dy ÷ 3, a slant to the right as it falls.
5. The tile is drawn twice, unsmoothed and at the cell's size: once at the cell's position shifted by (dx, dy), and once more one cell height higher and, for rain and storm, a third of a cell further left (no horizontal offset for snow). The drawing is clipped to the map area but not to the cell, so each wet cell paints a continuously scrolling two-cell-tall strip that overlaps the cells above and below it.

### 10.5 Stars

At night, cells whose class is "unknown" (unexplored, drawn black) may show a star tile: h = H(13, 31x + 1, y); about one cell in four (h mod 4 = 0) gets one. Tile = 404 + ((h ÷ 4 + (slow + h mod 7) ÷ 3) AND 1) with integer division, where slow = frame ÷ 10 (it advances once a second). Each star therefore switches between tiles 404 and 405 every three seconds, with stars out of step with one another: a slow twinkle. Stars are drawn at the cell's exact position, unsmoothed. By day unexplored cells get nothing.

### 10.6 Lightning

A flash is a white rectangle over the entire game view (HUD and all), drawn last of everything. Its alpha by age: 200 of 255 for the first 60 ms, then falling linearly to 0 at 260 ms (alpha = 200 × (260 − age) ÷ 200, integer division); the view redraws every animation frame while it lasts.

Flashes start in two ways, never in reduced mode:

* a Weather event with c = 1 (the game's lightning strike), at any time;
* ambiently, while the pass is active and the state is storm: when a storm is first seen a timer is set, and each time it expires a flash starts and the timer is set again. The delay is 6,000 ms + (H(current time in ms, 17) mod 14,000), that is 6 to 20 seconds. The timer is cleared whenever the pass is active and the state is not storm.

### 10.7 Reduced weather effects

With "Reduce weather effects" on:

* the frame number is the game turn, so rain, snow and star variants change only when a turn passes and nothing moves in between;
* precipitation is thinner (one floor cell in five);
* stars never twinkle (the slow counter is fixed at 0);
* there are no lightning flashes, neither from events nor ambient;
* no periodic redraw is scheduled.

The tint and the HUD icon are unchanged.

---

## 11. Glitch Realm rendering

The game exports "glitch turns left" (Part 12: 60–120 turns after a scroll of segfault). The pass is active while that value is above 0 and the display is in map mode. Its frame number is uptime in ms ÷ 125 (8 frames per second), or, in reduced mode, the number of turns left. While active and not reduced, the view schedules a redraw every 125 ms. All randomness uses the hash H of section 10.1 so a frame is stable however often it is redrawn.

### 11.1 Order within the map drawing

1. The map area is clipped. If not reduced, an off-screen layer covering the map rectangle is opened whose contents will be hue-rotated when it is composited back.
2. For each visible cell: the destination rectangle is shifted sideways if its row is "slipped" (11.3); then the map tile, the hat, the overlay tile and the pet heart are drawn; then the corruption tile if this cell has one (11.2); then the player cursor.
3. The weather pass draws its rain, snow or stars (unshifted).
4. The layer is composited back with the hue rotation (11.4). Everything drawn in steps 2 and 3 is therefore hue-rotated together.
5. The day/night tint is multiplied on top (not hue-rotated). The minimap and danger overlay follow, untouched.

### 11.2 Corruption tiles

Each frame, between 3 and 6 distinct visible cells get a corruption tile. With the visible range w cells wide and v cells high (nothing if either is 0):

* wanted = min(3 + H(frame, 1) mod 4, w × v);
* candidates k = 0, 1, 2, … up to 8 × wanted attempts: r = H(frame, 2, k); the cell is column (first visible column + r mod w), row (first visible row + (r ÷ w) mod v); a cell already chosen is skipped; stop when enough cells are chosen.

The tile drawn on a chosen cell (x, y) is 408 + (H(frame, 131x + 7, y) mod 8), one of the eight corruption tiles 408–415, drawn unsmoothed at alpha 200 of 255 over the cell's contents, in the cell's (possibly shifted) rectangle. So about eight times a second a new handful of squares flickers with noise.

### 11.3 Slipping slices

Not in reduced mode. Shift distance = max(1, tile size in px ÷ 8, truncated): 4 px at 32 px tiles. For each map row: r = H(frame, 3, row); if r mod 5 = 0 the whole row is drawn displaced horizontally by the shift distance, to the right if (r ÷ 5) is even and to the left if odd. About one row in five slips each frame, and which rows slip changes every frame.

### 11.4 Hue rotation

Not in reduced mode. The whole map layer is hue-rotated by (turns left × 37) mod 360 degrees. The angle changes once per game turn, not per frame, so the colours jump to a new scheme each turn. The rotation preserves luminance; with c = cos(angle), s = sin(angle) and luminance weights lr = 0.213, lg = 0.715, lb = 0.072, the new channels are:

* R' = (lr + c(1 − lr) − s·lr)·R + (lg − c·lg − s·lg)·G + (lb − c·lb + s(1 − lb))·B
* G' = (lr − c·lr + 0.143s)·R + (lg + c(1 − lg) + 0.140s)·G + (lb − c·lb − 0.283s)·B
* B' = (lr − c·lr − s(1 − lr))·R + (lg − c·lg + s·lg)·G + (lb + c(1 − lb) + s·lb)·B

Alpha is unchanged. At 0 degrees this is the identity, and greys stay grey at every angle.

### 11.5 HUD letter corruption

While active, the second HUD line (level, title, experience, location) and the third (the six attributes, AC, WC, spells) are passed through a corruptor before drawing: for each character position i holding anything but a space, r = H(frame, 4, i); if r mod 20 = 0 the character is replaced by one of four block glyphs chosen by (r ÷ 20) mod 4 from: dark shade "▓", full block "█", medium shade "▒", light shade "░". About one non-space character in twenty is replaced; spaces and the line's length are preserved. Because the hash depends only on the frame and the position, both lines are corrupted at the same character positions in a given frame. The hit-point line, the gold figure, the coin ticker, the effect chips and the message area are not corrupted. The sun/moon icon is positioned after the corrupted text.

### 11.6 Reduced glitch effects

With "Reduce glitch effects" on: no layer and no hue rotation; no slipping slices; no periodic redraw; and the frame number is the turns-left value, so the corruption tiles and the corrupted HUD characters change exactly once per game turn and hold still in between. The gameplay side of the glitch (Part 12) is unaffected.

---

## 12. Pet and bounty in the side panel

The side panel (Part 11) shows two Wild sections, after the perks section and before the map overview, each only while its data is present. Panel colours used: white for bold titles; "dim" (150, 150, 150); "accent" (255, 190, 60); section headers are small bold dim capitals over a thin rule.

### 12.1 Pet

Source: six exported integers and a short string (Part 09): species (monster id), hit points, maximum hit points, stage, kills made by the pet's own bite, the kill count at which it next grows (0 when fully grown), and the name of the level it was left on (empty when it is with the player). There is no pet, and no section, when the species is 0 or less. While the pet is on another level its hit points are the last seen values.

| Line | Content |
|---|---|
| Header | "PET" |
| Title (bold white) | "<Stage> <species name>": the stage word is "Baby", "Young", "Grown" or "Fearsome" for stages 0–3 (values below 0 are treated as 0 and above 3 as 3); the species name is the monster's stored lower-case name from the card table, or "pet" if the id is not a card. Example "Young jackal". |
| Bar | "HP <hp>/<max>" followed by a bar filled to hp ÷ max (clamped to 0–1; empty if max is 0). Bar colour by that fraction: above 0.6 green (90, 200, 110); above 0.3 yellow (240, 190, 60); otherwise red (235, 80, 70). |
| Growth (dim) | "<kills> kills · grows at <n>" while a next growth count exists, else "<kills> kills · fully grown"; the word is "kill" when kills is exactly 1. Examples "0 kills · grows at 5", "1 kill · grows at 5", "31 kills · fully grown". |
| Whereabouts (accent; only when left behind) | "Waiting in town" if the level name is "H", otherwise "Waiting on level <name>" ("Waiting on level 3", "Waiting on level V1"). |

### 12.2 Bounty

Source: four exported integers: wanted species, reward in gold, a "claimed" flag (one has been killed and the gold waits in town), and the turn after which an unanswered poster is taken down at the next town visit (0 when claimed, or when it comes down at the next visit regardless, as after a genocide). No section when the species is 0 or less.

| Line | Content |
|---|---|
| Header | "BOUNTY" |
| Title (bold white) | "WANTED: <species name>" (stored lower-case name; "monster" if the id is not a card). Example "WANTED: kobold". |
| Reward (accent) | "Reward <gold> gold", the number with a comma between thousands regardless of the device language. Examples "Reward 125 gold", "Reward 3,700 gold". |
| Outlook | If claimed: "Done! Collect it in town", in green (120, 220, 120). Otherwise in dim: if the take-down turn is at or before the current turn (including 0), "Comes down on your next town visit"; else "Comes down in <n> turns" with n = take-down turn − current turn, comma-grouped, and "turn" when n is exactly 1. Examples "Comes down in 2,500 turns", "Comes down in 1 turn". |

---

## 13. Player-visible strings (index)

Collected for checking; the rules for each are in the sections above.

| Where | Text |
|---|---|
| Start screen | "Card album (N/69)" |
| In-game menu | "Card album", "Radio", "Take a photo" |
| Control action | "Take a photo (Wild Edition photo mode)"; accessibility label "Take a photo" |
| Settings header | "Wild features (new games start with these; a saved game keeps its own)" |
| Settings checkboxes | the twelve feature labels in section 1.1; "Card drop toasts: show each card as it drops"; "Reduce glitch effects (no shaking or colour cycling)"; "Reduce weather effects (no drifting rain or snow, no lightning flashes)"; "Auto-photo on death / victory" |
| Radio dialog | "Dungeon Radio"; "Now playing: …"; "nothing"; "(follows the dungeon)"; "(your pick)"; "Next track"; "Follow the dungeon"; "Cassette tapes"; "▶ <track>"; "???  (a tape still hidden in the dungeon)"; "Volume"; "Music: N%"; "Sound effects: N%"; "Turn the radio off in Settings → Wild features."; "Close" |
| Tape toast | "New radio track: <name> (menu → Radio)" |
| Song names | "Town", "Shallows", "Depths", "Volcano", "Shop", "Game Over", "Victory", "Elevator Music", "Surf's Up", "Boss Fight" |
| Card popup | "New card: <Title>", "Card: <Title> (xN)", with " ★ HOLO" after the title for a holo drop |
| Album dialog | "Card album"; "All rarities"; rarity labels; "All cards", "Found", "Holo", "Not found yet"; the count lines of section 7.5; "Complete set! Gotta kill 'em all - and you did."; captions "???", "xN", "★ xN"; "Close" |
| Card face | "LV<n>", "LV?", "???", "★HOLO", rarity label in capitals, "No.NN/69", "AC", "DAM", "HP", "XP", "?", "Not found yet. Go and hit things." |
| Full-screen card | "Not found yet"; "Owned: N"; " (H holo)"; "First found <date>"; "Tap to close" |
| Card accessibility | "Unknown card"; "Card <id>: not found yet"; the owned-card sentence of section 8.5 |
| Photo | "Nothing to photograph right now"; "Saved to Pictures/Ularn Wild"; "Photo (not saved to the gallery)"; "OK"; "Share"; "Share photo"; "A snapshot from Ularn Wild"; "No app to share with"; "Photo of your final moment saved to Pictures/Ularn Wild"; "Your photo" |
| Statistics | "Last photo"; "Your last photo" |
| Side panel | "PET", "BOUNTY" and the lines of section 12 |

---

## 14. Differences between the feature notes and the code

The repository's short feature notes describe intent; where they differ, the behaviour above (from the code) is what the game does.

* The notes say Boss Fight plays when a demon lord is "in sight". The code tests for any demon tile (56–63) anywhere in the exported map grid of the current level.
* The notes say the HUD shows a sun or moon after the level name. It does so only on the surface, on the map screen, for characters that have the Weather feature.
* The notes say a saved game keeps the Cards flag it was started with. The game does; but the app's collecting, popups and menu entries follow the current app setting, so both must be on.
* The effects are described as square-wave blips each under 150 ms. Several use noise or triangle segments, and the limit is at most 150 ms in total.
* Stores are detected purely by phrases on the text screen, so the "Shop" tune also covers the college, the revenue office and the dealer's pad, and anything else that prints one of the five phrases.
* The "last photo" is described as the run's; it is simply the newest photo taken by any character.
