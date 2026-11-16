/* SPDX-License-Identifier: GPL-3.0-only
 * Versioned multi-section project model. Existing CSG1 phrases remain unchanged.
 */
#ifndef CF_PROJECT_H
#define CF_PROJECT_H
#include "cf_song.h"

#define CF_PROJECT_MAX_SECTIONS 6u
#define CF_PROJECT_MAX_EVENTS 88u
#define CF_PROJECT_NAME 24u
#define CF_PROJECT_SECTION_NAME 16u
#define CF_PROJECT_EVENT_WIRE 9u
#define CF_PROJECT_HEADER_WIRE 36u
#define CF_PROJECT_SECTION_WIRE (CF_PROJECT_SECTION_NAME + 4u + CF_SONG_WIRE)
#define CF_PROJECT_WIRE_MAX (CF_PROJECT_HEADER_WIRE + CF_PROJECT_MAX_SECTIONS * CF_PROJECT_SECTION_WIRE + \
                             2u * CF_PROJECT_MAX_EVENTS * CF_PROJECT_EVENT_WIRE + 4u)
#define CF_PROJECT_STORAGE_PAYLOAD_MAX 3840u
#if CF_PROJECT_WIRE_MAX > CF_PROJECT_STORAGE_PAYLOAD_MAX
#error "CPJ1 project exceeds one 4 KiB flash object's payload"
#endif
#define CF_PROJECT_DRUM_FOLLOW 65535u
#define CF_PROJECT_DRUM_OFF 65534u

enum { CF_PROJECT_MELODY = 0, CF_PROJECT_BASS = 1 };
enum { CF_PROJECT_EVENT_ACCENT = 1u, CF_PROJECT_EVENT_TIE = 2u,
       CF_PROJECT_EVENT_LEGATO = 4u, CF_PROJECT_EVENT_CHROMATIC = 8u,
       CF_PROJECT_EVENT_FLAGS = 15u };

typedef struct {
    char name[CF_PROJECT_SECTION_NAME];
    uint8_t repeats;
    uint8_t flags;                 /* v1: reserved, must be zero */
    uint16_t drum_pattern;         /* stable pattern ID, FOLLOW or OFF */
    cf_song_t phrase;              /* existing semantic CSG1 chord phrase */
} cf_project_section_t;

typedef struct {
    uint32_t start_tick;           /* section-relative, 24-bit on disk */
    uint16_t duration_ticks;
    uint8_t note, velocity, probability;
    uint8_t section, flags;
} cf_project_event_t;

typedef struct {
    uint32_t tick;               /* absolute song tick; total_ticks means stopped */
    uint32_t section_tick;       /* tick within the current section occurrence */
    uint8_t section, repeat;     /* zero-based section and repeat occurrence */
    uint8_t ended;
} cf_project_cursor_t;

typedef struct {
    char name[CF_PROJECT_NAME];
    uint8_t section_count, selected_section;
    uint8_t melody_count, bass_count;
    cf_project_section_t section[CF_PROJECT_MAX_SECTIONS];
    cf_project_event_t melody[CF_PROJECT_MAX_EVENTS];
    cf_project_event_t bass[CF_PROJECT_MAX_EVENTS];
} cf_project_t;

int cf_project_valid(const cf_project_t *project);
uint32_t cf_project_section_ticks(const cf_project_t *project, unsigned section);
uint32_t cf_project_total_ticks(const cf_project_t *project);
/* Deterministic arrangement transport. No allocation, I/O, or audio calls. */
int cf_project_playback_seek(const cf_project_t *project, cf_project_cursor_t *cursor, uint32_t tick);
int cf_project_playback_advance(const cf_project_t *project, cf_project_cursor_t *cursor, uint32_t ticks);
/* Returns matching onset count copied into out (may be zero); returned pointers
 * refer to the immutable project and remain valid until it is edited. */
unsigned cf_project_playback_events(const cf_project_t *project, const cf_project_cursor_t *cursor,
                                   unsigned track, const cf_project_event_t **out, unsigned capacity);
int cf_project_init(cf_project_t *project, const char *name, const cf_song_t *phrase);
int cf_project_insert_section(cf_project_t *project, unsigned at, unsigned source, int duplicate);
int cf_project_remove_section(cf_project_t *project, unsigned at);
int cf_project_add_event(cf_project_t *project, unsigned track, const cf_project_event_t *event);
int cf_project_remove_event(cf_project_t *project, unsigned track, unsigned at);
/* CPJ1 v1 is variable-length and bounded by CF_PROJECT_WIRE_MAX (3,808 bytes). */
int cf_project_encode(const cf_project_t *project, uint8_t *out, unsigned capacity, unsigned *written);
int cf_project_decode(cf_project_t *project, const uint8_t *bytes, unsigned size);
#endif
