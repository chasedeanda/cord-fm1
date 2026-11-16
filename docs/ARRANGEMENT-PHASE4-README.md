# CORD Chord Studio — Phase 4

This increment connects the Phase 3 melody/bass event scheduler to the existing accompaniment clock and note renderer.

See [`ARRANGEMENT-PHASE4.md`](ARRANGEMENT-PHASE4.md) for API, ownership, timing, routing, and limitations.

## Changed files

- `firmware/src/cf_project.h` — visitor scheduler and live bridge API declarations.
- `firmware/src/cf_project.c` — callback-based scheduler and stop visitor (no large ISR action array).
- `firmware/src/cf_accomp.c` — shared-clock bridge into `cfp_on` / `cfp_off`.
- `tests/cf_project_test.c` — callback ordering, note-off, and stop tests.

## Validation

- Project suite: 43,626 checks, 0 failures.
- ASan + UBSan project suite: passed.
- Song suite: 62,449 checks, 0 failures.
- Songs library suite: 2,136 checks, 0 failures.
- Storage log suite: 54,660 checks, 0 failures.
- Native editor suite: 3,144 checks, 0 failures.
- Editor storage log suite: 10,690 checks, 0 failures.
- Accompaniment workspace: 730 checks, 0 failures.
- Progression library: 80,519 checks, 0 failures.
- Progression suggestions: 933 checks, 0 failures.
- Headless firmware emulator: passed after generating the repository's generated headers.

A target FM-1 hardware build and device test have not been performed. The bridge is an API, not yet connected to an on-device arrangement UI. It schedules independent melody/bass events only; automatic section phrase/drum-plan switching, flash persistence, and SMF export remain future work.
