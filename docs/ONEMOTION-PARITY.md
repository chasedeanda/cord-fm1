# OneMotion Chord Player parity: phase 9 (cumulative; replaces phases 5 to 8)

Compiled and run through the host firmware harness (generate headers first: `python3 -c 'import sys; sys.path.insert(0,"tools"); import build; build.generate()'`).
Passing: test_song, test_project, test_accomp (plan stays 348 bytes), test_editor, test_perform, test_arp, test_home_banks, test_library, test_stage, cf_chord_test.
test_keyboard shows the same 16 label-width failures as the untouched zip. Not run on hardware.

| Feature | Where | Notes |
| --- | --- | --- |
| Slot type: Play / Chord only / Bass only / Silence / Break | Chord editor, Bass tab, K3 `SLOT TYPE` | Stored in the slot's `bass_off` byte (bit0 bass off, bit1 chord off, bit2 drums off); in the accompaniment plan these ride in spare bits of `tie` so the plan stays 348 bytes. Wire size unchanged; older firmware rejects the new values. Break drops the drums at the next bar line via HOME's DROP and brings them back on the next non-Break slot. |
| Bass pedal point | Bass page, K4 `EXTRA`: Off / Octaver / Pedal I / V / IV | Octaver and pedal are exclusive (one saved byte). Pedal sits in a fixed C2..B2 register shifted by the bass octave. |
| Tones 2/3/4/All, Pair (octave, 3rd, 4th, 5th, 6th, 10th) | Harmony page (GLO), K3 / K4 | Session-only. Tones limits chord size; Pair adds a scale-aware second voice above every sounding note (also single notes). |
| Pentatonic / Blues key sets | Already available: HOME Scale | Pick Major/Minor Pentatonic or Blues; Auto harmony needs a 7-note scale and falls back to manual. |
| Melody tab logic | `cf_melody.h` (cursor, scale-degree pitch, length 1/2/4/8/16 steps, toggle, `cf_melody_cell` for drawing), `cf_project_toggle_note` | Logic + host tests only. No screen: the firmware keeps no project in RAM (a `cf_project_t` is 4,420 B + 1,432 B player), no UI calls `cf_project_live_start`, and there is no persistence, so the screen needs that RAM/storage decided first; playback already exists (arrangement phase 4). Works for the Bass track too. |
| Melody editor in the browser | `tools/emu/web/melody.js`, a Melody panel in `tools/emu/web/index.html` (the /play/ page) | Piano-roll grid (click or arrow keys), key/scale (27 scales from the firmware's table), 1/2/4 bars, tempo, own Web Audio sound, saved in the browser, **Save .mid** (Standard MIDI File, 96 PPQ). Same editing rules as `cf_melody.h`; `node tools/emu/web/test_melody.mjs`. It does not send notes to the firmware (its MIDI input plays chords). Needs no wasm rebuild. |
| Warm Burnout Dark | `CF_SKIN_RGB[0]` (Devil skin), `web/site.css` dark scopes, emulator `theme-color` | bg #1a1510, ink #bfbdb6, muted #b4a89c, accent #ffb454, on-accent #141210. The pixel-art mascot images and `favicon.svg` are raster-style art and were not recoloured. The Cat (plum) skin is unchanged. |
