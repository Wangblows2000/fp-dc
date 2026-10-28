/* Battle Glacier enemies (session 24): types 2336-2399. Spec: build/re/spec_bg_enemies.md.
 * New classes: EnemyDrillSat_3291 (BG3-BG5) and EnemyEliteSpawner_3361 (BG5; reusable: Final Dreadnought's spawners map to the
 * same type, bg.h) with their runtime EliteSpawnerWave_3363, the pod's afterimage (EffectsSpadeImage_456 direction 12) and the
 * drill's dirt (EffectsDebris_427 anim 5).
 * Reused, not copied: the saucers / saucer limiters / lava squids are Thermal Base's (tb_enemy.c; RE: BG1-BG5 run the TB events
 * 12397-12424, BG3 / BG5 11969-11991), the Elites Jade Creek's (jc_enemy.c: BG4 / BG5 run the JC5 Elite events 10949-11038, 14397 /
 * 14655 = 11029; the BG-only 14408 "alert inside the view" is jc_enemy_elite_view_alert), the meteor rollers / bull frags Jade
 * Creek's, the super turretus / ghetto shell Fortune Night's, the spawn tubes / rotor blades / pelletizer Sky Battalion's, the
 * dragonflies / turretus / Tredds / extractors the engine's; the explosive blocks a drill sets off are tb_gim.c's
 * (tb_gim_xblock_ignite + its chain 11964 / 11965). The hits, contact damage (B = o->bd) and the defeat of the Q20 drill are the
 * shared rules of combat.c (Lilac, Carol and Milla alike).
 * Labels: RE (decompiled events of handle_frame_62..64: 14276-14306, 14656-14662, 14724, 14324; create_* constructors), RV
 * (original traces), INFERRED. Order: bg_enemy_obj runs each object's events after combat_frame (the original's events
 * 14276+ follow the shared 2730-2768). Performance: per-type lists (enemy_kit.h), nothing runs without these types (S.on); the
 * only object scan is a knocked drill's flight (14299, a few dozen frames), limited to 160 px around it. */
#include "bg.h"
#include "jc.h"
#include "tb.h"
#include "enemy_kit.h"

#define DEFEATED(o) ((o)->fl & 0x40)
#define RT0 OT_BGE_DRILL
#define NRT (OT_BGE_DEB - OT_BGE_DRILL + 1)
static ek_list_t LST[NRT], LMON, LXB;                 /* + the monitors (Q19) and the explosive blocks the drills touch */
#define EACH(T, OB) EK_EACH(&LST[(T) - RT0], T, OB)
static struct { int on, pick3_frame; } S;
static bge_payload_fn PAY;                            /* (set by another module before or after the level loads: kept across levels) */

static obj_t *spawn(int type, const char *anim, float x, float y) { return ek_spawn(&LST[type - RT0], type, anim, x, y, 0); }
static int rnd(int n) { return cross_rand(n); }
/* a point (lx, ly) relative to the hotspot, turned with the object (Chowdren angles: counter-clockwise, as obj_overlaps_bg) */
static void turn(const obj_t *o, double lx, double ly, double *x, double *y) {
    double a = ek_nrm(o->ang), c = ek_cos(a), sn = ek_sin(a);
    *x = floorf(o->x) + lx * c + ly * sn; *y = floorf(o->y) - lx * sn + ly * c;
}
static void act_point(const obj_t *o, double *x, double *y) {             /* the current frame's action point (turned) */
    *x = floorf(o->x); *y = floorf(o->y);
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an); int fi = (int)(f - o->an.b->frames);
    turn(o, o->an.b->act[2 * fi] - f->hx, o->an.b->act[2 * fi + 1] - f->hy, x, y);
}
/* the image box turned with the object (the flying drill at 90 / 270 is 96 x 32; Chowdren collides the rotated image): the
 * collisions of the drill's own events (14299 / 14301 / 14306) use it; its background test is obj_overlaps_bg (rotated mask) */
static void turned_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    *x0 = *x1 = floorf(o->x); *y0 = *y1 = floorf(o->y);
    if (!o->an.b) return;
    const frame_t *f = anim_frame(&o->an);
    for (int k = 0; k < 4; k++) {
        double x, y; turn(o, (k & 1) ? f->w - f->hx : -f->hx, (k & 2) ? f->h - f->hy : -f->hy, &x, &y);
        if (!k) { *x0 = *x1 = (float)x; *y0 = *y1 = (float)y; continue; }
        *x0 = fminf(*x0, (float)x); *x1 = fmaxf(*x1, (float)x); *y0 = fminf(*y0, (float)y); *y1 = fmaxf(*y1, (float)y);
    }
}
static int is_monitor(int t) {
    return t == OT_MONITOR || t == OT_MON_RINGS || t == OT_MON_1UP || t == OT_MON_INVINC || t == OT_MON_KEYSTONE ||
           (t >= OT_MON_WOOD && t <= OT_MON_WATER) || t == OT_MON_BOMB || t == OT_MON_CRATE;
}

int bg_enemy_q20(int t) { return t == OT_BGE_DRILL; }                          /* RE on_frame_62..64_init qualifier 20 */

/* ================================================================ EnemyDrillSat_3291 (RE 14276-14306, ctor, 14324)
 * Ctor: invisible, A 5, H 100; start: K, L = its placed x, y. av: [0] D state (0 buried, -1 rumbling, 1 bursting out, 2 rising,
 * 3 hovering, 6 knocked (spinning), 7 flying, 8 exploding, -2 waiting to come back), [1] E counter / flight speed, [2] F base y,
 * [3] G angle, [4] H 100, [5] I timer, [6] J spin angle, [7] K home x; o->fy = L home y. Contact damage B (o->bd).
 *  D 0: flag 16, C 5 (no hits); the player within 159 px in x and the drill 79 px above to 29 px below him -> D -1.
 *  D -1: sound 35 every 4th frame (g3 % 4); on even frames E + 1 and a clod of dirt; C 5. E > 15 -> D 1, anim 11, sound 306.
 *  D 1: shown, a clod each frame; anim 11 over -> D 2, F = y.  D 2: y = F - H sin G, G + 2; G >= 90 -> D 3, F - H / 2.
 *  D 3: y = F - H / 2 sin G, G + 3 (hovering 0..100 px above its burrow). B 5 while the player is above it (not with the metal
 *  shield), 0 when D <= 0.
 *  Beaten (the shared hit rules; flag 16: no fly-away): D 6, E = X (the hit's knock-back x), |E| >= 10, sound 74; it spins
 *  (J - 2 E a frame, angle J, x + 0.6 X) until |J| >= 450: D 7, angle 90 / 270, E x 1.25, anim 7, sound 195; it flies x + E
 *  (dust every other frame), hits the Q20 enemies it meets (A - 3, C 5, X = E, sound 24, I 4), breaks monitors (Q19) and sets
 *  off explosive blocks (any D: CAM.Z 8, sound 38, D 8); on the background D 8: five debris, explosion01, sound 29 (sound
 *  195 stopped), back home hidden (D -2, A 5, I 60) and buried again (D 0) when I runs out; 1626 px right / 1200 px left of
 *  the camera it goes home too (I 10). */
static void dirt(obj_t *o) {                                                       /* 14279 / 14281: a clod at (x - 20 + 0..39, y + 40) */
    obj_t *d = spawn(OT_BGE_DEB, "bgd_deb", floorf(o->x) - 20 + rnd(40), floorf(o->y) + 40);
    if (!d) return;
    d->layer = o->layer;
    obj_pinball_start(d, 4 + rnd(9), (float)(30 + rnd(20)));                      /* direction 4..12, speed 30..49 (RE pick_random / cross_seed) */
}
static void drill_home(obj_t *o, double I) {                                       /* 14297 / 14298 / 14303 */
    o->hidden = 1; o->ang = 0; o->fl &= ~0x40; o->av[0] = -2; o->A = 5; o->bd = 0; o->av[5] = I;
    o->x = (float)(int)o->av[7]; o->y = (float)(int)o->fy;
}
static void drill_hits(obj_t *o) {                                                 /* 14299: the flying drill on a Q20 enemy */
    float x0, y0, x1, y1; turned_box(o, &x0, &y0, &x1, &y1);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *e = &G.o[i];
        if (!e->alive || e == o || !e->an.b || fabsf(e->x - o->x) > 160 || fabsf(e->y - o->y) > 160 || !enemy_type(e->type)) continue;
        if ((e->fl & 0x60) || e->c > 0 || e->A <= 0 || !obj_rect_hit(e, (int)x0, (int)y0, (int)x1, (int)y1)) continue;
        e->c = 5; e->kx = (float)o->av[1]; e->ky = 0; e->A -= 3; e->inv = 1;      /* (set_shader 0x15: the hit flash) */
        o->av[5] = 4; snd_play_orig(24, 220, 128);
    }
}
static void drill_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gg = &o->av[3], H = o->av[4], *I = &o->av[5], *J = &o->av[6];
    double X = floorf(o->x), Y = floorf(o->y), px = ek_px(p), py = ek_py(p);
    int g3 = (int)G.frame;
    if (*D == 0) { o->fl |= F_NOFLY; o->c = 5; }                                     /* 14276 */
    if (*D == 0 && X >= px - 159 && X <= px + 159 && Y >= py - 79 && Y <= py + 29) *D = -1;   /* 14277 */
    if (*D == -1 && g3 % 4 == 0) snd_play_orig(35, 220, 128);                      /* 14278 */
    if (*D == -1 && g3 % 2 == 0) { *E += 1; dirt(o); o->c = 5; }                   /* 14279 */
    if (*D == -1 && *E > 15) {                                                     /* 14280 */
        *D = 1; ek_restart_a(o, "bgd_rise"); o->fl |= F_NOFLY; *E = 0; snd_play_orig(306, 220, 128);
    }
    if (*D == 1) { o->hidden = 0; dirt(o); }                                       /* 14281 */
    if (*D == 1 && !(ek_a_is(o, "bgd_rise") && !o->an.done)) { *D = 2; *F = Y; ek_set_a(o, "bgd_idle"); }   /* 14283 (anim 11 over: back to anim 0) */
    if (*D == 2) { o->y = (float)(int)(*F - ek_sin(*Gg) * H); *Gg += 2; }          /* 14284 */
    if (*D == 2 && *Gg >= 90) { *D = 3; *F -= H * 0.5; }                           /* 14285 */
    if (*D == 3) { o->y = (float)(int)(*F - ek_sin(*Gg) * H * 0.5); *Gg = fmod(*Gg + 3, 360); }   /* 14286 */
    Y = floorf(o->y);
    if (*D > 0) o->bd = 0;                                                         /* 14287 (RV drill3_idle: B 0 unless the player is above) */
    if (*D > 0 && p->shield != 3 && py < Y) o->bd = 5;                             /* 14288 (OTHERS.B 3: the metal shield) */
    if (*D < 6 && DEFEATED(o)) { *D = 6; *E = o->kx; snd_play_orig(74, 220, 128); o->hidden = 0; }   /* 14289 */
    if (*D == 6 && *E >= 0 && *E < 10) *E = 10;                                    /* 14290 */
    if (*D == 6 && *E < 0 && *E > -10) *E = -10;                                   /* 14291 */
    if (*D == 6) { *J += -2 * *E; o->ang = (float)ek_nrm(*J); o->x = (float)(int)(floorf(o->x) + 0.6 * o->kx); }   /* 14292 */
    if (*D == 6 && (*J >= 450 || *J <= -450)) {                                    /* 14293 / 14294 */
        o->ang = *J >= 450 ? 90.f : 270.f; *D += 1; *E *= 1.25; ek_restart_a(o, "bgd_fly"); snd_play_orig(195, 220, 128);
    }
    if (*D == 7 && *I <= 0) o->x = (float)(int)(floorf(o->x) + *E);               /* 14295 */
    if (*D == 7 && g3 % 2 == 0) { double ax, ay; act_point(o, &ax, &ay); obj_effect((float)ax, (float)ay, "dust"); }   /* 14296 */
    if (*D == 7 && (o->x > G.camx + 1626 || o->x < G.camx - 1200)) { drill_home(o, 10); return; }   /* 14297 / 14298 */
    if (*D == 7 && *I <= 0) drill_hits(o);                                         /* 14299 / 14406 / 14671 */
    if (*I > 0) *I -= 1;                                                           /* 14300 */
    float bx0, by0, bx1, by1; turned_box(o, &bx0, &by0, &bx1, &by1);
    if (*D > 0 && LMON.n) {                                                        /* 14301: one monitor it touches breaks (Q19, flag 6) */
        obj_t *c[8]; int n = 0;
        for (int k = 0; k < LMON.n && n < 8; k++) {
            obj_t *m = &G.o[LMON.idx[k]];
            if (m->alive && is_monitor(m->type) && !(m->fl & 0x200) && fabsf(m->x - o->x) < 96 && fabsf(m->y - o->y) < 128 && obj_rect_hit(m, (int)bx0, (int)by0, (int)bx1, (int)by1)) c[n++] = m;
        }
        if (n) c[rnd(n)]->fl |= 0x200;                                             /* (objects.c: the monitor's blast break, 3243) */
    }
    if (*D == 7 && S.pick3_frame != g3 && obj_overlaps_bg(o)) { S.pick3_frame = g3; *D = 8; }   /* 14302 (one a frame) */
    if (*D == 8) {                                                                 /* 14303 */
        ek_debris(o, 5); obj_effect(o->x, o->y, "explosion"); snd_play_orig(29, 220, 128);
        drill_home(o, 60); return;
    }
    if (*D == -2) { o->hidden = 1; o->c = 5; }                                     /* 14304 */
    if (*D == -2 && *I <= 0) { *D = 0; *Gg = 0; *E = 0; *J = 0; ek_set_a(o, "bgd_idle"); }   /* 14305 */
    if (LXB.n && tb_gim_xblock_ignite(bx0, by0, bx1, by1)) {                                        /* 14306 (hidden drills too: RE, no D test) */
        ek_shake(8); snd_play_orig(38, 220, 128); *D = 8;
    }
}
static void deb_frame(obj_t *d) {                                                  /* the dirt: PinballMovement (gravity INFERRED as TB's debris) */
    if (d->born != G.frame) obj_pinball_step(d, 25);
    if (d->y > G.camy + SCREEN_H + 64 || !obj_near_view(d, 200)) d->alive = 0;
}

/* ================================================================ EnemyEliteSpawner_3361 (RE BG5 14656-14662, 14724; FD 15245-15249 /
 * 21288-21294 / 22376: the same pod with other payloads). Ctor invisible. av: [0] A (its landing y), [1] B (0 waiting, 1 falling,
 * > 1 landed: the opening clock), [2] C (payload code). Start (14724): A = y, y = A - 300. */
static void ghost(const obj_t *s) {                                                /* 14658: EffectsSpadeImage_456 direction 12 (the pod) */
    obj_t *g = spawn(OT_BGE_GHOST, "bgs_pod", s->x, s->y);
    if (g) { g->hidden = 1; g->tm = 0; g->layer = s->layer; }
}
static void payload(obj_t *s) {                                                    /* 14661: the payload at its feet, the spawner goes */
    int code = (int)s->av[BGE_SPAWNER_C];
    if (!(PAY && PAY(s, code)) && code == 0) {                                     /* 0: an Elite (mask F = x, G = y, flag 16) */
        obj_t *m = jc_enemy_elite_create(floorf(s->x), floorf(s->y));
        if (m) m->layer = s->layer;
    }
    s->alive = 0;
}
static void spawner_frame(obj_t *s, player_t *p) {
    double *A = &s->av[0], *B = &s->av[1];
    double X = floorf(s->x), PX = ek_px(p), PY = ek_py(p);                          /* (MV.X / MV.Y) */
    if (*B == 0 && X > PX - 200 && X < PX + 200 && *A > PY - 50 && *A < PY + 150) {   /* 14657 */
        s->hidden = 0; *B += 1; snd_play_orig(345, 220, 128);
    }
    if (*B == 1) { s->y = (float)(int)fmin(*A, floorf(s->y) + 20); ghost(s); }     /* 14658 */
    if (*B == 1 && floorf(s->y) >= *A) {                                           /* 14659 */
        *B += 1; snd_play_orig(346, 220, 128);
        obj_t *w = spawn(OT_BGE_WAVE, "bgs_wave", s->x, s->y); if (w) w->layer = s->layer;
    }
    if (*B > 1) { *B += 1; ek_set_a(s, "bgs_open"); ek_force_frame(s, (int)(*B / 6)); }   /* 14660 */
    if (*B > 30) payload(s);                                                       /* 14661 */
}
static void wave_frame(obj_t *w) {                                                 /* 14656 (y - 4, A + 1) / 14662 (A > 15: gone) */
    if (w->born == G.frame) return;
    w->y -= 4; w->av[0] += 1;
    if (w->av[0] > 15) w->alive = 0;
}
#define GHOST_LIFE 8                                                               /* INFERRED (the class's fade is not in BG's events) */
static void ghost_frame(obj_t *g) { if (++g->tm > GHOST_LIFE) g->alive = 0; }

void bg_enemy_spawner_payload(bge_payload_fn fn) { PAY = fn; }
int bg_enemy_spawners_open(void) { obj_t *s; int n = 0; if (!S.on) return 0; EACH(OT_BGE_SPAWNER, s) n++; return n; }

/* ================================================================ spawn / passes */
void bg_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(&LMON, sizeof LMON); snap_register(&LXB, sizeof LXB); }
    memset(&S, 0, sizeof S); memset(LST, 0, sizeof LST); memset(&LMON, 0, sizeof LMON); memset(&LXB, 0, sizeof LXB);
    S.pick3_frame = -1;
    const level_def_t *lv = level_cur();
    if (lv && lv->stage == 9) jc_enemy_elite_view_alert(1);                        /* 14408 (BG4 / BG5; the other acts have no Elite) */
    S.on = objects_have(OT_BGE_DRILL) || objects_have(OT_BGE_SPAWNER);
    if (!S.on) return;
    int drills = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (is_monitor(o->type)) { ek_list_add(&LMON, o, o->type); continue; }
        if (o->type == OT_TBG_XBLOCK) { ek_list_add(&LXB, o, o->type); continue; }
        if (o->type < OT_BGE_FIRST || o->type > OT_BGE_LAST) continue;
        if (o->x < 0 || o->y < 0) { o->alive = 0; continue; }                      /* off-map templates */
        double C = o->av[BGE_SPAWNER_C];
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 2; o->hidden = 1;
        o->A = 0; o->c = 0; o->bd = 0; o->absorbed = 0; o->fx = o->fy = 0; o->ang = 0;
        ek_list_add(&LST[o->type - RT0], o, o->type);
        switch (o->type) {
        case OT_BGE_DRILL:                                                         /* ctor A 5, H 100, invisible; 14324 K, L */
            o->A = 5; o->av[4] = 100; o->av[7] = floorf(o->x); o->fy = floorf(o->y); o->fl = F_NOFLY; o->c = 5; ek_set_a(o, "bgd_idle"); drills++; break;
        case OT_BGE_SPAWNER:                                                       /* ctor invisible; 14724: A = y, y - 300 */
            o->av[0] = floorf(o->y); o->y = (float)(int)(o->av[0] - 300); o->av[BGE_SPAWNER_C] = C; ek_set_a(o, "bgs_pod"); break;
        default: o->alive = 0; break;
        }
    }
    if (!drills) { LMON.n = 0; LXB.n = 0; }
}
void bg_enemy_pre(player_t *p) { (void)p; }
void bg_enemy_draw(int pass) {                                                     /* the pod's afterimages, fading (alpha INFERRED) */
    if (!S.on || pass != 1) return;
    obj_t *g;
    EACH(OT_BGE_GHOST, g) {
        if (!g->an.b) continue;
        float sx = floorf(g->x - G.camx), sy = floorf(g->y - G.camy);
        if (sx < -96 || sx > SCREEN_W + 96 || sy < -96 || sy > SCREEN_H + 96) continue;
        uint32_t a = (uint32_t)(160 - 160 * g->tm / (GHOST_LIFE + 1));
        frame_draw(g->an.b, anim_frame(&g->an), sx, sy, 0, (a << 24) | 0xffffff);
    }
}
int bg_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_BGE_DRILL: drill_frame(o, p); return 1;
    case OT_BGE_DEB: deb_frame(o); return 1;
    case OT_BGE_SPAWNER: spawner_frame(o, p); return 1;
    case OT_BGE_WAVE: wave_frame(o); return 1;
    case OT_BGE_GHOST: ghost_frame(o); return 1;
    default: return 1;
    }
}
