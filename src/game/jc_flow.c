/* Jade Creek stage script (session 21, stage index 5 only): the flow events that belong to no enemy / gimmick / boss family.
 * Types 1280-1309. Spec: build/re/spec_jc_flow.md (static RE of on_frame_40..44_start_events, handle_frame_40..44_events and
 * the frames' own events 10155-11664; labels RE read from code, RV seen in the original, INFERRED reasoned).
 * Structure (RE): jc1-jc5 = json 40-44 = frame index 39-43, Classic route frame 8 I = 4 -> 39 for every character (after Sky
 * Battalion). JC1 starts with the card (4307; start point x -27 + 16, 2956) and its own entry (10365-10367, below); JC1-JC4 end at the
 * right edge (4135: X > W - 50, B > 0 -> 3578, frame + 1, g4 -1) and JC2-JC5 are entered by the walk-in (4490 / 4526 /
 * 10482); JC5's boss ends the stage (results 4560-4586 / 5394, wipe 8547-8549 -> frame 8). The generic parts are level data
 * (levels.c rows jc1-jc5: music slots, cards, checkpoints, water lines, camera bounds, the Sign); this file holds:
 *  - the loop pieces of the JC2 / JC3 shaft (x 5336-5700): JCLoop3 / 4 (Q10) and JCLoop2 (Q11) follow the loop layer like
 *    every loop set (gimmicks.c layers_refresh, registry OD_LAYER0 / 1); JCLoop1 (Q7 only) is driven by the frame's own
 *    events: JC2 10462 and JC3 10644 clear its flag 0 (it becomes solid) once the player is below it and not touching it (on
 *    at the act start, RV); JC3 10645 sets it on loop layer 0 and 10646 while the player is above it;
 *  - JC1's entry: held left of the frame during the card, thrown in at its end (10365-10367, jc_flow_card);
 *  - the act-3 boss end (10772 / 10773: jc_flow_act3_cleared, called by the boss module).
 * The water surface parallax (10368-10373) is level data + parallax.c (par water mode 2). */
#include "jc.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define JCLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("jcflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define JCLOG(...) do { } while (0)
#endif

#define NLOOP 24
static struct {
    int act;                          /* 1-5 in Jade Creek (stage index 5), else 0 */
    short loopx[NLOOP]; int nloopx;   /* JCLoop1 instances (object slots) */
    int loops;                        /* any loop piece placed */
    double y0; int fuel;              /* JC1 entry: the held y, 10366 done */
} J;

int jc_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 5 ? L->act : 0; }

/* the instance's sprite and layer from the importer's lvobj anim (member index | layer << 8 | 0x8000 json layer 1) */
static int lvobj_anim(const obj_t *o, int k) {
    int n = 0;
    for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == k) return G.lvl.objs[j].anim; }
    return 0;
}

void jc_flow_spawn(void) {
    memset(&J, 0, sizeof J);
    J.act = jc_flow_act();
    if (!J.act || !objects_have_from(OT_JCF_LOOP0)) return;
    int seen[3] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_JCF_LOOP0 || o->type > OT_JCF_LOOPX) continue;
        int a = lvobj_anim(o, seen[o->type - OT_JCF_LOOP0]++);
        o->par = a & 0xff; o->layer = (a & 0x8000) ? -1 : (a >> 8) & 0x7f;
        o->an.b = NULL; o->fl = o->type == OT_JCF_LOOPX ? 1 : 0;   /* JCLoop1 starts with flag 0 on (RV jc2_start / jc3_start: jl1_fl 1) */
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        const char *nm = o->type == OT_JCF_LOOP0 ? (o->par ? "jcl_0_1" : "jcl_0_0") : o->type == OT_JCF_LOOP1 ? "jcl_1_0" : "jcl_x";
        int an = ens_anim(nm);
        if (an >= 0) anim_set(&o->an, &G.ens, an); else { o->alive = 0; continue; }
        if (o->type == OT_JCF_LOOPX && J.nloopx < NLOOP) J.loopx[J.nloopx++] = (short)i;
        J.loops = 1;
    }
    layers_refresh(&G.p);                                   /* the Q10 / Q11 pieces take MV.P now (3031 / 3032) */
}

/* the loop pieces' own rules, every frame after the movement (the frame's event order: 10462 / 10644-10646 follow the
 * shared loop-layer events). JCLoop1's flag 0 = not solid (o->fl bit 0, the port's loop-layer convention); on at the act start
 * (RV: build/orig_traces/jc_flow/jc2_start_lilac.csv / jc3_start_lilac.csv, both JC2 instances and JC3's: alterable flags 1) */
static int p326_hit(const obj_t *o) {                      /* check_overlap(JCLoop1, Player_326): the main sensor rows (as fn_boss.c) */
    int ox, oy, h; const signed char *x0, *x1;
    return o->an.b && mv_sensor_rows(SENS_MAIN, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(o, ox, oy, x0, x1, h);
}
static void loops_frame(const player_t *p) {
    if (!J.nloopx) return;
    double py = floor(p->y - ORIG_REF_DY) - 13;             /* Player_326's top (27-px disc at int MV.Y, INFERRED hot spot 13, 13);
                                                               JCLoop1's top is its y (hot spot 0, 0) */
    for (int k = 0; k < J.nloopx; k++) {
        obj_t *o = &G.o[J.loopx[k]];
        if (!o->alive) continue;
        int was = o->fl & 1;
        if ((o->fl & 1) && o->y < py && !p326_hit(o)) o->fl &= ~1;                                     /* 10462 / 10644 */
        if (J.act == 3 && (p->layer == 0 || py < o->y)) o->fl |= 1;                                    /* 10645: MV.P = 0; 10646: above it */
        if ((o->fl & 1) != was) JCLOG("loop1 %s %d", (o->fl & 1) ? "open" : "solid", k);
    }
}

/* JC1's entry (RE 10365-10367, the card group 0x21a66 of frame 40; RV build/orig_traces/jc_flow/jc1_start_*.csv): while the card
 * runs with g4 = 1 the player is held at MV.X -40, G 0 (MV.Y stays at the start point + 16: 229); 10366, once, when the card
 * counter is <= 10 and MV.A = 2 (Carol on foot): an ObjectFuel at the player (her bike: carol_bike.inc 3222 takes it at once);
 * 10367 at the card's end: control, timer and pause back, CAM.M <= 2, MV.B 10, G 15 (RV: X -40 -> -30, Y + 15 per frame).
 * The port's card lasts CARD_DONE frames (approximate, game.c); RV: 161 frames from the act start in the original. */
void jc_flow_card(player_t *p, int left) {
    if (J.act != 1 || G.at_g4 != 1) return;
    if (left > 0) {
        p->x = -40; p->vy = 0; p->vx = 0; p->gsp = 0; p->ground = 0;
        if (!J.y0) J.y0 = p->y;
        p->y = J.y0;
        if (left <= 10 && !J.fuel && chr_is_carol(G.character) && !p->bike) {                      /* 10366 */
            J.fuel = 1;
            obj_t *f = obj_alloc(); int a = bank_anim(&G.objs, "fuel");
            if (f && a >= 0) {
                memset(f, 0, sizeof *f); f->type = OT_FUEL; f->alive = 1; f->hp = 1; f->layer = 2;
                f->x = f->ox = (float)floor(p->x); f->y = f->oy = (float)floor(p->y - ORIG_REF_DY);
                anim_set(&f->an, &G.objs, a); JCLOG("fuel");
            }
        }
    } else {                                                                                       /* 10367 */
        p->vx = 10; p->vy = 15; p->ground = 0; JCLOG("enter");
    }
}

void jc_flow_frame(player_t *p) {
    if (!J.act) return;
    loops_frame(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_JCW")) { double a, c; if (par_water_band(&a, &c)) printf("jcw %d C %.4f anchor %.1f vt %.1f\n", G.frame, c, a, G.camy); }   /* water band (tests) */
#endif
}

void jc_flow_act3_cleared(void) {
    if (J.act != 3) return;
    G.at_g4 = 10; G.at_g5 = 9128; G.at_g6 = 1550;                                                      /* loop_next_act_41 (10579) */
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
    const level_def_t *L = level_cur();
    if (L->music_cp) music_play(L->music_cp);                                                          /* m_stage5b (10579 / 10582) */
    level_release_camera(0);                                                                           /* 10773: CAM.M 2, U 0 */
    G.cam_nC = 0; G.cam_nE = (float)G.lvl.w + 50;                                                      /* the Sign at x 0: C = -213, E = W + 50 (4241 / 4242) */
    JCLOG("act3 cleared");
}

void  jc_flow_draw(int pass) { (void)pass; }
int   jc_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_JCF_LOOP0 && o->type <= OT_JCF_LOOPX; }   /* static pieces */
float jc_flow_hud_dy(void) { return 0.f; }
