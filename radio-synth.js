/* radio-synth.js - Dungeon Radio's synthesizer for the web port.
 *
 * A straight port of the Android app's audio/Synth.kt: NES-style
 * oscillators (pulse with a duty cycle, 16-step triangle, LFSR noise), a
 * linear ADSR envelope, a sequencer that plays a song voice by voice, a
 * pitch-slide sound-effect voice and a mixer that cross-fades songs and
 * adds up to four effects. Nothing is pre-recorded: it all renders as it
 * plays.
 *
 * This one file runs in three places:
 *  - as an AudioWorklet module (the normal case): registers the
 *    "ularn-radio" processor, driven by messages from radio.js;
 *  - as a plain <script> in the page, for the ScriptProcessorNode fallback
 *    on browsers without AudioWorklet (exposed as window.UlarnSynth);
 *  - in Node, for test-radio.js (module.exports).
 *
 * Songs arrive already parsed (radio.js has the notation parser):
 *   {name, bpm, loop, voices: [{wave, volume, adsr: [a, d, s, r], steps: [[note, len], ...]}]}
 * wave is one of the WAVE_* numbers; note is a MIDI number or REST / KICK /
 * SNARE / HAT; len is in steps (sixteenth notes).
 * Effects: {name, segs: [{wave, f0, f1, ms, vol}]}.
 */
(function (root) {
  'use strict';

  const WAVE_PULSE12 = 0, WAVE_PULSE25 = 1, WAVE_PULSE50 = 2, WAVE_TRIANGLE = 3, WAVE_NOISE = 4;
  const REST = -1, KICK = -2, SNARE = -3, HAT = -4;

  /** Equal temperament, A4 (MIDI 69) = 440 Hz. */
  function noteFreq(midi) { return 440 * Math.pow(2, (midi - 69) / 12); }

  /** One oscillator: pulse with a duty cycle, 16-step triangle, or LFSR noise. */
  class Osc {
    constructor(sampleRate) {
      this.sampleRate = sampleRate;
      this.wave = WAVE_PULSE50;
      this.freq = 440;
      this.phase = 0;
      this.lfsr = 1;
      this.noiseOut = 1;
    }
    reset() { this.phase = 0; }
    next() {
      const inc = this.freq / this.sampleRate;
      switch (this.wave) {
        case WAVE_NOISE: {
          // freq = how often the shift register clocks, like the NES noise channel.
          this.phase += inc;
          while (this.phase >= 1) {
            this.phase -= 1;
            const bit = (this.lfsr ^ (this.lfsr >> 1)) & 1;
            this.lfsr = (this.lfsr >> 1) | (bit << 14);
            this.noiseOut = (this.lfsr & 1) ? 1 : -1;
          }
          return this.noiseOut;
        }
        case WAVE_TRIANGLE: {
          this.phase = (this.phase + inc) % 1;
          const t = this.phase < 0.5 ? 4 * this.phase - 1 : 3 - 4 * this.phase;
          // 16 levels, the NES triangle's staircase.
          return Math.max(-1, Math.min(1, Math.floor(t * 8) / 8));
        }
        default: {
          this.phase = (this.phase + inc) % 1;
          const duty = this.wave === WAVE_PULSE12 ? 0.125 : this.wave === WAVE_PULSE25 ? 0.25 : 0.5;
          return this.phase < duty ? 1 : -1;
        }
      }
    }
  }

  const ENV_OFF = 0, ENV_ATTACK = 1, ENV_DECAY = 2, ENV_SUSTAIN = 3, ENV_RELEASE = 4;

  /** Linear ADSR envelope; adsr = [attack s, decay s, sustain level, release s]. */
  class Envelope {
    constructor(sampleRate) {
      this.sampleRate = sampleRate;
      this.stage = ENV_OFF;
      this.level = 0;
      this.adsr = [0.005, 0.08, 0.7, 0.08];
    }
    get idle() { return this.stage === ENV_OFF; }
    on(adsr) { this.adsr = adsr; this.stage = ENV_ATTACK; }
    off() { if (this.stage !== ENV_OFF) this.stage = ENV_RELEASE; }
    kill() { this.stage = ENV_OFF; this.level = 0; }
    next() {
      const [a, d, s, r] = this.adsr, sr = this.sampleRate;
      switch (this.stage) {
        case ENV_ATTACK:
          this.level += 1 / Math.max(1, a * sr);
          if (this.level >= 1) { this.level = 1; this.stage = ENV_DECAY; }
          break;
        case ENV_DECAY:
          this.level -= (1 - s) / Math.max(1, d * sr);
          if (this.level <= s) { this.level = s; this.stage = s <= 0 ? ENV_OFF : ENV_SUSTAIN; }
          break;
        case ENV_RELEASE:
          this.level -= 1 / Math.max(1, r * sr);
          if (this.level <= 0) { this.level = 0; this.stage = ENV_OFF; }
          break;
      }
      return this.level;
    }
  }

  const DRUM_KICK = [0.001, 0.12, 0, 0.02];
  const DRUM_SNARE = [0.001, 0.10, 0, 0.02];
  const DRUM_HAT = [0.001, 0.03, 0, 0.01];

  /** One voice of a song in a Sequencer. */
  class Channel {
    constructor(voice, song, sampleRate) {
      this.voice = voice;
      this.song = song;
      this.sampleRate = sampleRate;
      this.stepSamples = sampleRate * (60 / song.bpm / 4);
      this.osc = new Osc(sampleRate);
      this.osc.wave = voice.wave;
      this.env = new Envelope(sampleRate);
      this.idx = 0;
      this.nextAt = 0;
      this.gateOffAt = -1;
      this.done = voice.steps.length === 0;
      /** Per-sample pitch multiplier (the kick's falling pitch). */
      this.sweep = 1;
    }
    next(pos) {
      if (!this.done && pos >= this.nextAt) this.trigger();
      if (this.gateOffAt >= 0 && pos >= this.gateOffAt) { this.env.off(); this.gateOffAt = -1; }
      if (this.env.idle) return 0;
      if (this.sweep !== 1) this.osc.freq = Math.max(30, this.osc.freq * this.sweep);
      return this.osc.next() * this.env.next() * this.voice.volume;
    }
    trigger() {
      const steps = this.voice.steps;
      if (this.idx >= steps.length) {
        if (!this.song.loop) { this.done = true; this.env.off(); return; }
        this.idx = 0;
      }
      const st = steps[this.idx++], note = st[0], len = st[1];
      const start = this.nextAt;
      this.nextAt += len * this.stepSamples;
      this.sweep = 1;
      switch (note) {
        case REST: this.env.off(); this.gateOffAt = -1; return;
        case KICK:
          this.osc.wave = WAVE_TRIANGLE; this.osc.freq = 180;
          this.sweep = Math.pow(0.25, 1 / (0.08 * this.sampleRate));   // two octaves down in 80 ms
          this.env.on(DRUM_KICK);
          break;
        case SNARE: this.osc.wave = WAVE_NOISE; this.osc.freq = 7000; this.env.on(DRUM_SNARE); break;
        case HAT: this.osc.wave = WAVE_NOISE; this.osc.freq = 16000; this.env.on(DRUM_HAT); break;
        default:
          this.osc.wave = this.voice.wave;
          // A pitched note on the noise voice: higher note, faster clock.
          this.osc.freq = noteFreq(note) * (this.voice.wave === WAVE_NOISE ? 8 : 1);
          this.env.on(this.voice.adsr);
      }
      // Leave a little air between notes so repeated notes are heard.
      this.gateOffAt = start + len * this.stepSamples * 0.85;
    }
  }

  /** Plays a song voice by voice; render() adds into a buffer. */
  class Sequencer {
    constructor(song, sampleRate) {
      this.song = song;
      this.channels = song.voices.map((v) => new Channel(v, song, sampleRate));
      this.pos = 0;
    }
    /** A non-looping song has played to the end (and its notes have died away). */
    get finished() { return !this.song.loop && this.channels.every((c) => c.done && c.env.idle); }
    render(out, n, gain) {
      const ch = this.channels;
      for (let i = 0; i < n; i++) {
        let s = 0;
        for (let k = 0; k < ch.length; k++) s += ch[k].next(this.pos);
        out[i] += s * gain;
        this.pos++;
      }
    }
  }

  /** Plays one sound effect: a few segments, each a pitch slide on one waveform. */
  class SfxVoice {
    constructor(sampleRate) {
      this.sampleRate = sampleRate;
      this.osc = new Osc(sampleRate);
      this.def = null;
      this.seg = 0;
      this.t = 0;
      this.segLen = 0;
      /** Monotonic start counter, to steal the oldest voice. */
      this.started = 0;
    }
    get busy() { return this.def !== null; }
    start(def, order) { this.def = def; this.seg = 0; this.t = 0; this.started = order; this.begin(); }
    begin() {
      const d = this.def;
      if (!d) return;
      if (this.seg >= d.segs.length) { this.def = null; return; }
      const s = d.segs[this.seg];
      this.osc.wave = s.wave;
      this.osc.reset();
      this.segLen = Math.max(1, Math.floor(s.ms * this.sampleRate / 1000));
      this.t = 0;
    }
    next() {
      const d = this.def;
      if (!d) return 0;
      const s = d.segs[this.seg];
      const x = this.t / this.segLen;
      this.osc.freq = s.f0 * Math.pow(s.f1 / s.f0, x);
      // Fall to 40% over the segment, with 3 ms fades at both ends against clicks.
      const edge = Math.max(1, Math.floor(3 * this.sampleRate / 1000));
      const fade = Math.min(1, this.t / edge, (this.segLen - this.t) / edge);
      const out = this.osc.next() * (s.vol === undefined ? 1 : s.vol) * (1 - 0.6 * x) * fade;
      if (++this.t >= this.segLen) { this.seg++; this.begin(); }
      return out;
    }
  }

  const POLYPHONY = 4;
  const FADE_SECONDS = 0.3;
  /** Head-room: four music voices or four effects at full tilt stay below 1. */
  const MUSIC_LEVEL = 0.22;
  const SFX_LEVEL = 0.3;

  /** Music (with a cross-fade between songs) plus up to POLYPHONY sound effects. */
  class Mixer {
    constructor(sampleRate) {
      this.sampleRate = sampleRate;
      this.musicVolume = 0.6;
      this.sfxVolume = 0.8;
      this.current = null;
      this.fading = null;
      this.fadePos = 0;
      this.fadeLen = Math.floor(sampleRate * FADE_SECONDS);
      this.voices = [];
      for (let i = 0; i < POLYPHONY; i++) this.voices.push(new SfxVoice(sampleRate));
      this.sfxOrder = 0;
      this.scratch = new Float32Array(0);
    }
    /** The song playing (or fading in), null for silence. */
    get song() { return this.current ? this.current.song : null; }
    /** Nothing is playing or about to. */
    get silent() {
      return (this.current === null || this.current.finished) && this.fading === null && !this.voices.some((v) => v.busy);
    }
    /** Cross-fades to song (null = fade out). Asking for the song already playing does nothing. */
    play(song) {
      if (song === this.song || (song && this.song && song.name === this.song.name)) return;
      this.fading = this.current;
      this.current = song ? new Sequencer(song, this.sampleRate) : null;
      this.fadePos = 0;
    }
    sfx(def) {
      let v = this.voices.find((x) => !x.busy);
      if (!v) v = this.voices.reduce((a, b) => (a.started <= b.started ? a : b));
      v.start(def, ++this.sfxOrder);
    }
    /** Fills out[0 .. n) with the mix, in -1..1. */
    render(out, n) {
      if (this.scratch.length < n) this.scratch = new Float32Array(n);
      const music = this.scratch;
      music.fill(0, 0, n);
      let inGain, outGain;
      if (this.fadePos < this.fadeLen) {
        // One gain per buffer is smooth enough at a few ms per buffer.
        const f = this.fadePos / this.fadeLen;
        inGain = f; outGain = 1 - f;
        this.fadePos += n;
      } else {
        inGain = 1; outGain = 0;
        this.fading = null;
      }
      if (this.current) this.current.render(music, n, inGain);
      if (this.fading) this.fading.render(music, n, outGain);
      const mv = MUSIC_LEVEL * this.musicVolume, sv = SFX_LEVEL * this.sfxVolume, voices = this.voices;
      for (let i = 0; i < n; i++) {
        let s = music[i] * mv;
        let e = 0;
        for (let k = 0; k < voices.length; k++) if (voices[k].busy) e += voices[k].next();
        s += e * sv;
        out[i] = s > 1 ? 1 : s < -1 ? -1 : s;
      }
    }
    /** A control message from radio.js (shared by the worklet and the fallback). */
    handle(m) {
      switch (m.cmd) {
        case 'play': this.play(m.song || null); break;
        case 'sfx': this.sfx(m.def); break;
        case 'volume':
          if (m.music !== undefined) this.musicVolume = m.music;
          if (m.sfx !== undefined) this.sfxVolume = m.sfx;
          break;
      }
    }
  }

  const api = {
    WAVE_PULSE12, WAVE_PULSE25, WAVE_PULSE50, WAVE_TRIANGLE, WAVE_NOISE,
    REST, KICK, SNARE, HAT, POLYPHONY, MUSIC_LEVEL, SFX_LEVEL,
    noteFreq, Osc, Envelope, Sequencer, SfxVoice, Mixer,
  };

  if (typeof registerProcessor === 'function' && typeof AudioWorkletProcessor === 'function') {
    // AudioWorklet global scope: `sampleRate` is the context's rate.
    class UlarnRadioProcessor extends AudioWorkletProcessor {
      constructor() {
        super();
        this.mixer = new Mixer(sampleRate);
        this.port.onmessage = (e) => this.mixer.handle(e.data);
      }
      process(inputs, outputs) {
        const out = outputs[0];
        if (out.length) {
          this.mixer.render(out[0], out[0].length);
          for (let c = 1; c < out.length; c++) out[c].set(out[0]);
        }
        return true;
      }
    }
    registerProcessor('ularn-radio', UlarnRadioProcessor);
  } else if (typeof module === 'object' && module.exports) {
    module.exports = api;
  } else {
    root.UlarnSynth = api;
  }
})(typeof self !== 'undefined' ? self : this);
