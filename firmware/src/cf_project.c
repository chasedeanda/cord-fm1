/* SPDX-License-Identifier: GPL-3.0-only */
#include "cf_project.h"
#if !defined(CF_UNIT) && !defined(CF_EMU)
#include <string.h>
#endif

static void cp_u16(uint8_t *p, unsigned v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static unsigned cp_r16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1]<<8; }
static void cp_u24(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); }
static uint32_t cp_r24(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16; }
static void cp_u32(uint8_t *p, uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i)); }
static uint32_t cp_r32(const uint8_t *p) { uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8u*i);return v; }
static uint32_t cp_hash(const uint8_t *p, unsigned n)
{ uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h; }
static int cp_text(const char *s, unsigned cap, int required)
{
    unsigned end=0;
    for(unsigned i=0;i<cap;i++) {
        unsigned c=(uint8_t)s[i];
        if(!c){end=1;if(required && !i)return 0;continue;}
        if(end || c<32 || c>126)return 0;
    }
    return end;
}
static void cp_section_name(char *dst, unsigned index)
{
    static const char digits[]="123456";
    const char prefix[]="Section ";unsigned i=0;
    while(i<sizeof prefix-1){dst[i]=prefix[i];i++;}
    dst[i++]=digits[index<6?index:5];dst[i]=0;
}
static unsigned cp_track_count(const cf_project_t *p,unsigned track)
{ return track==CF_PROJECT_MELODY?p->melody_count:p->bass_count; }
static cf_project_event_t *cp_track_events(cf_project_t *p,unsigned track)
{ return track==CF_PROJECT_MELODY?p->melody:p->bass; }
static const cf_project_event_t *cp_track_events_const(const cf_project_t *p,unsigned track)
{ return track==CF_PROJECT_MELODY?p->melody:p->bass; }

static uint32_t cp_phrase_ticks(const cf_song_t *song)
{
    uint32_t ticks=0;
    for(unsigned i=0;i<song->n;i++) {
        const cf_song_slot_t *slot=&song->slot[i];
        ticks+=slot->unit==CF_SONG_BARS?(uint32_t)slot->bars*song->beats*96u:
            (uint32_t)slot->quarters*96u*(song->beats==6?2u:1u)/4u;
    }
    return ticks;
}

uint32_t cf_project_section_ticks(const cf_project_t *p,unsigned section)
{
    if(!p || section>=p->section_count || !cf_song_valid(&p->section[section].phrase))return 0;
    return cp_phrase_ticks(&p->section[section].phrase);
}

uint32_t cf_project_total_ticks(const cf_project_t *p)
{
    if(!p || !p->section_count || p->section_count>CF_PROJECT_MAX_SECTIONS)return 0;
    uint32_t total=0;
    for(unsigned i=0;i<p->section_count;i++) {
        uint32_t ticks=cf_project_section_ticks(p,i);
        if(!ticks)return 0;
        total+=ticks*p->section[i].repeats;
    }
    return total;
}

int cf_project_playback_seek(const cf_project_t *p,cf_project_cursor_t *c,uint32_t tick)
{
    if(!c || !cf_project_valid(p))return 0;
    uint32_t total=cf_project_total_ticks(p);
    if(tick>total)return 0;
    memset(c,0,sizeof *c);c->tick=tick;
    if(tick==total){c->ended=1;c->section=(uint8_t)(p->section_count-1u);
        c->repeat=(uint8_t)(p->section[c->section].repeats-1u);
        c->section_tick=cf_project_section_ticks(p,c->section);return 1;}
    uint32_t left=tick;
    for(unsigned i=0;i<p->section_count;i++) {
        uint32_t span=cf_project_section_ticks(p,i),section_span=span*p->section[i].repeats;
        if(left<section_span){c->section=(uint8_t)i;c->repeat=(uint8_t)(left/span);
            c->section_tick=left%span;return 1;}
        left-=section_span;
    }
    return 0;
}

int cf_project_playback_advance(const cf_project_t *p,cf_project_cursor_t *c,uint32_t ticks)
{
    if(!p || !c || !cf_project_valid(p) || c->tick>cf_project_total_ticks(p))return 0;
    uint32_t total=cf_project_total_ticks(p);
    uint32_t target=ticks>total-c->tick?total:c->tick+ticks;
    return cf_project_playback_seek(p,c,target);
}

unsigned cf_project_playback_events(const cf_project_t *p,const cf_project_cursor_t *c,
                                    unsigned track,const cf_project_event_t **out,unsigned capacity)
{
    if(!p || !c || !out || (track!=CF_PROJECT_MELODY && track!=CF_PROJECT_BASS))return 0;
    cf_project_cursor_t expected;
    if(!cf_project_playback_seek(p,&expected,c->tick) || expected.ended ||
       expected.section!=c->section || expected.repeat!=c->repeat ||
       expected.section_tick!=c->section_tick || c->ended)return 0;
    const cf_project_event_t *events=cp_track_events_const(p,track);unsigned n=cp_track_count(p,track),found=0;
    for(unsigned i=0;i<n;i++)if(events[i].section==c->section && events[i].start_tick==c->section_tick) {
        if(found<capacity)out[found]=&events[i];
        found++;
    }
    return found<capacity?found:capacity;
}

static unsigned cp_player_release(cf_project_player_t *player,cf_project_action_t *actions,unsigned capacity)
{
    unsigned count=0;
    for(unsigned i=0;i<CF_PROJECT_PLAYER_VOICES;i++)if(player->voice[i].active) {
        if(count<capacity)actions[count]=(cf_project_action_t){player->voice[i].track,player->voice[i].note,0,0};
        count++;player->voice[i].active=0;
    }
    return count<capacity?count:capacity;
}

int cf_project_player_start(cf_project_player_t *player,const cf_project_t *p,uint32_t tick)
{
    if(!player || !cf_project_valid(p))return 0;
    cf_project_cursor_t cursor;if(!cf_project_playback_seek(p,&cursor,tick))return 0;
    memset(player,0,sizeof *player);player->project=p;player->cursor=cursor;
    player->playing=(uint8_t)!cursor.ended;return 1;
}

unsigned cf_project_player_stop(cf_project_player_t *player,cf_project_action_t *actions,unsigned capacity)
{
    if(!player || !actions || capacity<CF_PROJECT_PLAYER_VOICES)return 0;
    unsigned n=cp_player_release(player,actions,capacity);player->playing=0;player->project=0;
    return n;
}

int cf_project_player_seek(cf_project_player_t *player,uint32_t tick,cf_project_action_t *actions,
                           unsigned capacity,unsigned *released)
{
    if(released)*released=0;
    if(!player || !player->project || !actions || capacity<CF_PROJECT_PLAYER_VOICES)return 0;
    cf_project_cursor_t cursor;if(!cf_project_playback_seek(player->project,&cursor,tick))return 0;
    unsigned n=cp_player_release(player,actions,capacity);
    if(released)*released=n;
    player->cursor=cursor;player->playing=(uint8_t)!cursor.ended;
    return 1;
}

unsigned cf_project_player_tick(cf_project_player_t *player,cf_project_action_t *actions,unsigned capacity)
{
    if(!player || !player->playing || !player->project || !actions || capacity<CF_PROJECT_PLAYER_ACTIONS)return 0;
    const cf_project_t *p=player->project;unsigned count=0;
    /* Note-offs are emitted before new onsets at the same tick. */
    for(unsigned i=0;i<CF_PROJECT_PLAYER_VOICES;i++) {
        cf_project_voice_t *v=&player->voice[i];
        if(v->active && v->end_tick<=player->cursor.tick) {
            if(count<capacity)actions[count]=(cf_project_action_t){v->track,v->note,0,0};
            count++;v->active=0;
        }
    }
    for(unsigned track=0;track<2;track++) {
        const cf_project_event_t *hits[CF_PROJECT_MAX_EVENTS];
        unsigned n=cf_project_playback_events(p,&player->cursor,track,hits,CF_PROJECT_MAX_EVENTS);
        for(unsigned j=0;j<n;j++) {
            const cf_project_event_t *e=hits[j];
            uint32_t mix=player->cursor.tick*2654435761u+(uint32_t)(j+1u)*2246822519u+(uint32_t)(track+1u)*3266489917u;
            if(e->probability==0 || (mix%100u)>=e->probability)continue;
            unsigned slot=CF_PROJECT_PLAYER_VOICES;
            for(unsigned k=0;k<CF_PROJECT_PLAYER_VOICES;k++)if(!player->voice[k].active){slot=k;break;}
            if(slot==CF_PROJECT_PLAYER_VOICES)continue;
            if(count<capacity)actions[count]=(cf_project_action_t){(uint8_t)track,e->note,e->velocity,1};
            count++;
            player->voice[slot].active=1;player->voice[slot].track=(uint8_t)track;
            player->voice[slot].note=e->note;player->voice[slot].velocity=e->velocity;
            player->voice[slot].end_tick=player->cursor.tick+e->duration_ticks;
        }
    }
    if(count>capacity)return capacity;
    if(!cf_project_playback_advance(p,&player->cursor,1) || player->cursor.ended) {
        count+=cp_player_release(player,actions+count,capacity-count);player->playing=0;
    }
    return count<capacity?count:capacity;
}

int cf_project_valid(const cf_project_t *p)
{
    if(!p || !cp_text(p->name,CF_PROJECT_NAME,1) || !p->section_count ||
       p->section_count>CF_PROJECT_MAX_SECTIONS || p->selected_section>=p->section_count ||
       p->melody_count>CF_PROJECT_MAX_EVENTS || p->bass_count>CF_PROJECT_MAX_EVENTS)return 0;
    for(unsigned i=0;i<p->section_count;i++) {
        const cf_project_section_t *s=&p->section[i];
        if(!cp_text(s->name,CF_PROJECT_SECTION_NAME,1) || !s->repeats || s->repeats>16 ||
           s->flags || !cf_song_valid(&s->phrase))return 0;
    }
    for(unsigned t=0;t<2;t++) {
        unsigned n=cp_track_count(p,t);const cf_project_event_t *events=cp_track_events_const(p,t);
        for(unsigned i=0;i<n;i++) {
            const cf_project_event_t *e=&events[i];
            if(e->start_tick>0xFFFFFFu || !e->duration_ticks || e->note>127 || e->velocity>127 ||
               e->probability>100 || e->section>=p->section_count || (e->flags&~CF_PROJECT_EVENT_FLAGS) ||
               e->start_tick+e->duration_ticks>cp_phrase_ticks(&p->section[e->section].phrase))return 0;
        }
        for(unsigned i=n;i<CF_PROJECT_MAX_EVENTS;i++) {
            const uint8_t *b=(const uint8_t *)&events[i];
            for(unsigned j=0;j<sizeof events[i];j++)if(b[j])return 0;
        }
    }
    for(unsigned i=p->section_count;i<CF_PROJECT_MAX_SECTIONS;i++) {
        const uint8_t *b=(const uint8_t *)&p->section[i];
        for(unsigned j=0;j<sizeof p->section[i];j++)if(b[j])return 0;
    }
    return 1;
}

int cf_project_init(cf_project_t *p,const char *name,const cf_song_t *phrase)
{
    if(!p || !name || !phrase || !cf_song_valid(phrase))return 0;
    char safe_name[CF_PROJECT_NAME]={0};unsigned name_len=0;
    while(name_len<sizeof safe_name && name[name_len])name_len++;
    if(!name_len || name_len>=sizeof safe_name)return 0;
    memcpy(safe_name,name,name_len);
    if(!cp_text(safe_name,sizeof safe_name,1))return 0;
    cf_song_t phrase_copy=*phrase;
    memset(p,0,sizeof *p);memcpy(p->name,safe_name,sizeof safe_name);
    p->section_count=1;p->section[0].repeats=1;p->section[0].drum_pattern=CF_PROJECT_DRUM_FOLLOW;
    cp_section_name(p->section[0].name,0);p->section[0].phrase=phrase_copy;
    return cf_project_valid(p);
}

int cf_project_insert_section(cf_project_t *p,unsigned at,unsigned source,int duplicate)
{
    if(!cf_project_valid(p) || p->section_count>=CF_PROJECT_MAX_SECTIONS || at>p->section_count ||
       (duplicate && source>=p->section_count))return 0;
    unsigned mc=0,bc=0,original_m=p->melody_count,original_b=p->bass_count;
    if(duplicate) {
        for(unsigned i=0;i<p->melody_count;i++)if(p->melody[i].section==source)mc++;
        for(unsigned i=0;i<p->bass_count;i++)if(p->bass[i].section==source)bc++;
        if((unsigned)p->melody_count+mc>CF_PROJECT_MAX_EVENTS || (unsigned)p->bass_count+bc>CF_PROJECT_MAX_EVENTS)return 0;
    }
    cf_project_section_t value;
    if(duplicate)value=p->section[source];
    else {
        memset(&value,0,sizeof value);value.repeats=1;value.drum_pattern=CF_PROJECT_DRUM_FOLLOW;
        value.phrase=p->section[0].phrase;
    }
    for(unsigned i=p->section_count;i>at;i--)p->section[i]=p->section[i-1];
    p->section[at]=value;p->section_count++;
    if(!duplicate)cp_section_name(p->section[at].name,at);
    for(unsigned i=0;i<p->melody_count;i++)if(p->melody[i].section>=at)p->melody[i].section++;
    for(unsigned i=0;i<p->bass_count;i++)if(p->bass[i].section>=at)p->bass[i].section++;
    if(duplicate) {
        unsigned old_source=source+(source>=at?1u:0u);
        /* Original events occupy the prefix; append copies after the index shift. */
        for(unsigned i=0;i<original_m;i++)if(p->melody[i].section==old_source) {
            cf_project_event_t e=p->melody[i];e.section=(uint8_t)at;p->melody[p->melody_count++]=e;
        }
        for(unsigned i=0;i<original_b;i++)if(p->bass[i].section==old_source) {
            cf_project_event_t e=p->bass[i];e.section=(uint8_t)at;p->bass[p->bass_count++]=e;
        }
    }
    if(p->selected_section>=at)p->selected_section++;
    return cf_project_valid(p);
}

int cf_project_remove_section(cf_project_t *p,unsigned at)
{
    if(!cf_project_valid(p) || p->section_count<=1 || at>=p->section_count)return 0;
    for(unsigned i=at;i+1<p->section_count;i++)p->section[i]=p->section[i+1];
    p->section_count--;memset(&p->section[p->section_count],0,sizeof p->section[0]);
    for(unsigned t=0;t<2;t++) {
        unsigned count=cp_track_count(p,t),w=0;
        cf_project_event_t *events=cp_track_events(p,t);
        for(unsigned i=0;i<count;i++) {
            cf_project_event_t e=events[i];if(e.section==at)continue;
            if(e.section>at)e.section--;
            events[w++]=e;
        }
        memset(events+w,0,(count-w)*sizeof events[0]);
        if(t==CF_PROJECT_MELODY)p->melody_count=(uint8_t)w;else p->bass_count=(uint8_t)w;
    }
    if(p->selected_section==at)p->selected_section=(uint8_t)(at<(unsigned)p->section_count?at:(unsigned)p->section_count-1u);
    else if(p->selected_section>at)p->selected_section--;
    return cf_project_valid(p);
}

int cf_project_add_event(cf_project_t *p,unsigned track,const cf_project_event_t *event)
{
    if(!p || !event || (track!=CF_PROJECT_MELODY && track!=CF_PROJECT_BASS) || !cf_project_valid(p))return 0;
    unsigned n=cp_track_count(p,track);if(n>=CF_PROJECT_MAX_EVENTS)return 0;
    cf_project_event_t *events=cp_track_events(p,track);events[n]=*event;
    if(track==CF_PROJECT_MELODY)p->melody_count++;else p->bass_count++;
    if(!cf_project_valid(p)) {
        if(track==CF_PROJECT_MELODY)p->melody_count--;else p->bass_count--;
        memset(&events[n],0,sizeof events[n]);return 0;
    }
    return 1;
}

int cf_project_remove_event(cf_project_t *p,unsigned track,unsigned at)
{
    if(!cf_project_valid(p) || (track!=CF_PROJECT_MELODY && track!=CF_PROJECT_BASS))return 0;
    unsigned n=cp_track_count(p,track);if(at>=n)return 0;
    cf_project_event_t *events=cp_track_events(p,track);
    for(unsigned i=at;i+1<n;i++)events[i]=events[i+1];
    memset(&events[n-1],0,sizeof events[0]);
    if(track==CF_PROJECT_MELODY)p->melody_count--;else p->bass_count--;return 1;
}

static void cp_event_encode(uint8_t *p,const cf_project_event_t *e)
{
    cp_u24(p,e->start_tick);cp_u16(p+3,e->duration_ticks);p[5]=e->note;p[6]=e->velocity;
    p[7]=e->probability;p[8]=(uint8_t)((e->section<<4)|e->flags);
}
static void cp_event_decode(cf_project_event_t *e,const uint8_t *p)
{
    memset(e,0,sizeof *e);e->start_tick=cp_r24(p);e->duration_ticks=(uint16_t)cp_r16(p+3);
    e->note=p[5];e->velocity=p[6];e->probability=p[7];e->section=p[8]>>4;e->flags=p[8]&15u;
}

int cf_project_encode(const cf_project_t *p,uint8_t *out,unsigned capacity,unsigned *written)
{
    if(written)*written=0;
    if(!out || !written || !cf_project_valid(p))return 0;
    unsigned size=CF_PROJECT_HEADER_WIRE+(unsigned)p->section_count*CF_PROJECT_SECTION_WIRE+
        ((unsigned)p->melody_count+p->bass_count)*CF_PROJECT_EVENT_WIRE+4u;
    if(size>CF_PROJECT_WIRE_MAX || capacity<size)return 0;
    memset(out,0,size);out[0]='C';out[1]='P';out[2]='J';out[3]='1';out[4]=1;
    out[5]=p->section_count;out[6]=p->melody_count;out[7]=p->bass_count;out[8]=p->selected_section;
    cp_u16(out+10,size);memcpy(out+12,p->name,CF_PROJECT_NAME);
    unsigned off=CF_PROJECT_HEADER_WIRE;
    for(unsigned i=0;i<p->section_count;i++) {
        const cf_project_section_t *s=&p->section[i];memcpy(out+off,s->name,CF_PROJECT_SECTION_NAME);
        out[off+16]=s->repeats;out[off+17]=s->flags;cp_u16(out+off+18,s->drum_pattern);
        if(!cf_song_encode(&s->phrase,out+off+20))return 0;
        off+=CF_PROJECT_SECTION_WIRE;
    }
    for(unsigned t=0;t<2;t++) {
        unsigned n=cp_track_count(p,t);const cf_project_event_t *events=cp_track_events_const(p,t);
        for(unsigned i=0;i<n;i++){cp_event_encode(out+off,&events[i]);off+=CF_PROJECT_EVENT_WIRE;}
    }
    cp_u32(out+off,cp_hash(out,off));off+=4;*written=off;return 1;
}

int cf_project_decode(cf_project_t *p,const uint8_t *bytes,unsigned size)
{
    if(!p || !bytes || size<CF_PROJECT_HEADER_WIRE+4u || size>CF_PROJECT_WIRE_MAX ||
       bytes[0]!='C' || bytes[1]!='P' || bytes[2]!='J' || bytes[3]!='1' || bytes[4]!=1 || bytes[9] ||
       cp_r16(bytes+10)!=size || cp_r32(bytes+size-4)!=cp_hash(bytes,size-4))return 0;
    unsigned ns=bytes[5],nm=bytes[6],nb=bytes[7];
    if(!ns || ns>CF_PROJECT_MAX_SECTIONS || nm>CF_PROJECT_MAX_EVENTS || nb>CF_PROJECT_MAX_EVENTS ||
       bytes[8]>=ns || size!=CF_PROJECT_HEADER_WIRE+ns*CF_PROJECT_SECTION_WIRE+(nm+nb)*CF_PROJECT_EVENT_WIRE+4u ||
       !cp_text((const char *)bytes+12,CF_PROJECT_NAME,1))return 0;

    /* Preflight the complete payload before touching the caller's project. */
    unsigned off=CF_PROJECT_HEADER_WIRE;cf_song_t phrase_check;
    uint32_t section_ticks[CF_PROJECT_MAX_SECTIONS]={0};
    for(unsigned i=0;i<ns;i++) {
        if(!cp_text((const char *)bytes+off,CF_PROJECT_SECTION_NAME,1) || !bytes[off+16] ||
           bytes[off+16]>16 || bytes[off+17] ||
           !cf_song_decode(&phrase_check,bytes+off+20,CF_SONG_WIRE))return 0;
        section_ticks[i]=cp_phrase_ticks(&phrase_check);
        off+=CF_PROJECT_SECTION_WIRE;
    }
    for(unsigned t=0;t<2;t++) {
        unsigned n=t==0?nm:nb;
        for(unsigned i=0;i<n;i++) {
            cf_project_event_t e;cp_event_decode(&e,bytes+off);
            if(!e.duration_ticks || e.note>127 || e.velocity>127 || e.probability>100 ||
               e.section>=ns || (e.flags&~CF_PROJECT_EVENT_FLAGS) ||
               e.start_tick+e.duration_ticks>section_ticks[e.section])return 0;
            off+=CF_PROJECT_EVENT_WIRE;
        }
    }

    memset(p,0,sizeof *p);memcpy(p->name,bytes+12,CF_PROJECT_NAME);
    p->section_count=(uint8_t)ns;p->melody_count=(uint8_t)nm;p->bass_count=(uint8_t)nb;
    p->selected_section=bytes[8];off=CF_PROJECT_HEADER_WIRE;
    for(unsigned i=0;i<ns;i++) {
        cf_project_section_t *s=&p->section[i];memcpy(s->name,bytes+off,CF_PROJECT_SECTION_NAME);
        s->repeats=bytes[off+16];s->flags=bytes[off+17];s->drum_pattern=(uint16_t)cp_r16(bytes+off+18);
        (void)cf_song_decode(&s->phrase,bytes+off+20,CF_SONG_WIRE);off+=CF_PROJECT_SECTION_WIRE;
    }
    for(unsigned i=0;i<nm;i++){cp_event_decode(&p->melody[i],bytes+off);off+=CF_PROJECT_EVENT_WIRE;}
    for(unsigned i=0;i<nb;i++){cp_event_decode(&p->bass[i],bytes+off);off+=CF_PROJECT_EVENT_WIRE;}
    return cf_project_valid(p);
}
