// SPDX-License-Identifier: GPL-3.0-only
// The browser Melody editor against the firmware's rules (tests/cf_project_test.c melody_editor) and its scale table.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
const M = createRequire(import.meta.url)('./melody.js');

// the scale table is the firmware's (cf_theory.c), tone for tone
const c = readFileSync(new URL('../../../firmware/src/cf_theory.c', import.meta.url), 'utf8');
const rows = [...c.matchAll(/\{"([^"]+)", "[^"]+", (M\d?\(([^)]*)\)|0x0FFFu)\}/g)];
assert.equal(rows.length, M.SCALES.length);
rows.forEach((r, i) => {
  assert.equal(M.SCALES[i][0], r[1]);
  assert.deepEqual(M.SCALES[i][1], r[3] ? r[3].split(',').map(Number) : [...Array(12).keys()], r[1]);
});

// cursor, pitch by scale degrees (C major: C D E F G A B)
const m = M.create({ scale: 0, tonic: 0 });
assert.equal(M.steps(m), 32); assert.equal(m.note, 72);
M.pitch(m, 1); assert.equal(m.note, 74); M.pitch(m, -3); assert.equal(m.note, 69);
M.pitch(m, 7); assert.equal(m.note, 81); m.note = 72;
M.move(m, -1); assert.equal(m.step, 31); M.move(m, 1); assert.equal(m.step, 0);
M.length(m, -9); assert.equal(m.lenI, 0); M.length(m, 9); assert.equal(m.lenI, 4); M.length(m, -2); assert.equal(m.lenI, 2);

// toggle, cells, a note cut to the section's end
assert.equal(M.toggle(m), 1); assert.equal(m.notes.length, 1);
assert.equal(M.cell(m, 0, 72), 1); assert.equal(M.cell(m, 3, 72), 2); assert.equal(M.cell(m, 4, 72), 0); assert.equal(M.cell(m, 0, 74), 0);
M.move(m, 30); assert.equal(M.fit(m), 2);
assert.equal(M.toggle(m), 1); assert.equal(m.notes[1].len, 2);
m.step = 0; assert.equal(M.toggle(m), 2); assert.equal(m.notes.length, 1);

// off-scale notes snap; the cursor never leaves 0..127
const a = M.create({ scale: 11, tonic: 9 });                          // A blues
assert.ok(M.SCALES[11][1].includes(((a.note - 9) % 12 + 12) % 12));
for (let i = 0; i < 300; i++) M.pitch(a, -1); assert.ok(a.note >= 0);
for (let i = 0; i < 300; i++) M.pitch(a, 1); assert.ok(a.note <= 127);
assert.equal(M.snap(0, 0, 61), 62);                                    // C# in C major: a tie goes up

// fewer bars drops and trims; a new scale re-snaps what is there
const b = M.create({ bars: 2 }); b.step = 20; M.toggle(b); b.step = 0; M.toggle(b);
M.setBars(b, 1); assert.equal(b.notes.length, 1); assert.equal(b.step, 0);
M.setScale(b, 0, 2); assert.ok([0, 2, 4, 7, 9].includes(b.notes[0].note % 12));

// Standard MIDI File: header, 96 PPQ, tempo, one note on/off pair, end of track
const e = M.create({ bpm: 120 }); M.toggle(e);                        // C5, 4 steps = 96 ticks
const f = M.toMidi(e), dv = new DataView(f.buffer);
assert.equal(String.fromCharCode(...f.slice(0, 4)), 'MThd'); assert.equal(dv.getUint16(12), 96);
assert.equal(String.fromCharCode(...f.slice(14, 18)), 'MTrk'); assert.equal(dv.getUint32(18), f.length - 22);
assert.deepEqual([...f.slice(22, 29)], [0, 0xff, 0x51, 3, 0x07, 0xa1, 0x20]);   // 500000 us per quarter at 120
assert.deepEqual([...f.slice(29, 33)], [0, 0x90, 72, 100]);
assert.deepEqual([...f.slice(33, 37)], [96, 0x80, 72, 0]);
assert.deepEqual([...f.slice(-3)], [0xff, 0x2f, 0]);
console.log('PASS: 27 scales match the firmware, cursor/pitch/length/toggle/fit rules, rescale, SMF export');
