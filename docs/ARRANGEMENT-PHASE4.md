# Arrangement Phase 4 — live accompaniment-clock bridge

Phase 4 connects the arrangement event scheduler to CoralFlower's existing audio/MIDI note path without creating another clock.

## Runtime API

`cf_project.h` declares:

- `cf_project_live_start(player, project, tick)` — starts the project event tracks at an absolute arrangement tick.
- `cf_project_live_stop()` — releases every active arrangement note and detaches the player.
- `cf_project_live_playing()` — reports whether the attached arrangement player is still running.

The live bridge is implemented in `cf_accomp.c`. Start requires the existing catalogue accompaniment transport to be running. Each `cpa_tick1()` call advances the project player by one 96-PPQN tick, so arrangement events share the accompaniment's internal clock, tempo, MIDI-clock input, and drift behavior. Melody events are sent to the existing LAYER part (part 2); bass events use the existing BASS part (part 1). Both go through `cfp_on` / `cfp_off`, so the current engine voice and configured MIDI-note output paths are used.

The bridge stops and releases its notes when the accompaniment releases/stops, including external-clock loss and transport stop. Natural project completion also releases active notes and detaches the player.

## Ownership and memory

The caller owns the `cf_project_t` and `cf_project_player_t`. Both must remain valid while live playback is active, and the project must not be edited during playback. The caller should place the long-lived objects in the firmware's `.pool` storage; do not add a global `.bss` project/player instance. On the host ABI used by tests, `sizeof(cf_project_t)` is 4,420 bytes and `sizeof(cf_project_player_t)` is 1,432 bytes. Target ABI sizes should be confirmed by the target build.

The visitor scheduler (`cf_project_player_tick_visit`) emits each note action immediately through a callback. It avoids allocating the 1,408-byte maximum action array on the audio ISR stack. The original buffered scheduler remains available for host tests and offline consumers.

## Current limits

- This is a firmware API bridge, not an on-device arrangement editor or transport screen; no UI calls `cf_project_live_start` yet.
- The project's chord phrases and drum-pattern references are still semantic arrangement data. This phase schedules only the independent melody and bass event tracks; it does not yet swap the catalogue chord plan or drum groove at section boundaries.
- The project bass track shares the existing BASS engine part with CORD's accompaniment bass. They therefore share the part's sound and MIDI channel; a future mixer/track ownership pass should make that relationship explicit.
- Flash persistence and Standard MIDI File export remain future phases.
