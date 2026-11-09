/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ttylab80387 */
/* FM BLOCKS' sound (ui_game.c): three two-operator FM voices of its own, as the metronome's click is one (click.c), so
 * the game never touches a track's sound. Voice 0 the effects, 1 the tune, 2 the bass.
 *   effects  ui_game.c asks for one (gsnd_play); the next audio block starts it when nothing more important plays
 *            (SFX_* in priority order: a move never cuts a cleared line's arpeggio). A few notes, each restarting
 *            the envelope (5 ms steps, block by block).
 *   music    Korobeiniki, a Russian folk song (public domain): its tune, and a bass of our own (the chords' roots and
 *            fifths, a quarter each); not any game's arrangement. Once a game, then quiet (8 bars, ~14.5 s at 132 BPM).
 *            Sample by sample here, so the main loop's frames
 *            never shake the beat. ui_game.c: gsx.mus_on while a game plays (off: the notes ring out, the
 *            place kept), gsx.mus_top for a new game, gsx.bpm by LEVEL.
 * Each note restarts the voice's envelope, and the modulator's index follows it (a bright attack, a softer tail).
 * Added after the master like the click (audio.c audio_block): the DAC only (headphones, line out, the speaker),
 * scaled by MASTER, never into USB audio or a recording. */
enum { SFX_NONE, SFX_MOVE, SFX_TURN, SFX_LOCK, SFX_DROP, SFX_PAUSE, SFX_START, SFX_LEVEL,
       SFX_LINE1, SFX_LINE2, SFX_LINE3, SFX_LINE4, SFX_SLAM, SFX_OVER, SFX_COUNT };   /* (LEVEL: a next clear cuts it) */
#define GS_GAIN 2048                                   /* Q12: the effects -12 dB of a full-scale mix at MASTER's top */
#define GS_TUNE_GAIN 1100                              /* .. the tune, the bass: under the effects */
#define GS_BASS_GAIN 1300
#define GS_STEP_SMP 220                                /* a step's length unit: 5 ms */
typedef struct { uint8_t note, len; } gs_step_t;       /* MIDI note (0 = rest), length in 5 ms */
static const gs_step_t GS_S_MOVE[] = {{84, 5}}, GS_S_TURN[] = {{91, 6}}, GS_S_LOCK[] = {{48, 10}},
    GS_S_DROP[] = {{55, 4}, {43, 12}}, GS_S_PAUSE[] = {{79, 8}, {72, 12}}, GS_S_START[] = {{72, 14}, {79, 14}, {84, 40}},
    GS_S_LINE1[] = {{72, 10}, {79, 24}}, GS_S_LINE2[] = {{72, 10}, {76, 10}, {79, 24}},
    GS_S_LINE3[] = {{72, 9}, {76, 9}, {79, 9}, {84, 28}},
    GS_S_LINE4[] = {{72, 9}, {76, 9}, {79, 9}, {84, 9}, {88, 9}, {91, 60}},
    GS_S_SLAM[] = {{52, 40}},                   /* the rows above a clear landing: a heavy thump, E3 sliding to A1 */
    GS_S_LEVEL[] = {{67, 8}, {72, 8}, {76, 8}, {79, 18}, {76, 8}, {79, 60}},   /* a bugle call: G C E G, E G */
    GS_S_OVER[] = {{67, 28}, {63, 28}, {60, 28}, {55, 90}};
static const struct {
    const gs_step_t *s;
    uint8_t n;
    uint8_t ratio;                                     /* the modulator's frequency / the carrier's, Q4 */
    uint8_t index;                                     /* the modulation index at the attack, Q4 (16 = 1 radian) */
    uint8_t decay;                                     /* the envelope falls by 1 / 2^decay a sample */
    uint8_t sweep;                                     /* 0, or the pitch times sweep / 256 each block (2.9 ms), .. */
    uint8_t floor;                                     /* .. down to this note (a kick drum's fall) */
    uint8_t gain;                                      /* 0 as GS_GAIN, else GS_GAIN x gain / 16 */
} GS_SFX[SFX_COUNT] = {
    {0, 0, 0, 0, 0},
    {GS_S_MOVE, 1, 32, 16, 9},   {GS_S_TURN, 1, 48, 24, 9},   {GS_S_LOCK, 1, 16, 32, 10},
    {GS_S_DROP, 2, 16, 48, 10},  {GS_S_PAUSE, 2, 32, 16, 10}, {GS_S_START, 3, 32, 24, 11},
    {GS_S_LEVEL, 6, 16, 56, 12, 0, 0, 20},             /* (ratio 1, a high index: brass, not the clears' bells) */
    {GS_S_LINE1, 2, 56, 32, 11}, {GS_S_LINE2, 3, 56, 32, 11}, {GS_S_LINE3, 4, 56, 32, 11},
    {GS_S_LINE4, 6, 56, 40, 12},
    {GS_S_SLAM, 1, 16, 80, 12, 248, 33, 28},           /* (E3 -> A1 in ~0.1 s, x1.75: heavy on a small speaker too) */
    {GS_S_OVER, 4, 16, 32, 13},
};
/* phase increments (2^32 / FS a cycle) of C9 .. B9; an octave lower: one bit right */
static const uint32_t GS_INC_TOP[12] = {815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
                                        1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
#define GS_RAD_PH 20861u                               /* a Q15 sine times this: 1 radian of phase (2^32 / 2 pi / 2^15) */

/* the music, A minor, in eighths: 8 bars, once */
static const gs_step_t GS_TUNE[] = {
    {76, 2}, {71, 1}, {72, 1}, {74, 2}, {72, 1}, {71, 1},   {69, 2}, {69, 1}, {72, 1}, {76, 2}, {74, 1}, {72, 1},
    {71, 3}, {72, 1}, {74, 2}, {76, 2},                     {72, 2}, {69, 2}, {69, 2}, {0, 2},
    {74, 3}, {77, 1}, {81, 2}, {79, 1}, {77, 1},            {76, 3}, {72, 1}, {76, 2}, {74, 1}, {72, 1},
    {71, 2}, {71, 1}, {72, 1}, {74, 2}, {76, 2},            {72, 2}, {69, 2}, {69, 2}, {0, 2}};
static const gs_step_t GS_BASS[] = {                   /* E Am E Am Dm Am E Am */
    {52, 2}, {59, 2}, {52, 2}, {59, 2}, {45, 2}, {52, 2}, {45, 2}, {52, 2},
    {52, 2}, {59, 2}, {52, 2}, {59, 2}, {45, 2}, {52, 2}, {45, 2}, {52, 2},
    {50, 2}, {57, 2}, {50, 2}, {57, 2}, {45, 2}, {52, 2}, {45, 2}, {52, 2},
    {52, 2}, {59, 2}, {52, 2}, {59, 2}, {45, 2}, {52, 2}, {45, 2}, {52, 2}};
typedef char gs_tune_fits[NELEM(GS_TUNE) == 39u && NELEM(GS_BASS) == 32u ? 1 : -1];   /* (64 eighths each) */

/* one voice: a sine modulating a sine */
typedef struct {
    uint32_t inc_c, inc_m, ph_c, ph_m;
    int32_t env;                                       /* Q15, restarted by each note */
    int32_t ix, d, g;                                  /* index at the attack (Q4); decay (1 / 2^d a sample); gain (Q12) */
} gv_t;
static struct {
    volatile uint8_t req;                              /* ui_game.c: the effect asked for (the most important this frame) */
    volatile uint8_t mus_on, mus_top, bpm;             /* ui_game.c: the music plays; from the top (cleared here); BPM */
    uint8_t on, tail, prio, step;                      /* the effect: sounding; its last note decaying; its SFX_*; next step */
    int32_t left;                                      /* .. samples left in the step */
    uint8_t it, ib;                                    /* the music: the next tune / bass step */
    int32_t lt, lb;                                    /* .. samples to it */
    gv_t v[3];                                         /* 0 the effects, 1 the tune, 2 the bass */
} gsx = {.bpm = 132};                                  /* (one block of state: the audio ISR's own globals keep their place) */

static void gsnd_play(uint32_t id)                     /* (main loop) */
{
    if (id > gsx.req)
        gsx.req = (uint8_t)id;
}

static void gv_note(gv_t *v, uint32_t note, uint32_t ratio)   /* (a rest, note 0: the last note rings on) */
{
    if (!note)
        return;
    v->inc_c = GS_INC_TOP[note % 12u] >> (10u - note / 12u);
    v->inc_m = (v->inc_c >> 4) * ratio;
    v->env = 32767;
}
static inline int32_t gv_tick(gv_t *v)
{
    int32_t m, s, env = v->env;
    if (!env)
        return 0;
    v->ph_m += v->inc_m;
    v->ph_c += v->inc_c;
    m = SINE[v->ph_m >> 22];
    s = SINE[(v->ph_c + (uint32_t)((m * ((v->ix * env) >> 15)) >> 4) * GS_RAD_PH) >> 22];
    env -= (env >> v->d) + 1;
    v->env = env < 0 ? 0 : env;
    return (((s * env) >> 15) * v->g) >> 12;
}

/* add the game's sound to one block of DAC samples (stereo Q15, after the master): out of line, a loop only while it
 * sounds */
static __attribute__((noinline)) void gsnd_render(int32_t *out, uint32_t n)
{
    uint32_t i, r = gsx.req, play, e8;
    int32_t ms = song.master_q12;
    if (r) {
        gsx.req = 0;
        if (!gsx.on || r >= gsx.prio) {
            gsx.on = 1, gsx.tail = 0, gsx.prio = (uint8_t)r, gsx.step = 0, gsx.left = 0;
        }
    }
    if (gsx.mus_top) {                                  /* a new game: the music from its first bar */
        gsx.mus_top = 0;
        gsx.it = gsx.ib = 0;
        gsx.lt = gsx.lb = 0;
    }
    play = gsx.mus_on && (gsx.it < NELEM(GS_TUNE) || gsx.ib < NELEM(GS_BASS));   /* (played through: quiet) */
    if (!gsx.on && !play && !gsx.v[1].env && !gsx.v[2].env)
        return;
    if (gsx.on && gsx.left <= 0 && !gsx.tail) {           /* the effect's next note */
        if (gsx.step >= GS_SFX[gsx.prio].n) {
            gsx.tail = 1;
        } else {
            gs_step_t st = GS_SFX[gsx.prio].s[gsx.step++];
            gsx.left = (int32_t)st.len * GS_STEP_SMP;
            gv_note(&gsx.v[0], st.note, GS_SFX[gsx.prio].ratio);
            gsx.v[0].ix = GS_SFX[gsx.prio].index;
            gsx.v[0].d = GS_SFX[gsx.prio].decay;
        }
    }
    gsx.left -= (int32_t)n;
    if (gsx.on && GS_SFX[gsx.prio].sweep) {            /* the pitch falls block by block, to its floor note */
        uint32_t lo = GS_INC_TOP[GS_SFX[gsx.prio].floor % 12u] >> (10u - GS_SFX[gsx.prio].floor / 12u);
        if (gsx.v[0].inc_c > lo) {
            gsx.v[0].inc_c = (uint32_t)(((uint64_t)gsx.v[0].inc_c * GS_SFX[gsx.prio].sweep) >> 8);
            gsx.v[0].inc_m = (gsx.v[0].inc_c >> 4) * GS_SFX[gsx.prio].ratio;
        }
    }
    gsx.v[0].g = (int32_t)(((uint32_t)GS_GAIN * (uint32_t)ms) >> 12);   /* (MASTER, as the rest of the DAC's signal) */
    if (gsx.on && GS_SFX[gsx.prio].gain)
        gsx.v[0].g = gsx.v[0].g * GS_SFX[gsx.prio].gain / 16;
    gsx.v[1].g = (int32_t)(((uint32_t)GS_TUNE_GAIN * (uint32_t)ms) >> 12);
    gsx.v[2].g = (int32_t)(((uint32_t)GS_BASS_GAIN * (uint32_t)ms) >> 12);
    gsx.v[1].ix = 24, gsx.v[1].d = 13;                       /* the tune: ratio 2, a mellow bell */
    gsx.v[2].ix = 32, gsx.v[2].d = 12;                       /* the bass: ratio 1, rounder, shorter */
    e8 = FS * 30u / (gsx.bpm ? gsx.bpm : 132u);          /* an eighth in samples */
    for (i = 0; i < n; i++) {
        int32_t v, l, rr;
        if (play) {
            if (gsx.it < NELEM(GS_TUNE)) {
                if (gsx.lt <= 0) {
                    gs_step_t st = GS_TUNE[gsx.it++];
                    gsx.lt += (int32_t)(st.len * e8);
                    gv_note(&gsx.v[1], st.note, 32);
                }
                gsx.lt--;
            }
            if (gsx.ib < NELEM(GS_BASS)) {
                if (gsx.lb <= 0) {
                    gs_step_t st = GS_BASS[gsx.ib++];
                    gsx.lb += (int32_t)(st.len * e8);
                    gv_note(&gsx.v[2], st.note, 16);
                }
                gsx.lb--;
            }
        }
        v = gv_tick(&gsx.v[0]) + gv_tick(&gsx.v[1]) + gv_tick(&gsx.v[2]);
        l = out[2u * i] + v;
        rr = out[2u * i + 1u] + v;
        out[2u * i] = l > 32767 ? 32767 : l < -32767 ? -32767 : l;
        out[2u * i + 1u] = rr > 32767 ? 32767 : rr < -32767 ? -32767 : rr;
    }
    if (gsx.tail && !gsx.v[0].env)
        gsx.on = 0;
}
