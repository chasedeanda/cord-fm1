# CORD Chord Studio — Phase 1 project model

This phase adds a versioned, multi-section project model without changing the
existing `cf_song_t` / `CSG1` phrase format or the six saved-idea records.

## Model

- `cf_project_t` is the composition container.
- A project has up to six named sections.
- Each section embeds one existing `cf_song_t` phrase, preserving the existing
  chord-slot, key, scale, meter, tempo, rhythm and voicing semantics.
- Melody and bass are independent event arrays. Events are section-relative;
  the shared playback clock can later concatenate sections and apply repeats.
- Each section references a drum pattern ID, `CF_PROJECT_DRUM_FOLLOW`, or
  `CF_PROJECT_DRUM_OFF`. Existing drum playback remains the implementation.
- Event probability is 0–100. Event flags currently reserve accent, tie,
  legato and chromatic markers.
- `cf_project_section_ticks` and `cf_project_total_ticks` derive the arranged
  duration from each embedded phrase and its section repeat count. Events are
  rejected if they extend beyond their section's phrase duration.

## CPJ1 wire format

`cf_project_encode` / `cf_project_decode` implement CPJ1 version 1. The record
is variable-length, little-endian, checksummed, and strictly bounded.

| Limit | Value |
|---|---:|
| Sections | 6 |
| Melody events | 88 |
| Bass events | 88 |
| Section phrase | Existing 344-byte CSG1 record |
| Section header | 20 bytes |
| Event record | 9 bytes |
| Maximum encoded size | 3,808 bytes |
| One-object payload budget | 3,840 bytes |

The six-section limit and 88-event-per-track limit are intentional: the
maximum CPJ1 record fits one existing 4 KiB flash object payload, with 32 bytes
of headroom. No new flash object ID or saved-song migration is allocated in
this phase. The existing `CSL1` saved-idea records remain unchanged.

Event records encode a 24-bit section-relative start tick, 16-bit duration,
note, velocity, probability, and a packed nibble for section index / flags.
This supports long sections without expanding the event record.

## Memory and real-time constraints

The host `sizeof` measurements for this revision are:

- `sizeof(cf_song_t)`: 360 bytes
- `sizeof(cf_project_section_t)`: 380 bytes
- `sizeof(cf_project_event_t)`: 12 bytes
- `sizeof(cf_project_t)`: 4,420 bytes

The project object must not be placed in ordinary `.bss`: the checked-in
resource audit reports only 1,528 bytes of ordinary-RAM headroom on its
measured baseline. When a project is integrated into the firmware, place the
single working project in the existing `.pool` region and account for any
serialization scratch buffer against the measured POOL headroom. The project
model does not allocate memory, write flash, or schedule audio from an ISR.

`cf_project_init` and `cf_project_decode` use only a `cf_song_t`-sized local
scratch object; they do not place a whole `cf_project_t` on the stack. Decode
preflights the complete record before changing the caller's project.

## Compatibility

- `CSG1` remains version 1 and 344 bytes.
- Existing six `CSL1` saved ideas are untouched.
- Existing draft/checkpoint and backup IDs are untouched.
- CPJ1 has a separate magic and version; unsupported or corrupt records are
  rejected without modifying the destination project.
- No flash object is claimed by Phase 1. A later storage phase must choose a
  safe object / slot policy and add backup, recovery, interrupted-write and
  migration tests before enabling device persistence.

## Host verification

Run:

```sh
sh tools/test_project.sh
sh tools/test_song.sh
```

The project suite tests section insertion/removal, event ownership and
renumbering, capacity failure atomicity, maximum-size encoding, round trips,
short buffers, and corruption rejection.
