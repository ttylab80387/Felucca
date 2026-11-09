/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ttylab80387 */
/* Felucca UI: FM BLOCKS, a falling-blocks game on the whole screen (included by ui_input.c, before ui_name.c).
 *
 * Where: at power-on (FELUCCA_GAME_BOOT), and MENU > SYSTEM > GAME, OCT+. It opens on the title (FM BLOCKS, PRESS PLAY
 * blinking, the keys, BEST); PLAY starts a game. HOME held leaves it for the menu as from any page; the game is kept,
 * paused, and GAME again comes back to it. A game per frame as NAME is (ui_input / ui_draw), never a loop of its own: the
 * browser build (web/emu) only moves the clock between those calls.
 *
 * The keys play nothing (keys_mode 2, as NAME):
 *   left OP2 (the black key G#3), right OP3 (A#3): held, repeat after GM_DAS_MS, every GM_ARR_MS (the black keys'
 *   printed names, as the player reads them); soft drop (held) the 4th white key from the left (B3)
 *   turn left MONO (C#5), turn right POLY (D#5); hard drop the 5th white key from the right (C5)
 *   (the white keys have no names: the title says "the lit white keys"). LEDs: the six glow (the dim level,
 *   whatever MENU > LEDS says: ui_input.c ui_leds), a key held lit fully (game_leds, game_glow)
 * The battery as the header has it (ui_draw.c batt_shown: a bolt on USB power, the accent at one bar): the title's
 * top right, the panel's top right.
 *   SELECT turned: left / right, a detent a column
 *   PLAY: start (the title), pause, go on; after GAME OVER the title again
 * The well: 10 x 20 cells of 12 px, the screen's left half; the right half NEXT, SCORE, LINES, LEVEL, BEST. Only the
 * cells that changed are drawn (a whole screen is ~77 ms over the LCD's SPI). Pieces come from a 7-bag; a turn tries
 * a few kicks; a piece on the floor locks after a delay (gm_lock_ms: 0.5 s, or a gravity step if longer), a move or
 * a turn there starting it again up to GM_LOCK_MOVES times until a cell of it is lower than ever (the Tetris
 * Guideline's "extended placement": landing again, the restarts used up, it locks at once); soft drop held on the
 * floor locks it at once (as TGM's). LEVEL is 1 + LINES / 10.
 * BEST is kept across power-offs (gm_best_read / gm_best_write: in the settings record).
 * A clear: the full rows flash white, then go out from the middle (GM_CLEAR_MS, nothing moves meanwhile), the keys'
 * LEDs flash with them, the panel says SINGLE / DOUBLE / TRIPLE / QUAD! for GM_NOTE_MS; the rows above land with
 * a heavy thump (SFX_SLAM); a clear that reaches a new LEVEL then a fanfare (SFX_LEVEL, GM_LVUP_MS after).
 * GAME OVER: the pieces turn gray a row at a time from the floor up (GM_OVER_ROW_MS).
 * The sound: game_snd.c (gsnd_play), its own FM voice.
 * The saver: GM_SAVER_MS without input off a game (the title, paused, GAME OVER), the screen goes dark but for a
 * small PRESS PLAY that moves every GM_SAVER_MOVE_MS (a still picture left up long can stay faintly on an LCD, image
 * retention); any input wakes it and does nothing more. No ghost piece (removed: it read as a shadow on the LCD). */
#define FMB_VERSION "v0.12"                            /* FM BLOCKS' own version (not Felucca's): the title, bottom right */
#define GM_W 10
#define GM_H 20
#define GM_CELL 12u
#define GM_X 8u                                        /* the well's left edge */
#define GM_PX 134u                                     /* the panel's left edge, .. 240 */
#define GM_DAS_MS 170u                                 /* a held left / right repeats after this, */
#define GM_ARR_MS 50u                                  /* .. every this */
#define GM_LOCK_MS 500u                                /* a piece on the floor locks this long after it landed (or a */
                                                       /* .. gravity step, if longer: gm_lock_ms), */
#define GM_LOCK_MOVES 15u                              /* .. each move / turn there starting it again, this many times */
#define GM_SOFT_MS 40u                                 /* G3 held: a row every this at most */
#define GM_FLASH_MS 240u                               /* a clear: the full rows flash white (4 x 60 ms), */
#define GM_CLEAR_MS 400u                               /* .. then go out from the middle to the walls; then they drop */
#define GM_NOTE_MS 1000u                               /* SINGLE .. QUAD! on the panel this long */
/* a new LEVEL: its fanfare this long after the thump (the thump's tail out: the fanfare is under it, game_snd.c) */
#define GM_LVUP_MS 350u
#define GM_WHITE 8u                                    /* (a cell's colour code: white, the clear's flash) */
#define GM_GRAY 9u                                     /* (.. gray: GAME OVER's) */
#define GM_OVER_ROW_MS 40u                             /* GAME OVER: the pieces turn gray a row every this, bottom up */
#define GM_SAVER_MS 60000u                             /* no input this long off a game (the title, paused, over): */
#define GM_SAVER_MOVE_MS 4000u                         /* .. the saver: dark, a small PRESS PLAY moving every this */
enum { GS_NONE, GS_TITLE, GS_PLAY, GS_PAUSE, GS_OVER };
enum { GK_LEFT = 1u << 3, GK_SOFT = 1u << 6, GK_RIGHT = 1u << 5,   /* key k = bit k: OP2 (G#3), B3, OP3 (A#3) */
       GK_CCW = 1u << 20, GK_CW = 1u << 22, GK_DROP = 1u << 19 };  /* MONO (C#5), POLY (D#5), C5 */
#define GK_ALL (GK_LEFT | GK_SOFT | GK_RIGHT | GK_CCW | GK_CW | GK_DROP)

/* I O T S Z J L, four turns each (right turns), a 4 x 4 box: bit 0x8000 >> (row * 4 + col) */
static const uint16_t GM_SHAPE[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, {0x6600, 0x6600, 0x6600, 0x6600}, {0x4E00, 0x4640, 0x0E40, 0x4C40},
    {0x6C00, 0x4620, 0x06C0, 0x8C40}, {0xC600, 0x2640, 0x0C60, 0x4C80}, {0x8E00, 0x6440, 0x0E20, 0x44C0},
    {0x2E00, 0x4460, 0x0E80, 0xC440}};
static const uint8_t GM_RGB[7][3] = {{0, 200, 230}, {240, 210, 0}, {170, 70, 220}, {70, 200, 70},
                                     {230, 60, 60}, {50, 100, 240}, {245, 140, 30}};
static const uint16_t GM_FALL_MS[15] = {800, 720, 630, 550, 470, 380, 300, 220, 130, 100, 80, 80, 80, 70, 70};
static const int8_t GM_KICK[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {-2, 0}, {2, 0}};
static const uint16_t GM_LINE_PTS[5] = {0, 100, 300, 500, 800};

static struct {
    uint8_t on;                                        /* the screen is up */
    uint8_t st;                                        /* GS_* */
    uint8_t board[GM_H][GM_W];                         /* 0 empty, 1..7 a piece's colour */
    uint8_t shown[GM_H][GM_W];                         /* what the LCD has (0xFF: not known) */
    uint8_t cur, rot, nxt;
    int8_t x, y;                                       /* the box's top left cell (y < 0: above the well) */
    uint8_t bag[7], bag_n;
    uint32_t rep;                                      /* the held left / right key (GK_*: a key bit), 0 none */
    uint32_t rep_t, fall_t;                            /* fm1_ms of the next repeat; of the next gravity step */
    uint32_t rnd;                                      /* xorshift, seeded per game */
    uint32_t score, best, lines;
    uint32_t sig;                                      /* the panel's drawn-state cache */
    uint8_t blink;                                     /* the title's PRESS PLAY as drawn: 0 dark, 1 lit, 2 not yet */
    uint8_t batt;                                      /* the title's battery as drawn (batt_shown; 0xFF not yet) */
    uint8_t saver, saver_drawn;                        /* the saver is up; its PRESS PLAY is on the screen */
    uint16_t sx, sy;                                   /* .. where */
    uint32_t act_t, saver_t;                           /* fm1_ms of the last input; of the saver's next move */
    uint32_t clr;                                      /* a clear's rows (bit = row) while its effect plays, 0 none */
    uint32_t clr_t, note_t;                            /* fm1_ms it began; when the panel's SINGLE .. QUAD! ends */
    uint8_t clr_n;                                     /* its rows, 1..4 */
    uint8_t lvup;                                      /* a new LEVEL's fanfare is due: at lvup_t, the clear over */
    uint32_t lvup_t;
    uint32_t over_t;                                   /* fm1_ms of GAME OVER (the gray rising from the floor) */
    uint8_t best_due;                                  /* a new BEST to save once the transport is stopped */
    uint8_t ground, lock_n;                            /* the piece on the floor (lock_t running); its restarts used */
    int8_t low;                                        /* .. the lowest row a cell of it reached (lower: restarts back) */
    uint32_t lock_t;                                   /* .. fm1_ms it locks */
    uint32_t pieces;                                   /* spawned so far (an action that locked one: its pass ends) */
    /* soft drop brought it down, or was pressed on it: held on the floor, it locks at once (a hold kept from the last
     * piece does not: a piece spawned on the floor gets its delay) */
    uint8_t soft;
    uint8_t scene;                                     /* the background drawn behind the well (gm_scene) */
} gm __attribute__((section(".pool")));                /* (zero-initialised; main loop only) */

/* BEST across power-offs, in the settings record (settings_persist.c) in bytes nothing reads: the DRUM engine's row of
 * favourites (favorites.c: a bit per factory preset, DRUM has a dozen kits) ends with these four, little endian (0 in
 * every older setting). Read when the game opens; written when a game beats it (game_poll, the next pass: settings_save
 * once the transport is stopped), so a power-off right after GAME OVER keeps it */
#define GM_BEST_AT (&favorites.factory[ENGI_DRUM][28])
#define GM_BEST_MAX 99999999u                          /* (8 digits: "BEST " and it fit the panel's 16 bytes) */
static uint32_t gm_best_read(void)                     /* (bytes no BEST ever wrote, a foreign backup's: 0) */
{
    const uint8_t *b = GM_BEST_AT;
    uint32_t v = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return v > GM_BEST_MAX ? 0u : v;
}
static void gm_best_write(uint32_t v)                  /* (RAM; game_poll writes the settings record) */
{
    uint8_t *b = GM_BEST_AT;
    b[0] = (uint8_t)v, b[1] = (uint8_t)(v >> 8), b[2] = (uint8_t)(v >> 16), b[3] = (uint8_t)(v >> 24);
}

static int game_on(void) { return gm.on; }
static void game_close(void)
{
    if (gm.on)
        ui.force = 1;
    gm.on = 0;
    gsx.mus_on = 0;                                     /* (game_snd.c: the music stops with the game's screen) */
    if (gm.st == GS_PLAY)
        gm.st = GS_PAUSE;
}
static uint32_t game_leds(void)                        /* lit: the game's keys held; a clear's flash: every key */
{
    uint32_t t = fm1_ms - gm.clr_t;
    if (gm.clr && t < GM_FLASH_MS && !((t / 60u) & 1u))
        return (1u << 27) - 1u;
    return fm1_in.notes & GK_ALL;
}
static uint32_t game_glow(void) { return GK_ALL & ~fm1_in.notes; }  /* the dim level: the others */
/* the buttons' LEDs (ui_input.c ui_leds): PLAY green while a game plays, else breathing (press to start / go on);
 * HOME dim (held: leave); every other button dark, REC too (an armed track is the synth's, nothing here) */
static int game_playing(void) { return gm.st == GS_PLAY && !gm.saver; }

static uint32_t gm_rand(uint32_t n)
{
    gm.rnd ^= gm.rnd << 13;
    gm.rnd ^= gm.rnd >> 17;
    gm.rnd ^= gm.rnd << 5;
    return gm.rnd % n;
}
static uint32_t gm_draw_bag(void)                      /* the 7-bag: each piece once per seven */
{
    uint32_t i, j, t;
    if (!gm.bag_n) {
        for (i = 0; i < 7u; i++)
            gm.bag[i] = (uint8_t)i;
        for (i = 6; i > 0; i--) {
            j = gm_rand(i + 1u);
            t = gm.bag[i], gm.bag[i] = gm.bag[j], gm.bag[j] = (uint8_t)t;
        }
        gm.bag_n = 7;
    }
    return gm.bag[--gm.bag_n];
}
static uint32_t gm_level(void) { return 1u + gm.lines / 10u; }
static uint32_t gm_fall_ms(void)
{
    uint32_t l = gm_level() - 1u;
    return l < NELEM(GM_FALL_MS) ? GM_FALL_MS[l] : 50u;
}

static int gm_fits(uint32_t p, uint32_t r, int32_t x, int32_t y)
{
    uint32_t m = GM_SHAPE[p][r & 3u], k;
    for (k = 0; k < 16u; k++) {
        int32_t bx = x + (int32_t)(k & 3u), by = y + (int32_t)(k >> 2);
        if (!(m & (0x8000u >> k)))
            continue;
        if (bx < 0 || bx >= GM_W || by >= GM_H)
            return 0;
        if (by >= 0 && gm.board[by][bx])
            return 0;
    }
    return 1;
}
static int32_t gm_bottom(void)                         /* the piece's lowest cell's row (its shape turned: not gm.y) */
{
    uint32_t m = GM_SHAPE[gm.cur][gm.rot], k, r = 0;
    for (k = 0; k < 16u; k++)
        if (m & (0x8000u >> k))
            r = k >> 2;
    return gm.y + (int32_t)r;
}
static int gm_spawn(void)                              /* the next piece at the top; 0: no room (GAME OVER) */
{
    gm.cur = gm.nxt;
    gm.nxt = (uint8_t)gm_draw_bag();
    gm.rot = 0;
    gm.x = 3;
    gm.y = gm.cur == 0 ? -1 : 0;                       /* (I: its row 1 on the well's top row) */
    gm.fall_t = fm1_ms + gm_fall_ms();
    gm.ground = gm.lock_n = 0;
    gm.low = (int8_t)gm_bottom();
    gm.pieces++;
    gm.soft = 0;
    return gm_fits(gm.cur, gm.rot, gm.x, gm.y);
}
static void gm_new(void)
{
    memset(gm.board, 0, sizeof gm.board);
    gm.rnd = fm1_ms * 2654435761u | 1u;
    gm.bag_n = 0;
    gm.score = gm.lines = 0;
    gm.rep = 0;
    gm.lvup = 0;
    gm.clr_n = 0;                                      /* (no last game's SINGLE .. QUAD!) */
    gm.note_t = fm1_ms;
    gm.best = gm_best_read();                          /* (a backup restored meanwhile) */
    gm.nxt = (uint8_t)gm_draw_bag();
    gm_spawn();
    gm.st = GS_PLAY;
    gsx.mus_top = 1;                                    /* (the music from its first bar, at once: not a frame on) */
    gsx.bpm = 132;
    gsx.mus_on = 1;
}
static void game_open(void)
{
    if (gm.st == GS_NONE || gm.st == GS_OVER)
        gm.st = GS_TITLE;
    /* the song stops: PLAY is the game's here, and settings_save waits for a stopped transport (BEST with it) */
    if (transport_busy())
        transport_req = 2;
    gm.on = 1;
    song.grid = 2;                                     /* (the keys silent at once: keys_mode, as NAME's) */
    gm.best = gm_best_read();
    gm.saver = 0;
    gm.act_t = fm1_ms;
    ui.force = 1;
}

static void gm_over(void)
{
    gm.st = GS_OVER;
    gm.over_t = fm1_ms;
    gsx.mus_on = 0;                                     /* (the music stops at once: not a frame on) */
    gsnd_play(SFX_OVER);
    if (gm.score > gm_best_read()) {                   /* (the stored one: a backup restored meanwhile) */
        gm.best = gm.score > GM_BEST_MAX ? GM_BEST_MAX : gm.score;
        gm_best_write(gm.best);
        gm.best_due = 1;                               /* (game_poll: written once the transport is stopped) */
    }
}
static void gm_lock(void)                              /* the piece into the board; full rows: their effect, then */
{                                                      /* gm_clear_end; none: the next piece */
    uint32_t m = GM_SHAPE[gm.cur][gm.rot], k, n = 0, lv = gm_level();
    int32_t r, c;
    for (k = 0; k < 16u; k++)
        if (m & (0x8000u >> k)) {
            int32_t by = gm.y + (int32_t)(k >> 2);
            if (by < 0) {                              /* (locked above the well) */
                gm_over();
                return;
            }
            gm.board[by][gm.x + (int32_t)(k & 3u)] = (uint8_t)(gm.cur + 1u);
        }
    gm.clr = 0;
    for (r = 0; r < GM_H; r++) {
        int full = 1;
        for (c = 0; c < GM_W; c++)
            full &= gm.board[r][c] != 0;
        if (full) {
            gm.clr |= 1u << r;
            n++;
        }
    }
    gm.score += GM_LINE_PTS[n] * gm_level();
    gm.lines += n;
    gsnd_play(n ? SFX_LINE1 + n - 1u : SFX_LOCK);
    if (gm_level() > lv)                               /* (only a clear: gm_clear_end times its fanfare; */
        gm.lvup = 1;                                   /* .. kept by the next lock until it plays) */
    if (n) {                                           /* (game_input ends it after GM_CLEAR_MS) */
        gm.clr_t = fm1_ms;
        gm.clr_n = (uint8_t)n;
        gm.note_t = fm1_ms + GM_NOTE_MS;
        return;
    }
    if (!gm_spawn())
        gm_over();
}
static void gm_clear_end(void)                         /* the effect over: the full rows out, the rest down, the next */
{
    int32_t r, w;
    for (r = w = GM_H - 1; r >= 0; r--) {              /* keep the other rows, from the bottom */
        if ((gm.clr >> r) & 1u)
            continue;
        if (w != r)
            memcpy(gm.board[w], gm.board[r], GM_W);
        w--;
    }
    for (; w >= 0; w--)
        memset(gm.board[w], 0, GM_W);
    gm.clr = 0;
    gsnd_play(SFX_SLAM);                               /* (the rows above land: a heavy thump) */
    gm.lvup_t = fm1_ms + GM_LVUP_MS;                   /* (a new LEVEL: its fanfare after the thump) */
    if (!gm_spawn())
        gm_over();
}
/* the piece a pass began with is gone (locked: a new one, a clear's effect, GAME OVER) */
static int gm_gone(uint32_t p0)
{
    return gm.pieces != p0 || gm.clr || gm.st != GS_PLAY;
}
static void gm_rep_take(uint32_t notes, uint32_t now)   /* a left / right pressed while nothing moves (a clear, a */
{                                                      /* .. hard drop): held, the next piece repeats it */
    if (notes & (GK_LEFT | GK_RIGHT)) {
        gm.rep = notes & GK_RIGHT ? GK_RIGHT : GK_LEFT;
        gm.rep_t = now + GM_DAS_MS;
    }
}
static uint32_t gm_lock_ms(void)                       /* the lock delay: never shorter than a gravity step */
{
    uint32_t g = gm_fall_ms();
    return g > GM_LOCK_MS ? g : GM_LOCK_MS;
}
static int gm_grounded(void) { return !gm_fits(gm.cur, gm.rot, gm.x, gm.y + 1); }
static void gm_lock_again(int was_down)                /* a move / turn: on the floor before it, its lock delay again */
{                                                      /* (on the floor now, not as last recorded: gm_floor's) */
    if (was_down && gm.lock_n < GM_LOCK_MOVES) {
        gm.lock_n++;
        gm.lock_t = fm1_ms + gm_lock_ms();
    }
}
static int gm_shift(int32_t d)
{
    if (!gm_fits(gm.cur, gm.rot, gm.x + d, gm.y))
        return 0;
    gm.x = (int8_t)(gm.x + d);
    return 1;
}
/* gravity's step: a row every gm_fall_ms, or GM_SOFT_MS at most with soft drop held */
static uint32_t gm_step_ms(void)
{
    uint32_t iv = gm_fall_ms();
    return (fm1_in.notes & GK_SOFT) && iv > GM_SOFT_MS ? GM_SOFT_MS : iv;
}
/* the floor, after every move / turn and gravity's step (a landing between two turns of a pass counts too): 1 = the
 * piece on it (it locks gm_lock_ms after it landed; landing again with the restarts used up: at once); 0 = it can fall
 * (off a ledge: gravity's step from now, not the one overdue while it sat there). A cell lower than ever gives the
 * restarts back */
static int gm_floor(void)
{
    uint32_t now = fm1_ms;
    int32_t bot = gm_bottom();
    if (bot > gm.low) {
        gm.low = (int8_t)bot;
        gm.lock_n = 0;
    }
    if (!gm_grounded()) {
        if (gm.ground) {
            gm.ground = 0;
            gm.fall_t = now + gm_step_ms();
        }
        return 0;
    }
    if (!gm.ground) {
        gm.ground = 1;
        gm.lock_t = now + (gm.lock_n < GM_LOCK_MOVES ? gm_lock_ms() : 0u);
    }
    if ((int32_t)(now - gm.lock_t) >= 0 || (gm.soft && (fm1_in.notes & GK_SOFT)))   /* (soft drop down to it) */
        gm_lock();
    return 1;
}
static void gm_turn(uint32_t d)                        /* d: 1 right, 3 left */
{
    uint32_t r = (gm.rot + d) & 3u, k;
    int down = gm_grounded();
    for (k = 0; k < NELEM(GM_KICK); k++)
        if (gm_fits(gm.cur, r, gm.x + GM_KICK[k][0], gm.y + GM_KICK[k][1])) {
            gm.x = (int8_t)(gm.x + GM_KICK[k][0]);
            gm.y = (int8_t)(gm.y + GM_KICK[k][1]);
            gm.rot = (uint8_t)r;
            gsnd_play(SFX_TURN);
            gm_lock_again(down);
            gm_floor();
            return;
        }
}
static void gm_move(int32_t d)                         /* a player's move (not gravity's): it ticks */
{
    int down = gm_grounded();
    if (gm_shift(d)) {
        gsnd_play(SFX_MOVE);
        gm_lock_again(down);
        gm_floor();
    }
}
static int32_t gm_drop_y(void)                         /* where a hard drop lands */
{
    int32_t y = gm.y;
    while (gm_fits(gm.cur, gm.rot, gm.x, y + 1))
        y++;
    return y;
}

/* the music while a game plays (not on the title, paused or over), faster by LEVEL: 132 BPM, 4 more a level, 180 at most */
static void gm_music(void)
{
    uint32_t b = 128u + 4u * gm_level();
    gsx.bpm = (uint8_t)(b > 180u ? 180u : b);
    gsx.mus_on = gm.on && gm.st == GS_PLAY;
}


/* ui_input, every pass (the game open or not, the screen dark or not): a new BEST written. settings_save waits for a
 * stopped transport, and a MIDI Start can start it again: stopped until then */
static void game_poll(void)
{
    if (!gm.best_due)
        return;
    if (transport_busy()) {
        transport_req = 2;
    } else {
        settings_save();
        gm.best_due = 0;
    }
}

/* ui_input: pressed (button edges), notes (key edges); every call (each ms or so) */
static void game_input(uint32_t pressed, uint32_t notes)
{
    uint32_t held = fm1_in.notes, now = fm1_ms, iv, k, p0;
    int32_t s = panel_enc(EN_SELECT), turned = 0;
    gm_music();                                        /* (what the last call changed) */
    for (k = 0; k < NE; k++)                           /* (the other knobs: they only wake the saver) */
        turned |= panel_enc(k);
    if (pressed || notes || s || turned || held) {     /* any input: awake; the saver's waking press: nothing more */
        gm.act_t = now;
        if (gm.saver) {
            gm.saver = 0;
            ui.force = 1;
            return;
        }
    }
    /* the saver: no still picture for long (LCD image retention) */
    if (!gm.saver && gm.st != GS_PLAY && now - gm.act_t > GM_SAVER_MS) {
        gm.saver = 1;
        ui.force = 1;
    }
    if ((pressed >> panel.btn[B_PLAY]) & 1u) {
        if (gm.st == GS_TITLE) {                       /* the title -> a new game */
            gm_new();
            gsnd_play(SFX_START);
            ui.force = 1;
        } else if (gm.st == GS_OVER) {                 /* GAME OVER -> the title */
            gm.st = GS_TITLE;
            ui.force = 1;
        } else if (gm.st == GS_PLAY) {
            if (!gm.clr) {                             /* (not in a clear's effect: its timers run on) */
                gm.st = GS_PAUSE;
                gsnd_play(SFX_PAUSE);
            }
        } else {
            gsnd_play(SFX_PAUSE);
            gm.st = GS_PLAY;
            gm.fall_t = now + gm_fall_ms();
            gm.lock_t = now + gm_lock_ms();            /* (on the floor: its delay from the start again) */
        }
    }
    if (gm.st != GS_PLAY)
        return;
    if (gm.lvup && !gm.clr && (int32_t)(now - gm.lvup_t) >= 0) {
        gm.lvup = 0;
        gsnd_play(SFX_LEVEL);
    }
    if (gm.clr) {                                      /* a clear's effect: nothing moves until it is over */
        gm_rep_take(notes, now);
        if (now - gm.clr_t >= GM_CLEAR_MS)
            gm_clear_end();
        return;
    }
    p0 = gm.pieces;                                    /* (an action below that locks it: the pass over) */
    /* this piece's soft drop held (a fresh press makes it this piece's): on the floor it locks, checked before any move
     * of the pass (one could take it off its support) and after each (gm_floor) */
    if (notes & GK_SOFT)
        gm.soft = 1;
    if (gm.soft && (held & GK_SOFT)) {
        gm_floor();
        if (gm_gone(p0)) {
            gm_rep_take(notes, now);                   /* (a left / right of this pass: the next piece's) */
            return;
        }
    }
    for (s = clamp(s, -4, 4); s; s += s > 0 ? -1 : 1) {
        gm_move(s > 0 ? 1 : -1);
        if (gm_gone(p0)) {
            gm_rep_take(notes, now);                   /* (a left / right of this pass: the next piece's) */
            return;
        }
    }
    if (notes & GK_CCW) {
        gm_turn(3u);
        if (gm_gone(p0)) {
            gm_rep_take(notes, now);                   /* (a left / right of this pass: the next piece's) */
            return;
        }
    }
    if (notes & GK_CW) {
        gm_turn(1u);
        if (gm_gone(p0)) {
            gm_rep_take(notes, now);                   /* (a left / right of this pass: the next piece's) */
            return;
        }
    }
    if (notes & GK_DROP) {
        int32_t y = gm_drop_y();
        gm.score += 2u * (uint32_t)(y - gm.y);
        gm.y = (int8_t)y;
        gsnd_play(SFX_DROP);
        gm_lock();
        gm_rep_take(notes, now);
        return;
    }
    if (notes & (GK_LEFT | GK_RIGHT)) {                /* (both: the later key, here the right one) */
        gm.rep = notes & GK_RIGHT ? GK_RIGHT : GK_LEFT;
        gm.rep_t = now + GM_DAS_MS;
        gm_move(gm.rep == GK_RIGHT ? 1 : -1);
    } else if (gm.rep && !(held & gm.rep)) {
        gm.rep = 0;
    } else if (gm.rep && (int32_t)(now - gm.rep_t) >= 0) {
        gm.rep_t = now + GM_ARR_MS;
        gm_move(gm.rep == GK_RIGHT ? 1 : -1);
    }
    if (gm_gone(p0))
        return;
    iv = gm_fall_ms();
    if (held & GK_SOFT) {
        if (iv > GM_SOFT_MS)
            iv = GM_SOFT_MS;
        if (notes & GK_SOFT) {
            gm.fall_t = now;                           /* (at once) */
        } else if ((int32_t)(gm.fall_t - now) > (int32_t)iv) {
            gm.fall_t = now + iv;                      /* (held over a lock: the new piece at its rate too) */
        }
    }
    if (gm_floor())
        return;
    if ((int32_t)(now - gm.fall_t) >= 0) {
        gm.fall_t = now + iv;
        gm.y++;
        if (held & GK_SOFT) {
            gm.score++;
            gm.soft = 1;
        }
        gm_floor();                                    /* (landed by it: recorded before any next input) */
    }
}

/* ------------------------------------------------------------- drawing --- */
static void gm_battery(int32_t x, int32_t cy)        /* on the canvas begun, as the header's (ui_draw.c draw_battery) */
{
    int32_t lvl = batt_shown();
    cv_icon_mid(x, cy, 24, batt_icon(lvl), lvl == 1 ? T_ACCENT : lvl == 0 ? T_MID : T_THEME, T_BG);
}
static uint16_t gm_colour(uint32_t v)                  /* 1..7 a piece, GM_WHITE the clear's flash, GM_GRAY over */
{
    if (v == GM_WHITE)
        return RGB(255, 255, 255);
    if (v == GM_GRAY)
        return RGB(110, 110, 110);
    const uint8_t *c = GM_RGB[(v - 1u) % 7u];
    return RGB(c[0], c[1], c[2]);
}
static void gm_draw_saver(void)                        /* dark, but a small PRESS PLAY, somewhere else every few s */
{
    static const char T[] = "PRESS PLAY";
    uint32_t w = (uint32_t)text_w(&AF_M, T), h = (uint32_t)AF_M.h, r;
    if (ui.force) {
        lcd_fill(0, 0, 240, 240, T_BG);
        gm.saver_drawn = 0;
        gm.saver_t = fm1_ms;
        ui.force = 0;
    }
    if ((int32_t)(fm1_ms - gm.saver_t) < 0)
        return;
    gm.saver_t = fm1_ms + GM_SAVER_MOVE_MS;
    if (gm.saver_drawn)
        lcd_fill(gm.sx, gm.sy, w, h, T_BG);
    r = fm1_ms * 2654435761u;
    gm.sx = (uint16_t)(r % (240u - w));
    gm.sy = (uint16_t)((r >> 16) % (240u - h));
    draw_text_line(gm.sx, gm.sy, w, &AF_M, T, T_MID, T_BG, 0);
    gm.saver_drawn = 1;
}
static void gm_draw_panel(void)
{
    static const char *const ST[] = {"", "", "", "PAUSE", "GAME OVER"};
    static const char *const HINT[] = {"", "", "", "PLAY: GO ON", "PLAY: TITLE"};
    static const char *const NOTE[] = {"", "SINGLE", "DOUBLE", "TRIPLE", "QUAD!"};
    uint32_t m = GM_SHAPE[gm.nxt][0], k;
    int32_t c0 = 3, c1 = 0, r0 = 3, r1 = 0, ox, oy;
    char b[16];
    cv_begin(240u - GM_PX, 240, T_BG);
    cv_text(6, 4, &AF_S, "NEXT", T_MID);
    gm_battery(78, 10);
    for (k = 0; k < 16u; k++)                          /* the piece's cells' bounds, to centre it */
        if (m & (0x8000u >> k)) {
            int32_t r = (int32_t)(k >> 2), c = (int32_t)(k & 3u);
            c0 = c < c0 ? c : c0, c1 = c > c1 ? c : c1;
            r0 = r < r0 ? r : r0, r1 = r > r1 ? r : r1;
        }
    ox = ((int32_t)(240u - GM_PX) - (c1 - c0 + 1) * 10) / 2 - c0 * 10;
    oy = 24 + (20 - (r1 - r0 + 1) * 10) / 2 - r0 * 10;
    for (k = 0; k < 16u; k++)
        if (m & (0x8000u >> k))
            cv_rect(ox + (int32_t)(k & 3u) * 10, oy + (int32_t)(k >> 2) * 10, 9, 9, gm_colour(gm.nxt + 1u));
    cv_text(6, 56, &AF_S, "SCORE", T_MID);
    fmt_int(b, (int32_t)gm.score);
    cv_text(6, 70, &AF_M, b, T_TEXT);
    cv_text(6, 96, &AF_S, "LINES", T_MID);
    fmt_int(b, (int32_t)gm.lines);
    cv_text(6, 110, &AF_M, b, T_TEXT);
    cv_text(6, 136, &AF_S, "LEVEL", T_MID);
    fmt_int(b, (int32_t)gm_level());
    cv_text(6, 150, &AF_M, b, T_TEXT);
    cv_text(6, 178, &AF_M, gm.st == GS_PLAY && (int32_t)(gm.note_t - fm1_ms) > 0 ? NOTE[gm.clr_n & 7u] : ST[gm.st],
            T_ACCENT);
    cv_text(6, 200, &AF_S, HINT[gm.st], T_MID);
    str_cpy(b, "BEST ", sizeof b);
    fmt_int(b + 5, (int32_t)gm.best);
    cv_text(6, 220, &AF_S, b, T_MID);
    cv_blit(GM_PX, 0);
}
static void gm_draw_title(void)
{
    uint32_t p, k, on = (fm1_ms / 500u) & 1u ? 0u : 1u;   /* PRESS PLAY: lit half a second, dark half a second */
    char b[16];
    if (ui.force) {
        uint32_t x = 12;                               /* the seven pieces in a row: 21 cells and 6 gaps of 8 px */
        lcd_fill(0, 0, 240, 240, T_BG);
        for (p = 0; p < 7u; p++) {
            uint32_t m = GM_SHAPE[p][0];
            int32_t c0 = 3, c1 = 0, r0 = 3, r1 = 0;
            for (k = 0; k < 16u; k++)
                if (m & (0x8000u >> k)) {
                    int32_t r = (int32_t)(k >> 2), c = (int32_t)(k & 3u);
                    c0 = c < c0 ? c : c0, c1 = c > c1 ? c : c1;
                    r0 = r < r0 ? r : r0, r1 = r > r1 ? r : r1;
                }
            for (k = 0; k < 16u; k++)
                if (m & (0x8000u >> k))
                    lcd_fill(x + (uint32_t)((int32_t)(k & 3u) - c0) * 8u,
                             (uint32_t)(40 - (r1 - r0 + 1) * 8 + ((int32_t)(k >> 2) - r0) * 8), 7, 7, gm_colour(p + 1u));
            x += (uint32_t)(c1 - c0 + 1) * 8u + 8u;
        }
        draw_text_line(0, 64, 240, &AF_L, "FM BLOCKS", T_TEXT, T_BG, 1);
        draw_text_line(0, 156, 240, &AF_S, "OP2 / OP3  MOVE", T_MID, T_BG, 1);
        draw_text_line(0, 172, 240, &AF_S, "MONO / POLY  TURN", T_MID, T_BG, 1);
        draw_text_line(0, 188, 240, &AF_S, "LIT WHITE KEYS  DOWN / DROP", T_MID, T_BG, 1);
        draw_text_line(0, 204, 240, &AF_S, "HOLD HOME  SYNTH", T_MID, T_BG, 1);
        str_cpy(b, "BEST ", sizeof b);
        fmt_int(b + 5, (int32_t)gm.best);
        draw_text_line(0, 223, 240, &AF_S, b, T_MID, T_BG, 1);
        draw_text_line(196, 223, 40, &AF_S, FMB_VERSION, T_DIM, T_BG, 2);   /* (right-aligned to x 236) */
        gm.blink = 2;
        gm.batt = 0xFF;
        ui.force = 0;
    }
    if ((uint32_t)batt_shown() != gm.batt) {           /* the battery, top right (redrawn when it changes) */
        gm.batt = (uint8_t)batt_shown();
        cv_begin(28, 20, T_BG);
        gm_battery(2, 10);
        cv_blit(210, 0);
    }
    if (on != gm.blink) {
        gm.blink = (uint8_t)on;
        draw_text_line(0, 124, 240, &AF_M, "PRESS PLAY", on ? T_ACCENT : T_BG, T_BG, 1);
    }
}
/* ------------------------------------------------------- backgrounds --- */
/* Behind the well's empty cells, a scene for every two levels, from the ancient world to a city at night: drawn by
 * code (no pictures; our own scenes, after no game's art), dim so the pieces stay clear and a still picture leaves
 * less on the LCD (image retention). gm_bg_px: the colour of well pixel x 0..119, y 0..239 (b: rows up from the
 * floor). */
#define GM_SCENES 8u
static const uint8_t GM_SKY[GM_SCENES][2][3] = {          /* the sky, top .. horizon */
    {{26, 14, 40}, {92, 46, 30}},   /* 0 stones at dawn */
    {{16, 40, 56}, {96, 78, 40}},   /* 1 pyramids */
    {{14, 30, 64}, {60, 76, 96}},   /* 2 a temple */
    {{30, 18, 30}, {96, 54, 34}},   /* 3 an arena at dusk */
    {{4, 6, 22}, {18, 24, 54}},     /* 4 a castle by moonlight */
    {{20, 12, 40}, {70, 40, 60}},   /* 5 domes at dusk */
    {{30, 26, 22}, {70, 58, 44}},   /* 6 factories in smog */
    {{2, 2, 10}, {12, 16, 40}},     /* 7 a city at night */
};
static uint32_t gm_scene(void)
{
    uint32_t s = (gm_level() - 1u) / 2u;
    return s < GM_SCENES ? s : GM_SCENES - 1u;
}
static int32_t gm_abs(int32_t v) { return v < 0 ? -v : v; }
static uint32_t gm_hash(uint32_t a, uint32_t b)          /* a fixed scatter (stars, lit windows) */
{
    uint32_t h = a * 73856093u ^ b * 19349663u;
    return (h ^ h >> 13) * 0x5bd1e995u >> 24;
}
static uint16_t gm_bg_px(uint32_t s, int32_t x, int32_t y)
{
    const uint8_t *t = GM_SKY[s][0], *h = GM_SKY[s][1];
    int32_t b = 239 - y, dx, d;
    switch (s) {
    case 0:                                            /* stones at dawn: hills, two trilithons, a low sun */
        if (b < 24 + gm_abs((x + 10) % 60 - 30) / 3)
            return RGB(26, 34, 22);
        if (((x >= 16 && x < 24) || (x >= 38 && x < 46)) && b < 78)
            return RGB(62, 58, 54);
        if (x >= 13 && x < 49 && b >= 78 && b < 86)
            return RGB(70, 66, 60);
        if (((x >= 72 && x < 79) || (x >= 92 && x < 99)) && b < 66)
            return RGB(54, 50, 48);
        if (x >= 70 && x < 101 && b >= 66 && b < 73)
            return RGB(62, 58, 54);
        dx = x - 86, d = b - 104;
        if (dx * dx + d * d < 18 * 18)
            return RGB(110, 60, 30);
        break;
    case 1:                                            /* pyramids on sand, a pale sun */
        if (b < 20)
            return RGB(84, 66, 34);
        if (b < 20 + 48 - gm_abs(x - 40))
            return x < 40 ? RGB(100, 80, 44) : RGB(70, 54, 30);
        if (b < 20 + 32 - gm_abs(x - 94))
            return x < 94 ? RGB(92, 74, 40) : RGB(64, 50, 28);
        dx = x - 26, d = b - 150;
        if (dx * dx + d * d < 11 * 11)
            return RGB(110, 100, 70);
        break;
    case 2:                                            /* a temple: steps, columns, a pediment */
        if (b < 8 || (b < 16 && x >= 6 && x < 114) || (b < 24 && x >= 10 && x < 110))
            return RGB(66, 66, 70);
        if (b < 84 && x >= 16 && x < 104 && (x - 16) % 14 < 7)
            return (x - 16) % 14 < 2 ? RGB(80, 80, 86) : RGB(64, 64, 70);
        if (b >= 84 && b < 96 && x >= 12 && x < 108)
            return RGB(72, 72, 78);
        if (b >= 96 && b < 96 + (48 - gm_abs(x - 60)) / 2 && x >= 12 && x < 108)
            return RGB(68, 68, 74);
        break;
    case 3:                                            /* an arena: three tiers of arches */
        if (b < 14)
            return RGB(40, 30, 20);
        if (x >= 4 && x < 116 && b < 104 - (x - 60) * (x - 60) / 160) {
            int32_t tier = (b - 14) / 30, k = (b - 14) % 30, a = x % 15 - 7;
            if (tier < 3 && k >= 4 && k < 24 && a > -5 && a < 5 && (k < 18 || a * a + (k - 18) * (k - 18) < 25))
                return RGB(30, 20, 16);
            return k < 3 ? RGB(92, 70, 48) : RGB(80, 60, 42);
        }
        break;
    case 4:                                            /* a castle by moonlight */
        if (b < 18 + gm_abs((x + 30) % 80 - 40) / 6)
            return RGB(12, 20, 16);
        if ((x >= 12 && x < 30) || (x >= 90 && x < 108)) {
            if (b < 100 || (b < 106 && x % 6 < 3))
                return (b > 70 && b < 80 && x % 18 == 6) ? RGB(70, 60, 20) : RGB(38, 40, 52);
        } else if (x >= 48 && x < 72) {
            if (b < 122 || (b < 128 && x % 6 < 3))
                return (b > 96 && b < 106 && (x == 58 || x == 61)) ? RGB(70, 60, 20) : RGB(42, 44, 56);
        } else if (x >= 30 && x < 90) {
            if (b < 64 || (b < 70 && x % 8 < 4))
                return RGB(34, 36, 48);
        }
        dx = x - 94, d = b - 190;
        if (dx * dx + d * d < 10 * 10)
            return RGB(96, 96, 84);
        if (b > 120 && gm_hash((uint32_t)x, (uint32_t)y) < 2)
            return RGB(70, 70, 80);
        break;
    case 5: {                                          /* domes at dusk */
        static const int8_t D[3][3] = {{26, 80, 12}, {62, 104, 17}, {98, 74, 11}};   /* x, base, radius */
        uint32_t i;
        if (b < 16)
            return RGB(30, 22, 26);
        for (i = 0; i < 3u; i++) {
            int32_t cx = D[i][0], base = D[i][1], r = D[i][2], e = r * 7 / 10;
            if (gm_abs(x - cx) < e && b < base)
                return (b > base - 22 && b < base - 12 && gm_abs(x - cx) < 3) ? RGB(20, 14, 18) : RGB(76, 50, 50);
            dx = x - cx, d = b - (base + r * 2 / 3);
            if (dx * dx + d * d < r * r ||                /* (the onion: a ball, a taper to its tip) */
                (b >= base + r * 2 / 3 && b < base + r * 2 + 6 && gm_abs(x - cx) * 2 < base + r * 2 + 6 - b))
                return i & 1u ? RGB(40, 72, 70) : RGB(96, 78, 34);
            if (x == cx && b >= base + r * 2 + 6 && b < base + r * 2 + 14)
                return RGB(96, 78, 34);
        }
        break;
    }
    case 6: {                                          /* factories, chimneys, smoke */
        int32_t k = x % 20;
        if (b < 10)
            return RGB(28, 24, 20);
        if (b < 46 + (k < 14 ? k : 0))
            return (b > 18 && b < 26 && x % 6 < 3) ? RGB(84, 70, 30) : RGB(46, 40, 36);
        if ((x >= 18 && x < 24 && b < 120) || (x >= 66 && x < 73 && b < 140) || (x >= 100 && x < 105 && b < 110))
            return RGB(54, 44, 40);
        dx = x - 72 - (b - 140) / 6, d = (b - 140) % 22 - 11;
        if (b > 140 && b < 220 && dx * dx + d * d < 64 - (b - 140) / 2)
            return RGB(58, 54, 50);
        break;
    }
    default: {                                         /* a city at night: towers, lit windows, stars */
        static const uint8_t H[10] = {96, 146, 112, 184, 124, 164, 102, 206, 136, 154};
        static const uint8_t F[8] = {60, 80, 70, 96, 74, 88, 66, 84};
        int32_t i = x / 12;
        if (x % 12 != 11 && b < H[i]) {
            if (x % 4 >= 1 && x % 4 < 3 && b % 6 >= 2 && b % 6 < 4 && b < H[i] - 4 &&
                gm_hash((uint32_t)(x / 4), (uint32_t)(b / 6)) % 3u == 0u)
                return RGB(96, 84, 40);
            return RGB(20, 22, 34);
        }
        if (b < F[(x + 6) / 15 % 8])
            return RGB(14, 16, 26);
        if (gm_hash((uint32_t)x, (uint32_t)y) < 2)
            return RGB(80, 80, 96);
        break;
    }
    }
    return RGB(t[0] + (h[0] - t[0]) * y / 239, t[1] + (h[1] - t[1]) * y / 239, t[2] + (h[2] - t[2]) * y / 239);
}
static uint16_t gm_cellpx[GM_CELL * GM_CELL] __attribute__((section(".pool")));
/* a cell: the piece's colour (its last row and column the background: the grid), or the background */
static void gm_draw_cell(int32_t r, int32_t c, uint32_t v)
{
    uint32_t i, j, s = gm_scene();
    uint16_t pc = v ? swap16(gm_colour(v)) : 0u;
    lcd_sync();                                        /* (the last cell's transfer may still read the buffer) */
    for (j = 0; j < GM_CELL; j++)
        for (i = 0; i < GM_CELL; i++)
            gm_cellpx[j * GM_CELL + i] = v && i < GM_CELL - 1u && j < GM_CELL - 1u ? pc :
                swap16(gm_bg_px(s, c * (int32_t)GM_CELL + (int32_t)i, r * (int32_t)GM_CELL + (int32_t)j));
    lcd_blit(GM_X + (uint32_t)c * GM_CELL, (uint32_t)r * GM_CELL, GM_CELL, GM_CELL, gm_cellpx);
}
static void game_draw(void)
{
    uint8_t st[16];
    uint32_t m = GM_SHAPE[gm.cur][gm.rot], k, sig;
    int32_t r, c;
    if (gm.saver) {
        gm_draw_saver();
        goto msg;
    }
    if (gm.st == GS_TITLE) {
        gm_draw_title();
        goto msg;
    }
    if (ui.force) {
        lcd_fill(0, 0, 240, 240, T_BG);
        lcd_fill(GM_X - 2u, 0, 1, 240, T_LINE);        /* the well's walls */
        lcd_fill(GM_X + GM_W * GM_CELL, 0, 1, 240, T_LINE);
        memset(gm.shown, 0xFF, sizeof gm.shown);
        gm.sig = 0;
        ui.force = 0;
    }
    if (gm.scene != gm_scene()) {                      /* a new level's scene: every cell again */
        gm.scene = (uint8_t)gm_scene();
        memset(gm.shown, 0xFF, sizeof gm.shown);
    }
    for (r = 0; r < GM_H; r++)
        for (c = 0; c < GM_W; c++) {
            uint32_t v = gm.board[r][c];
            int32_t kr = r - gm.y, kc = c - gm.x;
            if (!v && kc >= 0 && kc < 4 && kr >= 0 && kr < 4 && (m & (0x8000u >> (kr * 4 + kc))))
                v = gm.cur + 1u;                       /* the falling piece */
            if ((gm.clr >> r) & 1u) {                  /* a clear: white flashes, then out from the middle */
                uint32_t t = fm1_ms - gm.clr_t;
                int32_t d = c < GM_W / 2 ? GM_W / 2 - 1 - c : c - GM_W / 2;   /* 0 the middle .. 4 the walls */
                if (t < GM_FLASH_MS)
                    v = (t / 60u) & 1u ? v : GM_WHITE;
                else
                    v = (int32_t)((t - GM_FLASH_MS) / 32u) > d ? 0u : GM_WHITE;
            }
            if (v && gm.st == GS_OVER && r >= GM_H - (int32_t)((fm1_ms - gm.over_t) / GM_OVER_ROW_MS))
                v = GM_GRAY;                           /* GAME OVER: gray, a row at a time from the floor up */
            if (v == gm.shown[r][c])
                continue;
            gm.shown[r][c] = (uint8_t)v;
            gm_draw_cell(r, c, v);
        }
    st[0] = gm.st, st[1] = gm.nxt;
    for (k = 0; k < 4u; k++) {
        st[2 + k] = (uint8_t)(gm.score >> (k * 8u));
        st[6 + k] = (uint8_t)(gm.lines >> (k * 8u));
        st[10 + k] = (uint8_t)(gm.best >> (k * 8u));
    }
    st[14] = (uint8_t)batt_shown();
    st[15] = (uint8_t)(((int32_t)(gm.note_t - fm1_ms) > 0) | gm.clr_n << 1);   /* (the note up or not) */
    sig = (fnv(2166136261u, st, sizeof st) + ux.gen * 7919u) | 1u;
    if (sig != gm.sig) {
        gm.sig = sig;
        gm_draw_panel();
    }
msg:
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
}
