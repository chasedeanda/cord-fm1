// SPDX-License-Identifier: GPL-3.0-only
// CORD Melody: a browser-only step editor for a melody (or bass) line. The editing rules are those of the firmware's
// cf_melody.h (a cursor on a 16th-note grid, pitch moved by scale degrees, lengths 1/2/4/8/16 steps, a note cut to fit
// the section), kept in step with it by test_melody.mjs. It has its own sound (Web Audio), saves in this browser, and
// exports a Standard MIDI File; it does not send notes to the firmware, whose MIDI input plays chords.
(() => {
  "use strict";
  const PPQ = 96, STEP_TICKS = 24, LENGTHS = [1, 2, 4, 8, 16];
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const SCALES = [
  ["Major", [0, 2, 4, 5, 7, 9, 11]],
  ["Natural Minor", [0, 2, 3, 5, 7, 8, 10]],
  ["Major Pentatonic", [0, 2, 4, 7, 9]],
  ["Minor Pentatonic", [0, 3, 5, 7, 10]],
  ["Dorian", [0, 2, 3, 5, 7, 9, 10]],
  ["Mixolydian", [0, 2, 4, 5, 7, 9, 10]],
  ["Lydian", [0, 2, 4, 6, 7, 9, 11]],
  ["Phrygian", [0, 1, 3, 5, 7, 8, 10]],
  ["Locrian", [0, 1, 3, 5, 6, 8, 10]],
  ["Harmonic Minor", [0, 2, 3, 5, 7, 8, 11]],
  ["Melodic Minor", [0, 2, 3, 5, 7, 9, 11]],
  ["Blues", [0, 3, 5, 6, 7, 10]],
  ["Major Blues", [0, 2, 3, 4, 7, 9]],
  ["Harmonic Major", [0, 2, 4, 5, 7, 8, 11]],
  ["Phrygian Dominant", [0, 1, 4, 5, 7, 8, 10]],
  ["Lydian Dominant", [0, 2, 4, 6, 7, 9, 10]],
  ["Altered", [0, 1, 3, 4, 6, 8, 10]],
  ["Whole Tone", [0, 2, 4, 6, 8, 10]],
  ["Diminished W-H", [0, 2, 3, 5, 6, 8, 9, 11]],
  ["Diminished H-W", [0, 1, 3, 4, 6, 7, 9, 10]],
  ["Hungarian Minor", [0, 2, 3, 6, 7, 8, 11]],
  ["Double Harmonic", [0, 1, 4, 5, 7, 8, 11]],
  ["Hirajoshi", [0, 2, 3, 7, 8]],
  ["In Sen", [0, 1, 5, 7, 10]],
  ["Iwato", [0, 1, 5, 6, 10]],
  ["Kumoi", [0, 2, 3, 7, 9]],
  ["Chromatic", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]]
  ];
  const noteName = (n) => NOTE[n % 12] + (Math.floor(n / 12) - 1);
  const mod = (a, n) => ((a % n) + n) % n;

  // ------------------------------------------------------------------------------------------ the rules ---
  const pcs = (scale) => SCALES[scale][1];
  // the n-th tone of the scale above (n < 0: below) the tonic, in semitones (n = size: an octave)
  const scaleStep = (scale, n) => { const p = pcs(scale), o = Math.floor(n / p.length); return p[mod(n, p.length)] + 12 * o; };
  // the nearest tone of the scale (a tie goes up)
  const snap = (scale, tonic, note) => {
    const p = pcs(scale);
    for (let d = 0; d < 12; d++) {
      if (note + d <= 127 && p.includes(mod(note + d - tonic, 12))) return note + d;
      if (note - d >= 0 && p.includes(mod(note - d - tonic, 12))) return note - d;
    }
    return note;
  };
  const create = (opts = {}) => {
    const m = { track: "melody", tonic: 0, scale: 0, bars: 2, lenI: 2, vel: 100, step: 0, note: 72, bpm: 110, notes: [], ...opts };
    m.note = snap(m.scale, m.tonic, m.note);
    return m;
  };
  const steps = (m) => m.bars * 16;
  const move = (m, d) => { m.step = mod(m.step + d, steps(m)); };
  const pitch = (m, d) => {
    let n = snap(m.scale, m.tonic, m.note);
    const rel = n - m.tonic, o = Math.floor(rel / 12), idx = pcs(m.scale).indexOf(rel - 12 * o);
    n = m.tonic + 12 * o + scaleStep(m.scale, idx + d);
    if (n >= 0 && n <= 127) m.note = n;
  };
  const length = (m, d) => { m.lenI = Math.max(0, Math.min(LENGTHS.length - 1, m.lenI + d)); };
  const fit = (m) => Math.min(LENGTHS[m.lenI], steps(m) - m.step);
  const find = (m, step, note) => m.notes.findIndex((e) => e.step === step && e.note === note);
  // SEL: 1 a note was added, 2 removed
  const toggle = (m) => {
    const at = find(m, m.step, m.note);
    if (at >= 0) { m.notes.splice(at, 1); return 2; }
    m.notes.push({ step: m.step, len: fit(m), note: m.note, vel: m.vel });
    m.notes.sort((a, b) => a.step - b.step || a.note - b.note);
    return 1;
  };
  // 0 empty, 1 a note starts here, 2 a note sustains through it
  const cell = (m, step, note) => {
    let out = 0;
    for (const e of m.notes) {
      if (e.note !== note) continue;
      if (e.step === step) return 1;
      if (e.step < step && step < e.step + e.len) out = 2;
    }
    return out;
  };
  // a changed bar count or scale keeps what still fits
  const setBars = (m, bars) => {
    m.bars = bars;
    m.notes = m.notes.filter((e) => e.step < steps(m)).map((e) => ({ ...e, len: Math.min(e.len, steps(m) - e.step) }));
    m.step = Math.min(m.step, steps(m) - 1);
  };
  const setScale = (m, tonic, scale) => {
    m.tonic = tonic; m.scale = scale;
    m.notes.forEach((e) => { e.note = snap(scale, tonic, e.note); });
    m.note = snap(scale, tonic, m.note);
  };

  // ------------------------------------------------------------------------------ Standard MIDI File (type 0) ---
  const vlq = (v) => { const b = [v & 127]; while ((v >>= 7)) b.unshift((v & 127) | 128); return b; };
  const toMidi = (m) => {
    const ev = [];
    for (const e of m.notes) {
      ev.push([(e.step + e.len) * STEP_TICKS, 0, [0x80, e.note, 0]]);   // (offs sort before ons at one tick)
      ev.push([e.step * STEP_TICKS, 1, [0x90, e.note, e.vel]]);
    }
    ev.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    const us = Math.round(60000000 / m.bpm), body = [0, 0xff, 0x51, 3, (us >> 16) & 255, (us >> 8) & 255, us & 255];
    let t = 0;
    for (const [at, , data] of ev) { body.push(...vlq(at - t), ...data); t = at; }
    body.push(...vlq(steps(m) * STEP_TICKS - t), 0xff, 0x2f, 0);
    const be = (v, n) => Array.from({ length: n }, (_, i) => (v >>> (8 * (n - 1 - i))) & 255);
    return new Uint8Array([0x4d, 0x54, 0x68, 0x64, ...be(6, 4), 0, 0, 0, 1, ...be(PPQ, 2),
                           0x4d, 0x54, 0x72, 0x6b, ...be(body.length, 4), ...body]);
  };

  const api = { PPQ, LENGTHS, SCALES, noteName, scaleStep, snap, create, steps, move, pitch, length, fit, find, toggle, cell,
                setBars, setScale, toMidi };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  globalThis.FM1Melody = api;
  if (typeof document === "undefined") return;

  // ------------------------------------------------------------------------------------------------- the page ---
  const $ = (id) => document.getElementById(id);
  const root = $("melody");
  if (!root) return;
  const KEY = "cord.melody.v1";
  let m = create(), ac = null, playing = false, t0 = 0, nextStep = 0, nextTime = 0, timer = 0, raf = 0, shownStep = -1;
  try { const s = JSON.parse(localStorage.getItem(KEY) || "null"); if (s && Array.isArray(s.notes)) m = create(s); } catch {}
  const save = () => { try { localStorage.setItem(KEY, JSON.stringify(m)); } catch {} };

  const cv = $("melCanvas"), g = cv.getContext("2d"), ROWS = 15, KEYW = 44;
  const css = (n) => getComputedStyle(root).getPropertyValue(n).trim() || "#888";
  const cur = () => snap(m.scale, m.tonic, m.note);
  // rows run in scale degrees around the view note
  let view = cur();                                              // the middle row's note: it moves only when the cursor leaves the rows
  const rowNote = (r) => {
    const p = pcs(m.scale), rel = snap(m.scale, m.tonic, view) - m.tonic, o = Math.floor(rel / 12), idx = p.indexOf(rel - 12 * o) + (Math.floor(ROWS / 2) - r);
    return m.tonic + 12 * o + scaleStep(m.scale, idx);
  };
  const draw = () => {
    const dpr = Math.min(2, devicePixelRatio || 1), w = cv.clientWidth, h = Math.round(w * 0.46);
    if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    const bg = css("--color-bg"), surf = css("--color-surface"), line = css("--color-border"), txt = css("--color-text"),
          mut = css("--color-text-muted"), acc = css("--color-accent");
    g.fillStyle = bg; g.fillRect(0, 0, w, h);
    const n = steps(m), cw = (w - KEYW) / n, rh = h / ROWS;
    g.font = "11px 'Departure Mono', ui-monospace, monospace"; g.textBaseline = "middle";
    for (let r = 0; r < ROWS; r++) {
      const nn = rowNote(r), y = r * rh, tonic = mod(nn - m.tonic, 12) === 0;
      g.fillStyle = tonic ? surf : bg; g.fillRect(KEYW, y, w - KEYW, rh);
      g.fillStyle = tonic ? acc : mut; g.fillText(nn >= 0 && nn <= 127 ? noteName(nn) : "", 6, y + rh / 2);
      g.fillStyle = line; g.fillRect(KEYW, y + rh - 1, w - KEYW, 1);
    }
    for (let s = 0; s <= n; s++) { g.fillStyle = s % 4 === 0 ? line : surf; g.fillRect(KEYW + s * cw, 0, s % 16 === 0 ? 2 : 1, h); }
    for (const e of m.notes) {
      for (let r = 0; r < ROWS; r++) if (rowNote(r) === e.note) {
        g.fillStyle = acc; g.fillRect(KEYW + e.step * cw + 1, r * rh + 2, e.len * cw - 2, rh - 4);
      }
    }
    for (let r = 0; r < ROWS; r++) if (rowNote(r) === cur()) {
      g.strokeStyle = txt; g.lineWidth = 2; g.strokeRect(KEYW + m.step * cw + 1, r * rh + 1, Math.max(cw, 6) - 2, rh - 2);
    }
    if (playing && shownStep >= 0) { g.fillStyle = txt; g.globalAlpha = 0.28; g.fillRect(KEYW + shownStep * cw, 0, cw, h); g.globalAlpha = 1; }
  };
  const refresh = () => {
    if (![...Array(ROWS).keys()].some((r) => rowNote(r) === cur())) view = cur();
    $("melStatus").textContent = `${NOTE[m.tonic]} ${SCALES[m.scale][0]}, ${m.bars} bar${m.bars > 1 ? "s" : ""}: step ${m.step + 1}/${steps(m)}, ${noteName(cur())}, length ${LENGTHS[m.lenI]}/16`;
    $("melPlay").textContent = playing ? "Stop" : "Play";
    draw();
  };
  const changed = () => { save(); refresh(); };

  // sound: a plucked triangle, no samples
  const voice = (note, vel, at, dur) => {
    const o = ac.createOscillator(), a = ac.createGain(), f = ac.createBiquadFilter(), peak = 0.18 * (vel / 127);
    o.type = "triangle"; o.frequency.value = 440 * Math.pow(2, (note - 69) / 12);
    f.type = "lowpass"; f.frequency.value = 3200;
    a.gain.setValueAtTime(0.0001, at); a.gain.exponentialRampToValueAtTime(peak, at + 0.008);
    a.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak * 0.4), at + Math.max(0.03, dur * 0.7));
    a.gain.exponentialRampToValueAtTime(0.0001, at + dur + 0.12);
    o.connect(f); f.connect(a); a.connect(ac.destination); o.start(at); o.stop(at + dur + 0.15);
  };
  const preview = () => { try { ac = ac || new AudioContext(); void ac.resume(); voice(cur(), m.vel, ac.currentTime + 0.01, 0.25); } catch {} };
  const stepSec = () => 60 / m.bpm / 4;
  const schedule = () => {
    while (nextTime < ac.currentTime + 0.12) {
      for (const e of m.notes) if (e.step === nextStep) voice(e.note, e.vel, nextTime, e.len * stepSec() * 0.92);
      nextStep = (nextStep + 1) % steps(m); nextTime += stepSec();
    }
  };
  const tick = () => {
    if (!playing) return;
    const s = Math.floor(((ac.currentTime - t0) / stepSec()) % steps(m));
    if (s !== shownStep) { shownStep = Math.max(0, s); draw(); }
    raf = requestAnimationFrame(tick);
  };
  const play = () => {
    try { ac = ac || new AudioContext(); void ac.resume(); } catch { return; }
    playing = true; nextStep = 0; nextTime = t0 = ac.currentTime + 0.05; shownStep = -1;
    timer = setInterval(schedule, 25); raf = requestAnimationFrame(tick); refresh();
  };
  const stop = () => { playing = false; clearInterval(timer); cancelAnimationFrame(raf); shownStep = -1; refresh(); };

  // controls
  const fillSel = (id, items, val) => { const s = $(id); s.innerHTML = ""; items.forEach((t, i) => s.add(new Option(t, i))); s.value = val; };
  fillSel("melKey", NOTE, m.tonic);
  fillSel("melScale", SCALES.map((s) => s[0]), m.scale);
  fillSel("melBars", ["1 bar", "2 bars", "4 bars"], Math.max(0, [1, 2, 4].indexOf(m.bars)));
  $("melBpm").value = m.bpm;
  $("melKey").onchange = $("melScale").onchange = () => { setScale(m, +$("melKey").value, +$("melScale").value); changed(); };
  $("melBars").onchange = () => { setBars(m, [1, 2, 4][+$("melBars").value]); changed(); };
  $("melBpm").onchange = () => { m.bpm = Math.max(40, Math.min(240, +$("melBpm").value || 110)); $("melBpm").value = m.bpm; changed(); };
  $("melPlay").onclick = () => (playing ? stop() : play());
  $("melClear").onclick = () => { m.notes = []; changed(); };
  $("melExport").onclick = () => {
    const url = URL.createObjectURL(new Blob([toMidi(m)], { type: "audio/midi" })), a = document.createElement("a");
    a.href = url; a.download = "cord-melody.mid"; a.click(); setTimeout(() => URL.revokeObjectURL(url), 2000);
  };
  const KEYS = {
    ArrowLeft: () => move(m, -1), ArrowRight: () => move(m, 1), ArrowUp: () => { pitch(m, 1); preview(); }, ArrowDown: () => { pitch(m, -1); preview(); },
    Enter: () => { if (toggle(m) === 1) preview(); }, " ": () => { if (toggle(m) === 1) preview(); },
    "[": () => length(m, -1), "]": () => length(m, 1),
    Backspace: () => { const i = find(m, m.step, cur()); if (i >= 0) m.notes.splice(i, 1); },
    p: () => (playing ? stop() : play()),
  };
  cv.addEventListener("keydown", (e) => { const f = KEYS[e.key]; if (!f || e.ctrlKey || e.metaKey || e.altKey) return; e.preventDefault(); f(); changed(); });
  cv.addEventListener("pointerdown", (e) => {
    const r = cv.getBoundingClientRect(), x = e.clientX - r.left - KEYW, y = e.clientY - r.top;
    if (x < 0) return;
    cv.focus();
    const row = Math.max(0, Math.min(ROWS - 1, Math.floor(y / (r.height / ROWS))));
    const note = rowNote(row);                                   // (read before the cursor moves: the rows follow it)
    m.step = Math.max(0, Math.min(steps(m) - 1, Math.floor(x / ((r.width - KEYW) / steps(m)))));
    m.note = Math.max(0, Math.min(127, note));
    if (toggle(m) === 1) preview();
    changed();
  });
  addEventListener("resize", draw);
  refresh();
})();
