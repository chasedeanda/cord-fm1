/* Melody tab: step input for the project's Melody (or Bass) track, the logic a 240x240 piano roll sits on.
 * A cursor (step, pitch) walks a 16th-note grid; pitch moves by scale degrees (CF_SCALE_*), SEL toggles a note,
 * a length cycles 1/2/4/8/16 steps. Pure functions over cf_project_t: the screen only draws cf_melody_cell()
 * and calls cf_melody_move / cf_melody_pitch / cf_melody_toggle / cf_melody_length. Header-only, no globals. */
#ifndef CF_MELODY_H
#define CF_MELODY_H
#include <stdint.h>
#include "cf_project.h"
#include "cf_theory.h"

#define CF_MEL_STEP_TICKS 24u               /* a 16th at 96 PPQN */
static const uint8_t CF_MEL_LENGTHS[5] = {1, 2, 4, 8, 16};   /* in steps */

typedef struct {
    uint8_t track;      /* CF_PROJECT_MELODY or CF_PROJECT_BASS */
    uint8_t section;
    uint8_t len_i;      /* index in CF_MEL_LENGTHS */
    uint8_t vel;        /* 1..127 */
    uint16_t step;      /* the cursor's column */
    uint8_t note;       /* the cursor's row: a MIDI note, always on the scale */
    uint8_t tonic, scale;
} cf_melody_t;

static inline unsigned cf_melody_steps(const cf_project_t *p, const cf_melody_t *m)
{
    return (unsigned)(cf_project_section_ticks(p, m->section) / CF_MEL_STEP_TICKS);
}
static inline void cf_melody_init(cf_melody_t *m, unsigned track, unsigned section, int tonic, int scale)
{
    m->track = (uint8_t)track; m->section = (uint8_t)section; m->len_i = 2; m->vel = 100; m->step = 0;
    m->tonic = (uint8_t)tonic; m->scale = (uint8_t)scale;
    m->note = (uint8_t)cf_snap(scale, tonic, track == CF_PROJECT_BASS ? 40 : 72, 1);
}
/* the cursor along the grid: wraps inside the section */
static inline void cf_melody_move(const cf_project_t *p, cf_melody_t *m, int d)
{
    int n = (int)cf_melody_steps(p, m);
    if (n <= 0) { m->step = 0; return; }
    int s = ((int)m->step + d) % n;
    m->step = (uint16_t)(s < 0 ? s + n : s);
}
/* the cursor's pitch: d scale degrees up or down, kept in 0..127 */
static inline void cf_melody_pitch(cf_melody_t *m, int d)
{
    int rel = (int)m->note - (int)m->tonic, oct = rel >= 0 ? rel / 12 : -((-rel + 11) / 12);
    int idx = cf_scale_index(m->scale, rel - 12 * oct);
    if (idx < 0) { m->note = (uint8_t)cf_snap(m->scale, m->tonic, m->note, 1); return; }
    int n = (int)m->tonic + 12 * oct + cf_scale_step(m->scale, idx + d);
    if (n >= 0 && n <= 127) m->note = (uint8_t)n;
}
static inline void cf_melody_length(cf_melody_t *m, int d)
{
    int i = (int)m->len_i + d;
    m->len_i = (uint8_t)(i < 0 ? 0 : i > 4 ? 4 : i);
}
/* a length that would pass the section's end is cut to fit */
static inline unsigned cf_melody_fit(const cf_project_t *p, const cf_melody_t *m)
{
    unsigned room = cf_melody_steps(p, m) - m->step, len = CF_MEL_LENGTHS[m->len_i];
    return len > room ? room : len;
}
/* SEL: toggle the note under the cursor (1 added, 2 removed, 0 failed) */
static inline int cf_melody_toggle(cf_project_t *p, const cf_melody_t *m)
{
    return cf_project_toggle_note(p, m->track, m->section, (uint32_t)m->step * CF_MEL_STEP_TICKS, m->note,
                                  cf_melody_fit(p, m) * CF_MEL_STEP_TICKS, m->vel);
}
/* what to draw in a cell: 0 empty, 1 a note starts here, 2 a note sustains through it (any pitch) */
static inline int cf_melody_cell(const cf_project_t *p, const cf_melody_t *m, unsigned step, unsigned note)
{
    unsigned n = m->track == CF_PROJECT_BASS ? p->bass_count : p->melody_count, i;
    const cf_project_event_t *ev = m->track == CF_PROJECT_BASS ? p->bass : p->melody;
    uint32_t t = step * CF_MEL_STEP_TICKS;
    int out = 0;
    for (i = 0; i < n; i++) {
        if (ev[i].section != m->section || ev[i].note != note) continue;
        if (ev[i].start_tick == t) return 1;
        if (ev[i].start_tick < t && t < ev[i].start_tick + ev[i].duration_ticks) out = 2;
    }
    return out;
}
#endif
