/* radio.js - Dungeon Radio (Wild Edition) for the web port.
 *
 * The web counterpart of the Android app's audio/ package (Radio.kt,
 * TrackPicker.kt, Song.kt, Songs.kt, Sfx.kt): the track list in its tiny
 * text notation, the parser, the rule that picks the track from where the
 * player is, and the controller that drives the synthesizer
 * (radio-synth.js, in an AudioWorklet) from the game's events. game.js
 * calls Radio.start() when a game begins, Radio.update() on every redraw
 * (and every 250 ms), Radio.onEvent() for each game event, and
 * Radio.onGameExit() at the end; the "Radio" toolbar button opens the
 * dialog in index.html.
 *
 * Pattern notation, whitespace separated, one step = a sixteenth note:
 *   c4  f#3  bb2      a note (letter, any number of # or b, octave 0..8)
 *   c4:6              ... lasting 6 steps (default: the current length)
 *   -  or  r          a rest
 *   k  s  h           kick, snare, hi-hat (meant for the noise voice)
 *   l4                set the current length to 4 steps (no sound)
 *   [ ... ]3          play the group 3 times ("]" alone = twice); nests
 *   |                 bar line, ignored (for the reader)
 */
(function (root) {
  'use strict';

  const PULSE12 = 0, PULSE25 = 1, PULSE50 = 2, TRIANGLE = 3, NOISE = 4;
  const REST = -1, KICK = -2, SNARE = -3, HAT = -4;

  // ------------------------------------------------------------ notation
  const Notes = {
    REST, KICK, SNARE, HAT,
    freq(midi) { return 440 * Math.pow(2, (midi - 69) / 12); },
    /** "C4", "f#3", "Bb2" to a MIDI number (C4 = 60); null if it isn't a note. */
    midi(name) {
      if (name.length < 2) return null;
      const base = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 }[name[0].toLowerCase()];
      if (base === undefined) return null;
      let i = 1, acc = 0;
      while (i < name.length && (name[i] === '#' || name[i] === 'b')) { acc += name[i] === '#' ? 1 : -1; i++; }
      const oct = name.slice(i);
      if (!/^\d+$/.test(oct)) return null;
      const o = parseInt(oct, 10);
      if (o < 0 || o > 8) return null;
      return 12 * (o + 1) + base + acc;
    },
  };

  function bad(msg) { throw new Error(msg); }

  /** Splits on whitespace; '[' and ']N' are tokens even when glued to a note. */
  function tokens(text) {
    const out = [];
    let i = 0;
    while (i < text.length) {
      const ch = text[i];
      if (/\s/.test(ch)) i++;
      else if (ch === '[') { out.push('['); i++; }
      else if (ch === ']') {
        let j = i + 1;
        while (j < text.length && /\d/.test(text[j])) j++;
        out.push(text.slice(i, j)); i = j;
      } else {
        let j = i;
        while (j < text.length && !/\s/.test(text[j]) && text[j] !== '[' && text[j] !== ']') j++;
        out.push(text.slice(i, j)); i = j;
      }
    }
    return out;
  }

  function parseStep(tok, defLen) {
    const colon = tok.indexOf(':');
    const name = colon < 0 ? tok : tok.slice(0, colon);
    let len = defLen;
    if (colon >= 0) {
      const d = tok.slice(colon + 1);
      if (!/^\d+$/.test(d)) bad('bad duration in "' + tok + '"');
      len = parseInt(d, 10);
    }
    if (len < 1) bad('bad duration in "' + tok + '"');
    let note;
    if (name === '-' || name === 'r') note = REST;
    else if (name === 'k') note = KICK;
    else if (name === 's') note = SNARE;
    else if (name === 'h') note = HAT;
    else { note = Notes.midi(name); if (note === null) bad('not a note: "' + tok + '"'); }
    return [note, len];
  }

  /** Parses a pattern to a list of [note, len] steps. */
  function parsePattern(text) {
    const stack = [[]];
    let defLen = 1;
    for (const tok of tokens(text)) {
      if (tok === '|') continue;
      if (tok === '[') { stack.push([]); continue; }
      if (tok[0] === ']') {
        if (stack.length < 2) bad('unmatched \']\' in "' + text + '"');
        const times = tok.length === 1 ? 2 : parseInt(tok.slice(1), 10);
        if (!(times >= 1)) bad('bad repeat "' + tok + '"');
        const group = stack.pop();
        for (let i = 0; i < times; i++) stack[stack.length - 1].push(...group);
        continue;
      }
      if (tok.length > 1 && tok[0] === 'l' && /^\d+$/.test(tok.slice(1))) {
        defLen = parseInt(tok.slice(1), 10);
        if (defLen < 1) bad('bad length "' + tok + '"');
        continue;
      }
      stack[stack.length - 1].push(parseStep(tok, defLen));
    }
    if (stack.length !== 1) bad('unclosed \'[\' in "' + text + '"');
    return stack[0];
  }

  function patternLength(steps) { return steps.reduce((n, s) => n + s[1], 0); }

  /** Attack / decay / release in seconds, sustain as a level 0..1. */
  const Adsr = {
    PLUCK: [0.003, 0.12, 0.45, 0.06],
    LEAD: [0.005, 0.08, 0.7, 0.08],
    PAD: [0.03, 0.2, 0.6, 0.15],
    BASS: [0.002, 0.05, 0.9, 0.03],
  };

  /** A voice of a song: waveform, pattern text, volume, envelope. */
  function v(wave, pattern, volume, adsr) {
    return { wave, steps: parsePattern(pattern), volume: volume === undefined ? 1 : volume, adsr: adsr || Adsr.LEAD };
  }

  /**
   * A tune: bpm in quarter notes, each voice looping its own pattern (so a
   * one-bar drum loop can run under an eight-bar melody). A non-looping
   * song (the stingers) plays once and stops.
   */
  function song(name, bpm, voices, loop) {
    const s = { name, bpm, voices, loop: loop !== false };
    s.lengthSteps = Math.max(...voices.map((x) => patternLength(x.steps)));
    s.stepSeconds = 60 / bpm / 4;
    s.seconds = s.lengthSteps * s.stepSeconds;
    return s;
  }

  // ------------------------------------------------------------- songs
  // Dungeon Radio's track list (one bar = 16 steps). Every voice of a looping
  // song is a whole number of its loops long, so the parts stay in step
  // (test-radio.js checks it).
  const TOWN = song('Town', 132, [
    v(PULSE25, 'l2 e5 g5 c6 g5 e5 g5 c6:4 | d5 f5 a5 f5 d5 f5 a5:4 | e5 g5 c6 g5 f5 a5 c6 a5 | g5:4 b5:2 d6:2 c6:8', 0.8, Adsr.PLUCK),
    v(PULSE12, 'l4 c5 e5 c5 e5 | c5 f5 c5 f5 | c5 e5 c5 f5 | b4 d5 c5:8', 0.35, Adsr.PAD),
    v(TRIANGLE, 'l2 [c3 g3]4 | [f2 c3]4 | [c3 g3]2 [f2 c3]2 | [g2 d3]2 c3:4 g2:4', 1, Adsr.BASS),
    v(NOISE, 'k:2 h:2 s:2 h:2 k:2 k:2 s:2 h:2', 0.5),
  ]);

  const SHALLOWS = song('Shallows', 110, [
    v(PULSE50, 'l4 a4 c5 e5 d5 | c5:6 b4:2 a4:8 | f4 a4 c5 b4 | g#4:12 -:4', 0.55),
    v(PULSE12, 'l1 [a4 c5 e5 c5]8 | [f4 a4 c5 a4]4 | [e4 g#4 b4 g#4]4', 0.25, Adsr.PLUCK),
    v(TRIANGLE, 'l2 [a2 e3]8 | [f2 c3]4 | [e2 b2]4', 1, Adsr.BASS),
    v(NOISE, 'h:4 h:4 s:4 h:4', 0.35),
  ]);

  const DEPTHS = song('Depths', 80, [
    v(PULSE50, 'l8 d5 f5 | e5:4 c5:4 a4:8 | bb4 a4 | g4:12 a4:4 | d5 f5 | a5:4 g5:4 f5:8 | e5:4 f5:4 d5:8 | c#5:16', 0.5, Adsr.PAD),
    v(PULSE12, 'l4 [d4 f4 a4 f4]2 | [bb3 d4 f4 d4]2 | [d4 f4 a4 f4]2 | g3 bb3 d4 bb3 a3 c#4 e4 c#4', 0.25, Adsr.PAD),
    v(TRIANGLE, 'l16 d3 a2 bb2 g2 d3 f2 g2 a2', 1, Adsr.BASS),
    v(NOISE, 'k:8 -:4 h:4', 0.45),
  ]);

  const VOLCANO = song('Volcano', 168, [
    v(PULSE25, 'l1 [ [e5 f5 e5 bb4]2 [e5 f5 g5 f5]2 | [e5 f5 e5 bb4]2 b4:2 c5:2 bb4:4 ]2', 0.6, Adsr.PLUCK),
    v(PULSE12, 'l4 [e4 bb4 f4 b4]4', 0.3),
    v(TRIANGLE, 'l2 [e2 e2 e3 e2 f2 e2 bb2 e2]4', 1, Adsr.BASS),
    v(NOISE, 'k:2 h:1 h:1 s:2 h:1 k:1 k:2 h:2 s:2 h:2', 0.5),
  ]);

  const SHOP = song('Shop', 144, [
    v(PULSE25, 'l2 f5 - a5 f5 c6:4 a5:2 f5:2 | g5 - bb5 g5 d6:4 bb5:2 g5:2 | a5 c6 a5 f5 g5 bb5 g5 e5 | f5:4 c5:4 f5:8', 0.7, Adsr.PLUCK),
    v(TRIANGLE, 'l4 f2 c3 f2 c3 | g2 d3 c3 e3 | f2 a2 c3 c2 | f2 c3 f2:8', 1, Adsr.BASS),
    v(NOISE, 'k:4 s:4 k:2 k:2 s:4', 0.45),
  ]);

  const GAME_OVER = song('Game Over', 100, [
    v(PULSE50, 'l4 g4 f#4 f4 e4:12', 0.6),
    v(TRIANGLE, 'l4 c3 b2 bb2 a2:12', 1, Adsr.BASS),
  ], false);

  const VICTORY = song('Victory', 150, [
    v(PULSE25, 'l2 c5 e5 g5 c6 - g5 c6:12', 0.7),
    v(PULSE12, 'l12 e5 e5', 0.35, Adsr.PAD),
    v(TRIANGLE, 'l4 c3 g3 c3 c4:12', 1, Adsr.BASS),
    v(NOISE, 'l2 k h s h k h s:12', 0.4),
  ], false);

  // ---- bonus tracks, unlocked by cassette tapes ----

  const ELEVATOR = song('Elevator Music', 100, [
    v(PULSE12, 'l2 d5 - f5 d5 - c5 bb4:4 | c5 - eb5 c5 - bb4 a4:4 | bb4 d5 f5 a5 g5:4 f5:4 | eb5:4 d5:4 c5:8', 0.6),
    v(PULSE50, 'l4 - d4 - f4 | - eb4 - a4 | - g4 - g4 | - a4 - a4', 0.2, Adsr.PLUCK),
    v(TRIANGLE, 'l2 bb2 - f2 bb2 - f2 bb2 f2 | f2 - c3 f2 - c3 f2 c3 | eb2 - bb2 eb2 - bb2 eb2 bb2 | f2 - c3 f2 f2:8', 1, Adsr.BASS),
    v(NOISE, 'h:2 h:1 h:1 s:2 h:2 h:2 h:1 h:1 s:2 h:2', 0.3),
  ]);

  const SURF = song("Surf's Up", 176, [
    v(PULSE25, 'l1 [e5 e5 e5 e5 d#5 d#5 d#5 d#5]2 [g5 g5 g5 g5 f#5 f#5 f#5 f#5]2 [a5 a5 a5 a5 g5 g5 g5 g5]2 b5:8 -:8', 0.6, Adsr.PLUCK),
    v(TRIANGLE, 'l2 [e2 e2 b2 e3]4 [a2 a2 e3 a3]2 [b2 b2 f#3 b3]2', 1, Adsr.BASS),
    v(NOISE, 'l2 k h s h k k s h', 0.5),
  ]);

  const BOSS = song('Boss Fight', 150, [
    v(PULSE50, 'l2 c5 c5 eb5 c5 f#5:4 g5:4 | c5 c5 eb5 c5 b4:4 ab4:4 | c6 b5 bb5 a5 ab5 g5 f#5 f5 | g5:8 g4:8', 0.6),
    v(TRIANGLE, 'l1 [c3 c3 c4 c3]8 | [ab2 ab2 ab3 ab2]4 | [g2 g2 g3 g2]4', 1, Adsr.BASS),
    v(NOISE, 'l1 k h s h k k s h k h s h k s s s', 0.5),
  ]);

  const Songs = {
    TOWN, SHALLOWS, DEPTHS, VOLCANO, SHOP, GAME_OVER, VICTORY, ELEVATOR, SURF, BOSS,
    /** Cassette tracks, in the order of the C side's track index (compat/wild_radio.c). */
    BONUS: [ELEVATOR, SURF, BOSS],
    BONUS_ELEVATOR: 0, BONUS_SURF: 1, BONUS_BOSS: 2,
    /** Tracks the location picks (and the radio's "next track" cycles through). */
    REGULAR: [TOWN, SHALLOWS, DEPTHS, VOLCANO, SHOP],
  };
  Songs.ALL = Songs.REGULAR.concat([GAME_OVER, VICTORY], Songs.BONUS);

  // ------------------------------------------------------------- effects
  /** Square-wave one-shots, each under 150 ms. seg(wave, f0, f1, ms, vol). */
  function seg(wave, f0, f1, ms, vol) { return { wave, f0, f1, ms, vol: vol === undefined ? 1 : vol }; }
  function fx(name, segs) { return { name, segs, ms: segs.reduce((n, s) => n + s.ms, 0) }; }

  const Sfx = {
    HIT: fx('hit', [seg(NOISE, 1800, 500, 90, 0.9), seg(TRIANGLE, 110, 70, 40)]),
    KILL: fx('kill', [seg(PULSE50, 520, 130, 70), seg(NOISE, 3000, 800, 70, 0.8)]),
    TAKE: fx('take', [seg(PULSE25, 660, 660, 40), seg(PULSE25, 990, 990, 60)]),
    GOLD: fx('gold', [seg(PULSE50, 988, 988, 45, 0.8), seg(PULSE50, 1319, 1319, 90, 0.8)]),
    STAIRS_DOWN: fx('stairs down', [seg(TRIANGLE, 600, 150, 140)]),
    STAIRS_UP: fx('stairs up', [seg(TRIANGLE, 150, 600, 140)]),
    SPELL: fx('spell', [seg(PULSE12, 400, 1600, 120, 0.9)]),
    LEVEL_UP: fx('level up', [seg(PULSE25, 523, 523, 35), seg(PULSE25, 659, 659, 35), seg(PULSE25, 784, 784, 35), seg(PULSE25, 1047, 1047, 40)]),
    CASSETTE: fx('cassette', [seg(NOISE, 9000, 9000, 20, 0.6), seg(PULSE50, 784, 784, 50), seg(PULSE50, 1047, 1047, 70)]),
    CARD: fx('card', [seg(PULSE12, 1568, 1568, 40, 0.8), seg(PULSE12, 2093, 2093, 40, 0.8), seg(PULSE12, 2637, 2637, 50, 0.8)]),
    CLUNK: fx('clunk', [seg(TRIANGLE, 120, 60, 80), seg(NOISE, 1500, 500, 60, 0.7)]),
    SPLASH: fx('splash', [seg(NOISE, 4000, 1200, 140, 0.8)]),
    CHIRP: fx('chirp', [seg(PULSE25, 1200, 1800, 60, 0.7), seg(PULSE25, 1500, 2200, 60, 0.7)]),
    GLITCH: fx('glitch', [seg(NOISE, 20000, 200, 60), seg(PULSE12, 3000, 100, 80)]),
    POP: fx('pop', [seg(PULSE50, 300, 900, 60, 0.8)]),
  };
  Sfx.ALL = [Sfx.HIT, Sfx.KILL, Sfx.TAKE, Sfx.GOLD, Sfx.STAIRS_DOWN, Sfx.STAIRS_UP, Sfx.SPELL, Sfx.LEVEL_UP,
    Sfx.CASSETTE, Sfx.CARD, Sfx.CLUNK, Sfx.SPLASH, Sfx.CHIRP, Sfx.GLITCH, Sfx.POP];

  // US_EV_* in app/src/main/cpp/shared_state.h
  const EV = {
    KILL: 1, LEVEL_UP: 2, NEW_LEVEL: 3, TAKE: 4, PLAYER_HIT: 5, GOLD: 6, SPELL: 7, DIED: 8, WON: 9,
    WILD_RADIO: 100, WILD_CARD: 110, WILD_VENDING: 120, WILD_HAT: 130, WILD_COIN: 140,
    WILD_GLITCH: 150, WILD_FISHING: 160, WILD_PET: 170, WILD_PERK: 180, WILD_WEATHER: 190,
  };

  /**
   * The effect for a game event, or null for none. NEW_LEVEL is left to the
   * caller (up or down depends on the previous level); DIED / WON play a
   * stinger on the music bus instead.
   */
  Sfx.forEvent = function (type, a) {
    switch (type) {
      case EV.KILL: return Sfx.KILL;
      case EV.LEVEL_UP: return Sfx.LEVEL_UP;
      case EV.TAKE: return Sfx.TAKE;
      case EV.PLAYER_HIT: return Sfx.HIT;
      case EV.GOLD: return Sfx.GOLD;
      case EV.SPELL: return Sfx.SPELL;
      case EV.WILD_RADIO: return Sfx.CASSETTE;
      case EV.WILD_CARD: return Sfx.CARD;
      case EV.WILD_VENDING: return Sfx.CLUNK;
      case EV.WILD_HAT: return Sfx.POP;
      case EV.WILD_COIN: return a === 0 ? null : Sfx.GOLD;   // a = 0 is the silent price tick
      case EV.WILD_GLITCH: return Sfx.GLITCH;
      case EV.WILD_FISHING: return Sfx.SPLASH;
      case EV.WILD_PET: return Sfx.CHIRP;
      case EV.WILD_PERK: return Sfx.LEVEL_UP;
      default: return null;
    }
  };

  // -------------------------------------------------------- track picker
  const END_DIED = 1, END_WON = 2;
  const FISHING_MS = 20000;
  /** Phrases that only a store's text page shows (see ularn/store.c). */
  const SHOP_WORDS = ['Thrift Shoppe', 'College of Ularn', 'Trading Post', 'Revenue Service', 'McDope'];
  const BANK_WORDS = 'Bank of Ularn';

  /**
   * Chooses the track from where the player is. A scene is
   * {dlevel, textPage, shopText, bankText, bossVisible, endState}:
   * dungeon level index (0 town, 1..15 dungeon, 16..20 volcano); whether a
   * full text page shows instead of the map, and whether it is a store's or
   * the bank's; whether a demon lord is on the map; end state 0 none,
   * 1 died, 2 won, 3 quit.
   */
  class TrackPicker {
    constructor() { this.inShop = false; this.inBank = false; this.fishingUntil = 0; }
    /** A fishing event arrived at `now` ms: surf's up for a while. */
    fishing(now) { this.fishingUntil = now + FISHING_MS; }
    /** The track for scene s at time now (ms), given the unlocked tape indices; null = silence. */
    pick(s, unlocked, now) {
      // A store spreads over several pages and not all of them repeat its
      // name, so once a store page is seen it stays "the shop" until the
      // map comes back.
      if (!s.textPage) { this.inShop = false; this.inBank = false; }
      else if (s.bankText) this.inBank = true;
      else if (s.shopText) this.inShop = true;
      if (s.endState === END_DIED) return GAME_OVER;
      if (s.endState === END_WON) return VICTORY;
      if (s.endState) return null;
      if (s.bossVisible && unlocked.has(Songs.BONUS_BOSS)) return BOSS;
      if (now < this.fishingUntil && unlocked.has(Songs.BONUS_SURF)) return SURF;
      if (this.inBank && unlocked.has(Songs.BONUS_ELEVATOR)) return ELEVATOR;
      if (this.inBank || this.inShop) return SHOP;
      return TrackPicker.forLevel(s.dlevel);
    }
    static forLevel(dlevel) {
      return dlevel <= 0 ? TOWN : dlevel <= 4 ? SHALLOWS : dlevel <= 15 ? DEPTHS : VOLCANO;
    }
  }
  TrackPicker.SHOP_WORDS = SHOP_WORDS;
  TrackPicker.BANK_WORDS = BANK_WORDS;
  TrackPicker.FISHING_MS = FISHING_MS;
  TrackPicker.END_DIED = END_DIED;
  TrackPicker.END_WON = END_WON;

  // ------------------------------------------------------------ controller
  const TICK_MS = 250;

  /** The browser side: owns the AudioContext and the dialog. Only in a page. */
  const Radio = {
    Songs, Sfx, Notes, Adsr, EV, TrackPicker, parsePattern, patternLength,
    WAVES: { PULSE12, PULSE25, PULSE50, TRIANGLE, NOISE },

    // localStorage-backed settings; game.js's `store` is used when present.
    _store: null,
    get store() {
      if (!this._store) this._store = root.store || {
        get(k, d) { try { const v = localStorage.getItem('ularn.' + k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
        set(k, val) { try { localStorage.setItem('ularn.' + k, JSON.stringify(val)); } catch (e) { /* private mode */ } },
      };
      return this._store;
    },
    /** Dungeon Radio on at all (Settings -> Wild features on Android). */
    get enabled() { return !!this.store.get('radio', true); },
    set enabled(on) { this.store.set('radio', !!on); this.update(); },
    get musicVolume() { return this.store.get('radioMusic', 60); },
    set musicVolume(p) { this.store.set('radioMusic', Math.max(0, Math.min(100, p | 0))); this.applyVolumes(); },
    get sfxVolume() { return this.store.get('radioSfx', 80); },
    set sfxVolume(p) { this.store.set('radioSfx', Math.max(0, Math.min(100, p | 0))); this.applyVolumes(); },
    /** Cassette tracks found (indices into Songs.BONUS); kept across games. */
    get unlocked() {
      const bits = this.store.get('radioTapes', 0) | 0, set = new Set();
      Songs.BONUS.forEach((_, i) => { if (bits & (1 << i)) set.add(i); });
      return set;
    },
    unlock(i) {
      if (i < 0 || i >= Songs.BONUS.length) return;
      this.store.set('radioTapes', (this.store.get('radioTapes', 0) | 0) | (1 << i));
    },

    ctx: null,          // AudioContext, made on the first game start (a user gesture)
    node: null,         // AudioWorkletNode (or ScriptProcessorNode fallback)
    post: null,         // sends a control message to the mixer
    ready: false,
    picker: new TrackPicker(),
    /** The song last asked for (what the mixer plays or fades in). */
    playing: null,
    /** The player's pick in the dialog; null = follow the dungeon. */
    chosen: null,
    scene: null,        // the last scene game.js gave us
    running: false,
    lastLevel: -1,      // level of the last NEW_LEVEL event, to tell stairs up from down
    ended: 0,           // DIED / WON arrive before the game shows its final score
    timer: 0,
    onChange: null,     // game.js hook: the track changed (toolbar label)

    /**
     * Starts the audio for a game. Must run from a user gesture (the title
     * screen's button): browsers only let a page make sound after one.
     */
    start() {
      this.running = true;
      this.ended = 0;
      this.lastLevel = -1;
      this.chosen = null;
      this.picker = new TrackPicker();
      if (!this.ctx) this.open();
      else this.resume();
      clearInterval(this.timer);
      this.timer = setInterval(() => this.update(), TICK_MS);
    },

    open() {
      const AC = root.AudioContext || root.webkitAudioContext;
      if (!AC) return;
      let ctx;
      try { ctx = new AC({ latencyHint: 'interactive' }); } catch (e) { console.warn('no audio', e); return; }
      this.ctx = ctx;
      const connect = () => { this.ready = true; this.applyVolumes(); this.update(); };
      if (ctx.audioWorklet && root.AudioWorkletNode) {
        ctx.audioWorklet.addModule('radio-synth.js').then(() => {
          const node = new AudioWorkletNode(ctx, 'ularn-radio', { numberOfInputs: 0, outputChannelCount: [1] });
          node.connect(ctx.destination);
          this.node = node;
          this.post = (m) => node.port.postMessage(m);
          connect();
        }).catch((e) => { console.warn('AudioWorklet failed, using ScriptProcessor', e); this.fallback(); });
      } else this.fallback();
      if (ctx.state !== 'running') ctx.resume().catch(() => {});
    },

    /** Browsers without AudioWorklet: the mixer runs on the main thread. */
    fallback() {
      const ctx = this.ctx;
      const useSynth = (S) => {
        if (!ctx.createScriptProcessor) return;
        const mixer = new S.Mixer(ctx.sampleRate);
        const node = ctx.createScriptProcessor(2048, 0, 1);
        node.onaudioprocess = (e) => mixer.render(e.outputBuffer.getChannelData(0), e.outputBuffer.length);
        node.connect(ctx.destination);
        this.node = node;
        this.post = (m) => mixer.handle(m);
        this.ready = true; this.applyVolumes(); this.update();
      };
      if (root.UlarnSynth) useSynth(root.UlarnSynth);
      else {
        const s = document.createElement('script');
        s.src = 'radio-synth.js';
        s.onload = () => { if (root.UlarnSynth) useSynth(root.UlarnSynth); };
        document.head.appendChild(s);
      }
    },

    /** The page is hidden or shown: the radio goes quiet in the background. */
    pause() { if (this.ctx && this.ctx.state === 'running') this.ctx.suspend().catch(() => {}); },
    resume() { if (this.ctx && this.enabled && this.ctx.state !== 'running') this.ctx.resume().catch(() => {}); },

    applyVolumes() {
      if (this.post) this.post({ cmd: 'volume', music: this.musicVolume / 100, sfx: this.sfxVolume / 100 });
    },

    /** The game has exited (death, win, save): let a stinger play out, fade anything else. */
    onGameExit() {
      this.running = false;
      this.update();
    },

    /**
     * Picks the track for the scene (from game.js; see TrackPicker) and
     * plays it. With no scene, re-picks for the last one (the 250 ms tick,
     * for the fishing timeout and the dialog).
     */
    update(scene) {
      if (scene) this.scene = scene;
      if (!this.ready) return;
      let s = null;
      if (!this.enabled) { this.pause(); }
      else {
        if (this.ctx.state !== 'running' && !document.hidden) this.resume();
        if (this.running && this.scene) {
          const sc = this.scene, now = performance.now();
          if (!sc.endState && this.ended) sc.endState = this.ended;
          const auto = this.picker.pick(sc, this.unlocked, now);
          s = sc.endState ? auto : (this.chosen || auto);
        } else if (this.playing && !this.playing.loop) s = this.playing;   // the stinger
      }
      if (s !== this.playing) {
        this.playing = s;
        this.post({ cmd: 'play', song: s });
        if (this.onChange) this.onChange(s);
      }
    },

    /** A game event from the shared state's ring (US_EV_*). */
    onEvent(type, a, b, c) {
      if (!this.enabled) return;
      let fx;
      if (type === EV.NEW_LEVEL) {
        const prev = this.lastLevel;
        this.lastLevel = a;
        fx = prev < 0 || prev === a ? null : a < prev ? Sfx.STAIRS_UP : Sfx.STAIRS_DOWN;
      } else fx = Sfx.forEvent(type, a);
      switch (type) {
        case EV.DIED: this.ended = END_DIED; this.update(); break;
        case EV.WON: this.ended = END_WON; this.update(); break;
        case EV.WILD_FISHING: this.picker.fishing(performance.now()); this.update(); break;
        case EV.WILD_RADIO: {
          this.unlock(a);
          const song = Songs.BONUS[a];
          if (song) this.toast('New radio track: ' + song.name + ' (Radio button)');
          if (this.dialogOpen) this.renderDialog();
          break;
        }
      }
      if (fx && this.post && this.ctx && this.ctx.state === 'running') this.post({ cmd: 'sfx', def: fx });
    },

    toast(text) {
      const t = document.getElementById('toast');
      if (!t) return;
      t.textContent = text;
      t.hidden = false;
      t.classList.add('show');
      clearTimeout(this.toastTimer);
      this.toastTimer = setTimeout(() => { t.classList.remove('show'); t.hidden = true; }, 4000);
    },

    // --------------------------------------------------------------- dialog
    dialogOpen: false,

    /** The "Radio" toolbar button: now playing, next track, tapes, volumes. */
    showDialog() {
      const d = document.getElementById('radio-dialog');
      if (!d) return;
      this.dialogOpen = true;
      this.renderDialog();
      if (d.showModal) { if (!d.open) d.showModal(); } else { d.setAttribute('open', ''); }
    },

    closeDialog() {
      const d = document.getElementById('radio-dialog');
      this.dialogOpen = false;
      if (!d) return;
      // A slider that kept focus would take the next key the player types.
      if (d.contains(document.activeElement)) document.activeElement.blur();
      if (d.close && d.open) d.close(); else d.removeAttribute('open');
    },

    choose(s) { this.chosen = s; this.update(); this.renderDialog(); },

    nextTrack() {
      const list = [null].concat(Songs.REGULAR, [...this.unlocked].sort().map((i) => Songs.BONUS[i]));
      const i = list.indexOf(this.chosen);
      this.choose(list[(i + 1) % list.length]);
    },

    renderDialog() {
      const $ = (id) => document.getElementById(id);
      if (!$('radio-dialog')) return;
      const s = this.chosen || this.playing;
      $('radio-now').textContent = 'Now playing: ' + (s ? s.name : 'nothing');
      $('radio-how').textContent = !this.enabled ? '(the radio is off)' : this.chosen ? '(your pick)' : '(follows the dungeon)';
      $('radio-on').checked = this.enabled;
      $('radio-music').value = this.musicVolume; $('radio-music-v').textContent = this.musicVolume + '%';
      $('radio-sfx').value = this.sfxVolume; $('radio-sfx-v').textContent = this.sfxVolume + '%';
      const got = this.unlocked, tapes = $('radio-tapes');
      tapes.innerHTML = '';
      Songs.BONUS.forEach((song, i) => {
        if (got.has(i)) {
          const b = document.createElement('button');
          b.type = 'button'; b.tabIndex = -1;
          b.textContent = '▶ ' + song.name;
          b.classList.toggle('on', this.chosen === song);
          b.addEventListener('click', () => this.choose(song));
          tapes.appendChild(b);
        } else {
          const p = document.createElement('div');
          p.className = 'tape-hidden';
          p.textContent = '???  (a tape still hidden in the dungeon)';
          tapes.appendChild(p);
        }
      });
      $('radio-follow').classList.toggle('on', this.chosen === null);
      $('radio-dialog').classList.toggle('off', !this.enabled);
    },

    /** Wires the dialog's controls (once, from game.js). */
    wireDialog() {
      const $ = (id) => document.getElementById(id);
      const d = $('radio-dialog');
      if (!d) return;
      $('radio-next').addEventListener('click', () => this.nextTrack());
      $('radio-follow').addEventListener('click', () => this.choose(null));
      $('radio-close').addEventListener('click', () => this.closeDialog());
      $('radio-on').addEventListener('change', (e) => {
        this.enabled = e.target.checked;
        if (this.enabled) this.resume();
        this.renderDialog();
      });
      $('radio-music').addEventListener('input', (e) => { this.musicVolume = +e.target.value; $('radio-music-v').textContent = this.musicVolume + '%'; });
      $('radio-sfx').addEventListener('input', (e) => { this.sfxVolume = +e.target.value; $('radio-sfx-v').textContent = this.sfxVolume + '%'; });
      $('radio-sfx').addEventListener('change', () => { if (this.enabled) this.onEvent(EV.GOLD, 0, 0, 0); });
      d.addEventListener('close', () => { this.dialogOpen = false; });
      // Clicking the backdrop closes it.
      d.addEventListener('click', (e) => { if (e.target === d) this.closeDialog(); });
      // Keys typed while the dialog is open belong to it, not the game.
      d.addEventListener('keydown', (e) => { if (d.open) e.stopPropagation(); });
    },
  };

  if (typeof module === 'object' && module.exports) module.exports = Radio;
  else root.Radio = Radio;
})(typeof self !== 'undefined' ? self : this);
