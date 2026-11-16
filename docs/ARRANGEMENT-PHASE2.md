# CORD Chord Studio — Phase 2 arrangement timeline

Phase 2 adds a deterministic playback cursor over the Phase 1 multi-section
project model. It deliberately does not yet replace or modify CORD's live
`cf_accomp.c` scheduler.

## Playback cursor

`cf_project_cursor_t` tracks:

- absolute song tick (`tick`)
- zero-based section index (`section`)
- zero-based repeat occurrence within that section (`repeat`)
- tick within the current section occurrence (`section_tick`)
- terminal state (`ended`)

The valid terminal position is exactly `cf_project_total_ticks(project)`. Seeking
past the end fails; advancing past the end saturates at the terminal position.

## API

- `cf_project_playback_seek(project, cursor, tick)` maps an absolute tick to its
  section, repeat occurrence, and local tick.
- `cf_project_playback_advance(project, cursor, ticks)` advances by a bounded
  number of ticks and saturates at the end.
- `cf_project_playback_events(project, cursor, track, out, capacity)` returns
  pointers to events whose `start_tick` equals the cursor's section-local tick.
  The returned pointers refer to the project and must not be retained across
  edits. The function returns the number of pointers copied, capped by capacity.

The timeline is deterministic and has no allocation, storage, audio, or ISR
side effects. It revalidates cursor position before event lookup, rejecting
forged or stale section/repeat/local-tick fields.

## Integration boundary

The existing accompaniment scheduler in `cf_accomp.c` owns a shared 96-PPQN
clock, chord onset and gate handling, drum groove scheduling, MIDI clock
handling, and live-edit mailboxes. Phase 2 does not inject melody/bass events
into that scheduler yet. Doing so safely requires bounded note-on/note-off
queues, ownership rules for the existing chord/bass voices, transport stop/pause
semantics, and a UI snapshot handoff. Those are deferred rather than guessed.

Project persistence is also still deferred. No flash object ID is allocated;
CSG1 and CSL1 remain unchanged.

## Verification

`sh tools/test_project.sh` includes section/repeat boundary, event onset,
seek, saturation, terminal state, and invalid-cursor tests. Existing song,
library, editor, and accompaniment workspace suites remain independent and
are rerun alongside the project suite.
