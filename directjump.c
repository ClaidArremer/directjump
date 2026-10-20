/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIRECT JUMP for the Elektron Digitone mk1 (OS 1.44): an elekloader mod.
 *
 * SETTINGS > DIRECT JUMP, ticked: a pattern picked while the sequencer plays
 * takes over on the next step instead of at the end of the current pattern,
 * and it carries on from the same place in the bar (Elektron's "direct jump").
 * Unticked: the stock behaviour.
 *
 * SETTINGS > START ON BAR, ticked: a pick of another pattern (with DIRECT JUMP
 * unticked) starts it from its first step on the next bar line, in time; and
 * pressing the key of the pattern playing relaunches it from its first step on
 * the next beat (as a Perkons does: on the bar, a 16-step pattern would be on
 * its first step anyway, and the relaunch could not be heard). The bar is the Digitone's own song
 * position (its 16ths since PLAY, or from MIDI song position), so it never
 * loses a beat against the tempo or other gear. Both ticked: legato jumps
 * between patterns, and the playing pattern's key relaunches it on the bar.
 * DIGISNAKE (1.1) relaunches its paths with a start: dj_starts.
 *
 * Both are saved with the project; on 1.44 a change also asks the OS to
 * refresh its power-up copy, so they survive a power cycle.
 *
 * How the stock sequencer changes pattern (all in its 24 PPQN tick interrupt,
 * 0x40095538): two ticks into every master step it peeks the next entry of the
 * pattern source (the chain; 0x4009367c) and, only on the master length's last
 * step, arms the change (0x40096054): the next pattern into SQ_NEXT, its start
 * step into SQ_START (always 0), SQ_ARMED set. At the next step boundary the
 * switch (0x4009610c) makes it the active pattern, sets the master counter to
 * the start step and works out each track's step from it.
 *
 * Picking a pattern while playing gives the source a new chain and sets its
 * index to -1; the switch moves it on to 0. So "a pattern has just been
 * picked" is: index -1, and the source's next entry is not the pattern
 * playing. Then this mod arms the change itself (site A), on the next step,
 * with the master counter (wrapped to the new pattern's length) as the start
 * step. A start (START ON BAR) is armed the same way with step 0 as the start,
 * but only on the step whose successor is heard on a bar line; until then the
 * stock arm is held off too. A chain playing on and song mode are left to the
 * stock.
 *
 * The switch's per-track arithmetic assumes the start step 0 (it reads the old
 * pattern's track speeds and never wraps by length), so after a jump this mod
 * works each track's step out again from the new pattern (site B).
 *
 * Addresses are OS 1.44's and come from mod.json's cflags.
 */
#include "digitone-mk1/core3.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef long s32;

#define R8(a)   (*(volatile u8 *)(a))
#define R16(a)  (*(volatile u16 *)(a))
#define R32(a)  (*(volatile u32 *)(a))
#define S32(a)  (*(volatile s32 *)(a))

/* the sequencer's state (OS 1.44) */
#define SQ_ACTIVE     (DJ_SEQ + 0x214)   /* -> the pattern playing */
#define SQ_NEXT       (DJ_SEQ + 0x218)   /* -> the pattern an armed change goes to */
#define SQ_NEXT_ID    (DJ_SEQ + 0x224)
#define SQ_MSTEP      (DJ_SEQ + 0x1c8)   /* u16: the master counter: the step playing */
#define SQ_MSPEED     (DJ_SEQ + 0x24c)   /* s8: the step length's index (per-track mode: 2) */
#define SQ_POS16      (DJ_SEQ + 0x244)   /* 16ths since PLAY (or MIDI song position) */
#define SQ_POSTICK    (DJ_SEQ + 0x248)   /* ticks into that 16th, 0-5 */
#define SQ_LEN_OVR    (DJ_SEQ + 0x2d8)   /* s16: the source's master length, > 1 overrides */
#define SQ_START      (DJ_SEQ + 0x238)   /* the start step a switch takes */
#define SQ_END        (DJ_SEQ + 0x23c)
#define SQ_STEP0      (DJ_SEQ + 0x256)   /* u16[8]: each track's step after a switch */
#define SQ_TICK0      (DJ_SEQ + 0x266)   /* u16[8]: ticks to that step's start */
#define SQ_PENDING    (DJ_SEQ + 0x276)   /* u16: bit t, track t waits SQ_TICK0[t] */
#define SQ_ARMED      (DJ_SEQ + 0x2d0)
#define SQ_NEXT_W12   (DJ_SEQ + 0x2d6)
#define SQ_NEXT_W10   (DJ_SEQ + 0x2da)
#define SQ_NEXT_L16   (DJ_SEQ + 0x2e0)
#define SQ_SOURCE     (DJ_SEQ + 0x2e4)   /* -> the pattern source playing */
#define SQ_SOURCE2    (DJ_SEQ + 0x2e8)   /* -> the one it changes to (song mode) */
#define CHAIN_INDEX   DJ_CHAIN_INDEX     /* the source's index: -1 after a new pick */

/* a pattern's layout (seq-events-dn1's dev.h, core3.h) */
#define PAT_TRACK     976
#define TRK_LEN       965                /* u16 big-endian, per-track mode */
#define TRK_SPEED     967                /* s8, an index into DJ_TICKS */
#define PAT_LEN       90380              /* u16: the master length; 1 = INF */
#define PAT_MODE      90385              /* s8: 1 per-track scale mode */
#define PAT_SPEED     90386              /* s8: the master scale */
#define TRACKS        8

#define TICKS(i)      (((const s32 *)DJ_TICKS)[(signed char)(i)])   /* 24 PPQN ticks a step */
#define PC_OUT        ((void (*)(u32))DJ_PC_OUT)    /* the stock arm's call: MIDI program out */

/* ---- the settings -----------------------------------------------------------
 * dj_data[0]: DIRECT JUMP, dj_data[1]: START ON BAR, 1 ticked. Saved in the
 * project ("DJMP"; a project saved by 1.0-1.2 has [1] = 0, by 1.3 its RESTART
 * PATTERN, which START ON BAR replaces). A project saved
 * without them keeps whatever the unit had, so loading an old project or making
 * a new one does not untick them. */
u8 dj_data[4];
static u8 dj_keep[2];

static void dj_loaded(int32_t found)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (found)
            dj_data[i] &= 1;
        else
            dj_data[i] = dj_keep[i];
        dj_keep[i] = dj_data[i];
    }
}

const struct core_projdata dj_projdata = {
    0x444a4d50u,                        /* "DJMP" */
    sizeof dj_data, dj_data, dj_loaded
};

#define dj_on()       (*(volatile u8 *)&dj_data[0] != 0)
#define dj_bar()      (*(volatile u8 *)&dj_data[1] != 0)

#define BAR_TICKS 96                    /* 16 sixteenths at 24 PPQN: starts */
#define BEAT_TICKS 24                   /* a quarter note: relaunches */
#define J_JUMP    1
#define J_START   2

/* Bumped at every start's switch: DIGISNAKE 1.1 (a weak import) relaunches its
 * paths when it moves. */
volatile uint32_t dj_starts;

/* ---- site A: the sequencer has peeked the next pattern ----------------------
 * The interrupt, two ticks into a master step, while playing. next: the
 * source's next pattern; song: nonzero when song mode is about to change the
 * source; rec: the 24-byte entry it peeked (+4 its id, +10 and +12 words, +16,
 * +20 a flag: 1 stops the sequencer at the switch, as the stock tests it). Returns 1 when it armed the change, 2 when it holds a
 * start until its bar: either way the stock end-of-pattern test and arm are
 * skipped (djsites.s). */
static volatile u8 dj_jumped;           /* the change armed: J_JUMP or J_START */

/* The ticks of a pattern's master step: per-track mode 6 (its master clock
 * runs at 1X), else its master scale's. */
static s32 step_ticks(u32 pat)
{
    if ((signed char)R8(pat + PAT_MODE) == 1)
        return 6;
    return TICKS(R8(pat + PAT_SPEED));
}

int32_t dj_peek(u32 next, u32 song, const u8 *rec)
{
    u32 cur = R32(SQ_ACTIVE);
    u32 start, len;
    int same = next == cur;             /* the playing pattern's key */
    int kind;

    if (song || !next)
        return 0;
    if (same)
        kind = dj_bar() ? J_START : 0;  /* a relaunch */
    else
        kind = dj_on() ? J_JUMP : dj_bar() ? J_START : 0;
    if (!kind)
        return 0;
    if (R32(SQ_SOURCE) != DJ_PATTERN_SOURCE || R32(SQ_SOURCE2) != DJ_PATTERN_SOURCE)
        return 0;                       /* song mode: the stock */
    if (S32(CHAIN_INDEX) >= 0)
        return 0;                       /* a chain playing on: the stock */
    if (R32(SQ_ARMED) || rec[20] == 1)
        return 0;                       /* already armed, or a stop (1; a pick from the keys is 0xff) */

    if (kind == J_START) {
        /* The switch is at the next step boundary and the new pattern's first
         * step is heard one of its steps after that. Here (two ticks into the
         * step, before the clock moves) the ticks played are the song
         * position less one, the boundary was a tick before: so it is heard at
         * pos - 1 + L + Lnew. Arm on the step that puts it on a bar line (a
         * relaunch: a beat), or the first one past it, off the grid; hold the
         * stock off until then. A step longer than a beat (1/2X and slower)
         * relaunches on the first beat its steps reach. */
        s32 L = TICKS(R8(SQ_MSPEED)), Ln = step_ticks(next);
        u32 q = same ? BEAT_TICKS : BAR_TICKS;
        u32 heard;
        if (L <= 0 || Ln <= 0)
            return 0;
        heard = R32(SQ_POS16) * 6 + R32(SQ_POSTICK) + (q - 1) + (u32)L + (u32)Ln;
        if (heard % q >= (u32)L)
            return 2;                   /* not this step: wait for the beat or bar */
    }

    /* SQ_MSTEP is one ahead of the step heard (the sequencer fires a step
     * ahead), and the new pattern's first step is heard a step after the
     * boundary: so it starts at SQ_MSTEP + 1, wrapped as the stock wraps the
     * counter (the source's length override, else the pattern's), then to the
     * new pattern's master length. (1.2: checked in time on a unit.) */
    start = (u32)R16(SQ_MSTEP) + 1;
    len = (u32)(s32)(short)R16(SQ_LEN_OVR);
    if ((s32)len <= 1)
        len = R16(cur + PAT_LEN);
    if ((s32)(short)len > 1)
        start %= len;
    len = R16(next + PAT_LEN);
    if (len > 1)
        start %= len;
    if (kind == J_START)
        start = 0;                      /* from its first step, on the bar */

    /* the stock arm (0x40096054), with this start step */
    R32(SQ_NEXT_ID) = *(const u32 *)(rec + 4);
    R16(SQ_NEXT_W12) = *(const u16 *)(rec + 12);
    R16(SQ_NEXT_W10) = *(const u16 *)(rec + 10);
    R32(SQ_NEXT) = next;
    R32(SQ_NEXT_L16) = *(const u32 *)(rec + 16);
    R32(SQ_START) = start;
    S32(SQ_END) = -1;
    PC_OUT(next);
    R32(SQ_ARMED) = 1;
    dj_jumped = (u8)kind;
    return 1;
}

/* ---- site B: the switch has worked out each track's step --------------------
 * The interrupt, on the switch path, before it stores the pending mask.
 * Returns the mask to store. After a jump, each track's step and the ticks to
 * it come from the new pattern: the master step in ticks of its own speed,
 * wrapped to its own length. */
uint32_t dj_switched(uint32_t mask)
{
    u32 pat, trk, g, len;
    s32 tt, step, off;
    int t, perTrack;

    if (!dj_jumped)
        return mask;
    if (dj_jumped == J_START)
        dj_starts++;                    /* DIGISNAKE relaunches its paths */
    dj_jumped = 0;
    pat = R32(SQ_ACTIVE);
    if (!pat)
        return mask;
    perTrack = (signed char)R8(pat + PAT_MODE) == 1;
    mask = R16(SQ_PENDING);             /* the stock ORs into the old mask */
    for (t = 0; t < TRACKS; t++) {
        trk = pat + PAT_TRACK * t;
        if (perTrack) {
            tt = TICKS(R8(trk + TRK_SPEED));
            g = (u32)R32(SQ_START) * 6;
            len = (u32)R8(trk + TRK_LEN) << 8 | R8(trk + TRK_LEN + 1);
        } else {
            tt = TICKS(R8(pat + PAT_SPEED));
            g = (u32)R32(SQ_START) * tt;
            len = R16(pat + PAT_LEN);
        }
        if (tt <= 0)
            continue;
        step = ((s32)g + tt - 1) / tt;  /* the stock's: the first step at or after g */
        off = (s32)g - step * tt;
        if (off < 0)
            off += tt;
        if (len >= 1)
            step %= (s32)len;
        R16(SQ_STEP0 + 2 * t) = (u16)step;
        R16(SQ_TICK0 + 2 * t) = (u16)off;
        if (off > 0)
            mask |= 1u << t;
        else
            mask &= ~(1u << t);
    }
    return mask;
}

/* ---- the UI task: keep the setting, and its power-up copy -------------------
 * As DIGISNAKE and PUMP do: after a change and 1.5 s of quiet, the request the
 * project's own change notice makes (fw_project's slot 17 with its mirror
 * buffer at +32), unless its notices are suspended (+36). */
#define SAVE_TICKS 45
static u16 dj_save_wait;

static void dj_mirror_request(void)
{
#ifdef DJ_MIRROR
    uint8_t *p = fw_project;
    uint32_t buf;
    if (!p)
        return;
    buf = *(volatile uint32_t *)(p + 32);
    if (!buf || *(volatile uint8_t *)(p + 36))
        return;
    ((void (*)(void *, uint32_t, uint32_t))(*(uint32_t **)p)[17])(p, buf, 0);
#endif
}

void dj_tick(void *ctrl)
{
    (void)ctrl;
    if (dj_data[0] != dj_keep[0] || dj_data[1] != dj_keep[1]) {     /* SETTINGS changed one */
        dj_keep[0] = dj_data[0];
        dj_keep[1] = dj_data[1];
        dj_save_wait = SAVE_TICKS;
    }
    if (dj_save_wait && --dj_save_wait == 0)
        dj_mirror_request();
}
