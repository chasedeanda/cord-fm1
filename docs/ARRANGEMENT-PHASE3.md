# Arrangement Phase 3 — deterministic MIDI event scheduler

Phase 3 adds a bounded event scheduler for the CPJ1 arrangement model. It is
independent of the existing CORD accompaniment renderer: it produces note
actions, but does not yet call the audio engine or take ownership of transport.
This separation allows the event semantics to be tested before connecting them
to the FM-1's live scheduler.

## API

- `cf_project_player_start(player, project, tick)` starts at an absolute song tick.
- `cf_project_player_tick(player, actions, capacity)` advances exactly one
  96-PPQN tick and returns note actions for that tick.
- `cf_project_player_seek(...)` releases active notes and repositions.
- `cf_project_player_stop(...)` emits note-offs for every active voice.

The project must remain alive and unchanged while the player references it.
The scheduler has a fixed 176-voice table (88 melody + 88 bass) and a maximum
352-action output buffer. `tick` requires the full action capacity; `seek` and
`stop` require 176 slots. There is no allocation, file I/O, or variable-length
work buffer.

Actions identify melody (`CF_PROJECT_MELODY`) and bass
(`CF_PROJECT_BASS`) separately. Event probability is deterministic for a given
song tick and event index, so seeking/replaying produces repeatable results.
At a shared tick, note-offs are emitted before note-ons. Section repeats replay
the same section-relative events; section boundaries follow the Phase 2 cursor.

## Deliberate non-goals

This phase does not yet connect actions to `cfp_on`/`cfp_off`, MIDI output, or
`cpa_tick1`; it does not add device UI, flash persistence, SMF export, or
real-time clock ownership. Those require a separate integration pass and
hardware timing validation. Existing CSG1/CSL1 records and flash allocation are
unchanged.
