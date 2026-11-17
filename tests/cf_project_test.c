/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <string.h>
#include "cf_project.h"
#include "cf_melody.h"
static unsigned checks,failures;
static uint32_t hash(const uint8_t *p,unsigned n){uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}
static void fix_hash(uint8_t *p,unsigned n){uint32_t h=hash(p,n-4);for(unsigned i=0;i<4;i++)p[n-4+i]=(uint8_t)(h>>(8*i));}
#define CHECK(x) do{checks++;if(!(x)){failures++;if(failures<30)printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
static cf_project_event_t event(unsigned section,unsigned tick,unsigned note)
{
    cf_project_event_t e={0};e.section=(uint8_t)section;e.start_tick=tick;e.duration_ticks=24;
    e.note=(uint8_t)note;e.velocity=96;e.probability=100;e.flags=CF_PROJECT_EVENT_ACCENT;return e;
}
static void init_project(cf_project_t *p)
{
    cf_song_t song;CHECK(cf_song_clone(&song,cf_progression_get(0)));
    CHECK(cf_project_init(p,"First Song",&song));CHECK(cf_project_valid(p));
    CHECK(p->section_count==1 && !strcmp(p->section[0].name,"Section 1"));
    CHECK(cf_project_section_ticks(p,0)>0 && cf_project_total_ticks(p)==cf_project_section_ticks(p,0));
    CHECK(!cf_project_init(p,"This project name is too long",&song));CHECK(cf_project_valid(p));
}
static void sections_and_tracks(void)
{
    cf_project_t p;init_project(&p);cf_project_event_t e=event(0,0,60);
    CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&e));e=event(0,96,48);
    CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    CHECK(cf_project_insert_section(&p,1,0,1));CHECK(p.section_count==2 && p.melody_count==2 && p.bass_count==2);
    CHECK(p.melody[0].section==0 && p.melody[1].section==1 && p.bass[1].section==1);
    CHECK(cf_project_insert_section(&p,1,0,0));CHECK(p.section_count==3);
    CHECK(p.melody[0].section==0 && p.melody[1].section==2 && p.melody[0].start_tick==0);
    CHECK(cf_project_remove_section(&p,1));CHECK(p.section_count==2 && p.melody_count==2 && p.bass_count==2);
    CHECK(p.melody[0].section==0 && p.melody[1].section==1);
    CHECK(cf_project_remove_event(&p,CF_PROJECT_MELODY,0));CHECK(p.melody_count==1 && p.melody[0].section==1);
    CHECK(!cf_project_remove_section(&p,99));CHECK(cf_project_valid(&p));
    CHECK(cf_project_remove_section(&p,0));CHECK(p.section_count==1 && p.selected_section==0 && p.melody_count==1 && p.bass_count==1);
}
static void bounds_and_atomicity(void)
{
    cf_project_t p;init_project(&p);p.section[0].phrase.slot[0].bars=8;cf_project_event_t e=event(0,0,60);
    for(unsigned i=0;i<CF_PROJECT_MAX_EVENTS;i++){e.start_tick=i*24;CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&e));}
    cf_project_t before=p;CHECK(!cf_project_add_event(&p,CF_PROJECT_MELODY,&e));CHECK(!memcmp(&p,&before,sizeof p));
    CHECK(!cf_project_insert_section(&p,1,0,1));CHECK(!memcmp(&p,&before,sizeof p));
    e=event(0,0,128);CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));CHECK(p.bass_count==0);
    e=event(0,0,60);e.probability=101;CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    e=event(0,0,60);e.section=1;CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    e=event(0,0,60);e.start_tick=0x1000000u;CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    e=event(0,5000,60);CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    e=event(0,0,60);e.flags=0x80;CHECK(!cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    CHECK(cf_project_valid(&p));
}
static void roundtrip(void)
{
    cf_project_t p,q;init_project(&p);p.section[0].phrase.slot[0].bars=8;
    CHECK(cf_project_insert_section(&p,1,0,1));CHECK(cf_project_insert_section(&p,2,1,1));
    p.selected_section=2;p.section[0].repeats=2;p.section[1].drum_pattern=CF_PROJECT_DRUM_OFF;
    p.section[2].phrase.slot[0].bars=8;
    for(unsigned i=0;i<70;i++) {
        cf_project_event_t m=event(i%3,i*24,60+i%12),b=event(i%3,i*24,36+i%12);
        CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&m));CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&b));
    }
    uint8_t bytes[CF_PROJECT_WIRE_MAX],again[CF_PROJECT_WIRE_MAX];unsigned n=0,n2=0;
    CHECK(cf_project_encode(&p,bytes,sizeof bytes,&n));CHECK(n<=CF_PROJECT_WIRE_MAX);
    CHECK(cf_project_decode(&q,bytes,n));CHECK(cf_project_valid(&q));
    cf_project_t saved=q;
    CHECK(q.section_count==p.section_count && q.melody_count==p.melody_count && q.bass_count==p.bass_count);
    CHECK(q.selected_section==2 && q.section[1].drum_pattern==CF_PROJECT_DRUM_OFF);
    CHECK(cf_project_section_ticks(&q,2)==cf_project_section_ticks(&p,2));
    CHECK(cf_project_total_ticks(&q)==cf_project_total_ticks(&p));
    CHECK(!strcmp(q.name,p.name) && q.melody[69].note==p.melody[69].note && q.bass[69].section==p.bass[69].section);
    CHECK(cf_project_encode(&q,again,sizeof again,&n2) && n==n2 && !memcmp(bytes,again,n));
    for(unsigned i=0;i<n;i++) {
        CHECK(!cf_project_decode(&q,bytes,i));CHECK(!memcmp(&q,&saved,sizeof q));
    }
    for(unsigned bit=0;bit<n*8u;bit++) {
        memcpy(again,bytes,n);again[bit/8]^=(uint8_t)(1u<<(bit%8));
        CHECK(!cf_project_decode(&q,again,n));CHECK(!memcmp(&q,&saved,sizeof q));
    }
    memcpy(again,bytes,n);
    unsigned event_offset=CF_PROJECT_HEADER_WIRE+p.section_count*CF_PROJECT_SECTION_WIRE;
    again[event_offset]=0xFF;again[event_offset+1]=0xFF;again[event_offset+2]=0xFF;
    fix_hash(again,n);CHECK(!cf_project_decode(&q,again,n));CHECK(!memcmp(&q,&saved,sizeof q));
    CHECK(!cf_project_encode(&p,bytes,n-1,&n2));
}

static void playback_timeline(void)
{
    cf_project_t p;init_project(&p);
    uint32_t span=cf_project_section_ticks(&p,0);CHECK(span>96);
    CHECK(cf_project_insert_section(&p,1,0,0));
    p.section[0].repeats=2;p.section[1].repeats=3;
    cf_project_event_t e=event(0,0,60);CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&e));
    e=event(0,96,62);CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&e));
    e=event(1,0,65);CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&e));
    cf_project_cursor_t c;const cf_project_event_t *hits[4];
    CHECK(cf_project_playback_seek(&p,&c,0));CHECK(c.section==0 && c.repeat==0 && c.section_tick==0 && !c.ended);
    CHECK(cf_project_playback_events(&p,&c,CF_PROJECT_MELODY,hits,4)==1 && hits[0]->note==60);
    CHECK(cf_project_playback_advance(&p,&c,96));CHECK(c.section==0 && c.repeat==0 && c.section_tick==96);
    CHECK(cf_project_playback_events(&p,&c,CF_PROJECT_MELODY,hits,4)==1 && hits[0]->note==62);
    CHECK(cf_project_playback_advance(&p,&c,span-96));CHECK(c.section==0 && c.repeat==1 && c.section_tick==0);
    CHECK(cf_project_playback_events(&p,&c,CF_PROJECT_MELODY,hits,4)==1 && hits[0]->note==60);
    CHECK(cf_project_playback_advance(&p,&c,span));CHECK(c.section==1 && c.repeat==0 && c.section_tick==0);
    CHECK(cf_project_playback_events(&p,&c,CF_PROJECT_BASS,hits,4)==1 && hits[0]->note==65);
    CHECK(cf_project_playback_advance(&p,&c,span*2u));CHECK(c.section==1 && c.repeat==2 && c.section_tick==0);
    uint32_t total=cf_project_total_ticks(&p);CHECK(cf_project_playback_advance(&p,&c,total));
    CHECK(c.ended && c.tick==total && !cf_project_playback_events(&p,&c,CF_PROJECT_BASS,hits,4));
    CHECK(cf_project_playback_advance(&p,&c,1) && c.ended && c.tick==total); /* saturates at end */
    CHECK(cf_project_playback_seek(&p,&c,total-1));CHECK(!c.ended && c.section==1 && c.repeat==2);
    CHECK(!cf_project_playback_seek(&p,&c,total+1));
    CHECK(cf_project_playback_seek(&p,&c,0));c.section_tick++;
    CHECK(!cf_project_playback_events(&p,&c,CF_PROJECT_MELODY,hits,4)); /* forged cursor rejected */
}

static void player_scheduler(void)
{
    cf_project_t p;init_project(&p);p.section[0].phrase.slot[0].bars=1;
    cf_project_event_t m=event(0,0,60);m.duration_ticks=2;
    cf_project_event_t b=event(0,1,36);b.duration_ticks=1;
    CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&m));
    CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&b));
    cf_project_player_t player;cf_project_action_t actions[CF_PROJECT_PLAYER_ACTIONS];
    CHECK(cf_project_player_start(&player,&p,0));CHECK(player.playing);
    unsigned n=cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS);
    CHECK(n==1 && actions[0].on && actions[0].track==CF_PROJECT_MELODY && actions[0].note==60);
    n=cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS);
    CHECK(n==1 && actions[0].on && actions[0].track==CF_PROJECT_BASS && actions[0].note==36);
    n=cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS);
    CHECK(n==2);CHECK(!actions[0].on && actions[0].note==60);CHECK(!actions[1].on && actions[1].note==36);
    CHECK(player.playing && player.cursor.tick==3);
    unsigned released=0;CHECK(cf_project_player_seek(&player,20,actions,CF_PROJECT_PLAYER_ACTIONS,&released));
    CHECK(released==0 && player.cursor.tick==20);
    n=cf_project_player_stop(&player,actions,CF_PROJECT_PLAYER_ACTIONS);CHECK(n==0 && !player.playing);
    CHECK(!cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS));

    /* Seeking while notes sound emits balanced note-offs before repositioning. */
    CHECK(cf_project_player_start(&player,&p,0));
    n=cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS);CHECK(n==1 && actions[0].on);
    CHECK(cf_project_player_seek(&player,12,actions,CF_PROJECT_PLAYER_ACTIONS,&released));
    CHECK(released==1 && !actions[0].on && actions[0].note==60);

    /* A zero-probability event is intentionally silent. */
    p.melody[0].probability=0;CHECK(cf_project_valid(&p));
    CHECK(cf_project_player_start(&player,&p,0));
    n=cf_project_player_tick(&player,actions,CF_PROJECT_PLAYER_ACTIONS);CHECK(n==0);
    CHECK(cf_project_player_stop(&player,actions,CF_PROJECT_PLAYER_ACTIONS)==0);
}

typedef struct { cf_project_action_t action[16]; unsigned count; } action_log_t;
static void action_log(void *context,const cf_project_action_t *action)
{
    action_log_t *log=(action_log_t *)context;
    if(log->count<16)log->action[log->count]=*action;
    log->count++;
}
static void visitor_scheduler(void)
{
    cf_project_t p;init_project(&p);p.section[0].phrase.slot[0].bars=1;
    cf_project_event_t m=event(0,0,64);m.duration_ticks=2;
    cf_project_event_t b=event(0,0,40);b.duration_ticks=1;
    CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&m));
    CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&b));
    cf_project_player_t player;action_log_t log={0};
    CHECK(cf_project_player_start(&player,&p,0));
    CHECK(cf_project_player_tick_visit(&player,action_log,&log)==2);
    CHECK(log.count==2 && log.action[0].on && log.action[1].on);
    CHECK(log.action[0].track==CF_PROJECT_MELODY && log.action[1].track==CF_PROJECT_BASS);
    CHECK(cf_project_player_tick_visit(&player,action_log,&log)==1);
    CHECK(!log.action[2].on && log.action[2].note==40);
    CHECK(cf_project_player_tick_visit(&player,action_log,&log)==1);
    CHECK(!log.action[3].on && log.action[3].note==64);
    CHECK(cf_project_player_stop_visit(&player,action_log,&log)==0);
    CHECK(!player.playing && !player.project);

    CHECK(cf_project_player_start(&player,&p,0));log.count=0;
    CHECK(cf_project_player_tick_visit(&player,action_log,&log)==2);
    CHECK(cf_project_player_stop_visit(&player,action_log,&log)==2);
    CHECK(log.count==4 && !log.action[2].on && !log.action[3].on);
}

static void maximum_wire(void)
{
    cf_project_t p;init_project(&p);p.section[0].phrase.slot[0].bars=8;
    while(p.section_count<CF_PROJECT_MAX_SECTIONS)CHECK(cf_project_insert_section(&p,p.section_count,0,0));
    cf_project_event_t e=event(0,0,60);
    for(unsigned i=0;i<CF_PROJECT_MAX_EVENTS;i++){e.start_tick=i*24;CHECK(cf_project_add_event(&p,CF_PROJECT_MELODY,&e));}
    for(unsigned i=0;i<CF_PROJECT_MAX_EVENTS;i++){e.start_tick=i*24;CHECK(cf_project_add_event(&p,CF_PROJECT_BASS,&e));}
    uint8_t bytes[CF_PROJECT_WIRE_MAX];unsigned n=0;
    CHECK(cf_project_encode(&p,bytes,sizeof bytes,&n));CHECK(n==CF_PROJECT_WIRE_MAX);CHECK(n==3808);
}
static void melody_step_input(void)
{
    cf_project_t p;init_project(&p);
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_MELODY,0,0,64,24,100)==1 && p.melody_count==1);
    CHECK(cf_project_find_event(&p,CF_PROJECT_MELODY,0,0,64)==0 && cf_project_find_event(&p,CF_PROJECT_MELODY,0,0,65)<0);
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_MELODY,0,24,64,24,100)==1 && p.melody_count==2);   /* same note, another step */
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_MELODY,0,0,64,24,100)==2 && p.melody_count==1);    /* toggled off */
    CHECK(cf_project_valid(&p));
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_MELODY,0,cf_project_section_ticks(&p,0),60,24,100)==0);  /* past the section */
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_MELODY,5,0,60,24,100)==0 && cf_project_toggle_note(&p,CF_PROJECT_MELODY,0,0,128,24,100)==0);
    CHECK(cf_project_toggle_note(&p,CF_PROJECT_BASS,0,0,40,48,90)==1 && p.bass_count==1);
}
static void melody_editor(void)
{
    cf_project_t p;init_project(&p);
    cf_melody_t m;cf_melody_init(&m,CF_PROJECT_MELODY,0,0,CF_SC_MAJOR);
    unsigned steps=cf_melody_steps(&p,&m);
    CHECK(steps>=16 && m.note==72 && m.step==0);
    cf_melody_pitch(&m,1);CHECK(m.note==74);cf_melody_pitch(&m,-3);CHECK(m.note==69);   /* C major: C D E F G A */
    cf_melody_pitch(&m,7);CHECK(m.note==81);                                              /* a full octave of degrees */
    m.note=72;
    cf_melody_move(&p,&m,-1);CHECK(m.step==steps-1);cf_melody_move(&p,&m,1);CHECK(m.step==0);   /* wraps */
    cf_melody_length(&m,-9);CHECK(m.len_i==0);cf_melody_length(&m,9);CHECK(m.len_i==4);cf_melody_length(&m,-2);CHECK(m.len_i==2);
    CHECK(cf_melody_toggle(&p,&m)==1 && p.melody_count==1);
    CHECK(cf_melody_cell(&p,&m,0,72)==1 && cf_melody_cell(&p,&m,3,72)==2 && cf_melody_cell(&p,&m,4,72)==0 && cf_melody_cell(&p,&m,0,74)==0);
    cf_melody_move(&p,&m,steps-2);CHECK(cf_melody_fit(&p,&m)==2);                         /* cut to the section's end */
    CHECK(cf_melody_toggle(&p,&m)==1 && p.melody[1].duration_ticks==2*CF_MEL_STEP_TICKS && cf_project_valid(&p));
    m.step=0;CHECK(cf_melody_toggle(&p,&m)==2 && p.melody_count==1);                      /* SEL again removes it */
    cf_melody_init(&m,CF_PROJECT_BASS,0,9,CF_SC_MINOR);CHECK(m.note==cf_snap(CF_SC_MINOR,9,40,1));
    CHECK(cf_melody_toggle(&p,&m)==1 && p.bass_count==1 && cf_melody_cell(&p,&m,0,m.note)==1);
    for(int i=0;i<200;i++)cf_melody_pitch(&m,-1);CHECK(m.note<=127);
    for(int i=0;i<200;i++)cf_melody_pitch(&m,1);CHECK(m.note<=127);
}
int main(void)
{
    melody_step_input();
    melody_editor();
    sections_and_tracks();bounds_and_atomicity();roundtrip();playback_timeline();player_scheduler();visitor_scheduler();maximum_wire();
    printf("multi-section project: %u checks, %u failures\n",checks,failures);return failures!=0;
}
