/* Level definitions (session 13): one row per ported act, so the shared code (act end and transition, results, stage
 * music, water, Time Attack / save records, walk-in) reads level DATA instead of testing level names. Adding an act of
 * an already-ported mechanic = one row here + the importer run (ASSET_PIPELINE "Adding a level"); only genuinely new
 * behaviour needs new code. Values are the original's (labels per column below). */
#include "../fp.h"
#include <math.h>
#include <string.h>

/* Designated fields: anything left out is 0 / NULL (dry, no edge results, ...). Labels: the event numbers are the
 * original's (RE); see RE_NOTES "Level table". */
/* Carol's jump pads (spec_carol_bike C2): x, y, index A, angle B, chain end D, lit E, block pad (DV4 anim 7) */
static const padcfg_t PADS_DV2[] = { {2736,1144,0,0,0,1,0}, {2936,992,1,90,0,0,0}, {2912,688,2,180,0,0,0}, {3176,688,3,180,1,0,0},
    {3400,1416,4,0,0,1,0}, {3400,1224,5,180,0,0,0}, {3728,1224,6,180,0,0,0}, {4032,1224,7,180,1,0,0} };
static const padcfg_t PADS_DV3[] = { {7408,2200,0,90,0,1,0}, {7072,1952,1,270,0,0,0}, {7344,1952,2,90,1,0,0}, {680,1776,3,0,0,1,0},
    {744,1528,4,90,0,0,0}, {256,1328,5,270,0,0,0}, {520,1064,6,0,0,0,0}, {776,1064,7,0,0,0,0}, {128,1704,8,270,0,1,0}, {384,1776,9,0,1,0,0},
    {4736,2104,10,0,0,1,0}, {4904,1920,11,180,0,0,0}, {5136,2104,12,0,0,0,0}, {5408,1864,13,180,0,0,0}, {5664,1712,14,180,1,0,0} };
static const padcfg_t PADS_DV4[] = { {4040,2296,0,0,0,1,1}, {4368,2152,1,180,0,0,0}, {4616,2160,2,0,0,0,0}, {4864,2168,3,180,0,0,0},
    {5136,2152,4,180,0,0,0}, {5296,2152,5,0,0,0,0}, {5504,2176,6,90,0,0,0}, {5344,2096,7,270,0,0,0}, {5344,2000,8,270,1,0,0},
    {10120,2134,9,-4.5f,0,1,1}, {10400,2024,10,180,0,0,0}, {10432,1992,11,270,1,0,0} };
/* Milla's dig spot rewards by spot index A (AT1 4189-4194; spot 3 has no event) */
static const signed char DIGR_AT1[] = { DIG_KEYSTONE_MON, DIG_CRYSTALS, DIG_CRYSTALS, DIG_NONE, DIG_PETAL, DIG_CRYSTALS, DIG_WOOD_MON };
/* Milla's dig zones (spec_milla_moves 6, events 4171-4182 of frame 17) */
static const zone_t DIGZ_AT1[] = {
    { 6939, 7302, 102, 185 }, { 1479, 1705, 594, 671 }, { 9450, 9654, 181, 262 }, { 3214, 3315, 838, 904 },
    { 8866, 9270, 715, 794 }, { 8197, 8418, 442, 507 }, { 9247, 10206, 611, 729 }, { 10607, 10744, 656, 711 },
    { 10523, 10563, 659, 713 }, { 10413, 11262, 1052, 1154 }, { 11671, 12410, 822, 903 }, { 10388, 10482, 157, 223 } };
/* Aqua Tunnel stage flow (spec_at_flow 3 / 6): checkpoints (4196 / 4197, 4405, 4598), boss camera locks (4198 / 4199,
 * 4408 / 4409, 4599) and the act-start item sets (4318 / 4319 crystals per character, 4484 / 4485) */
static const cpzone_t CP_AT1[] = { { 7540, 7640, 2, 7600, 1100, 0, 0 }, { 11250, 11350, 3, 11300, 988, 0, 0 } };
static const cpzone_t CP_AT2[] = { { 5640, 5740, 3, 5720, 270, 0, 0 } };
static const cpzone_t CP_AT4[] = { { 3264 - 0, 3364, 2, 3264, 200, 0, 0 } };
/*                                g4 max, lock X, Y min,  Y max,  l,     t,   r,     b,    unlock X, unlock Y */
static const camzone_t CAM_AT1 = { 0,  11704, -32768, 903,    11368, 600, 12479, 903,  11704 - 240, 903 };
static const camzone_t CAM_AT2 = { 10, 6032,  976,    32767,  6032,  976, 6863,  1231, 32767,  976 };
static const camzone_t CAM_AT4 = { 10, 6464,  -32768, 32767,  6464,  8,   7039,  319,  -32768, -32768 };
static const killbox_t KILL_AT1[] = { { 7579, 7788, 732, 1129, 1, 0 }, { 7790, 8050, 929, 1133, 2, 0 } };
static const killbox_t KILL_AT2[] = { { 5260, 5467, 379, 814, 0, 1 } };
/* Aqua Tunnel's sound group (D-047: water, jets, slimes, bombs, bosses) */
/* Relic Maze flow (RE spec_rm_flow 1-2, 6.1). RM1's right edge: the middle band (260 < MV.Y < 420, 5813, behind the barriers
 * opened by stage flags 0x2 / 0x4) goes on to RM3, any other height (5814) into the side area RM2, which leaves by its left
 * edge (5916) back to RM1's right edge (2967+: X 7400, Y 190 / 540 by flag 0x1, facing left, B -3 for 32 frames, 5827).
 * RM2 is entered at X 48 (5921) at y 381 or 1466 (5600 + start point 1450 when flag 0x1); RM3 at X 48 (B 3); RM5 at X 340. */
static const exit_t EX_RM1[] = { { 1, 2, -50, 260, 420, "rm3" }, { 1, 2, -50, 0, 0, "rm2" } };
static const exit_t EX_RM2[] = { { -1, -2, 80, 0, 0, "rm1" } };
static const exit_t EX_RM3[] = { { 1, 0, 20, 0, 0, "rm4" } };
static const entry_t EN_RM1[] = { { "rm2", 7400, 190, 350, -1, -3 } };
static const entry_t EN_RM2[] = { { NULL, 48, 381, 1466 - 381, 1, 0 } };
static const entry_t EN_RM3[] = { { NULL, 48, 509, 0, 1, 3 } };
static const entry_t EN_RM5[] = { { NULL, 340, 549, 0, 1, 0 } };
static const cpzone_t CP_RM1[] = { { 6777, 7372, 2, 6360, 256, 0, 0 } };
static const cpzone_t CP_RM2[] = { { 100, 32767, 2, 6360, 256, 0, 1 } };       /* 5924: RM1's checkpoint, no snapshot */
static const cpzone_t CP_RM3[] = { { 7400, 8360, 10, 8352, 2669, 2000, 0 } };     /* 6296 -> loop_next_act_26 (Y > 2000) */
static const cpzone_t CP_RM4[] = { { 4330, 5330, 2, 4330, 830, 0, 0 } };
static const killbox_t KILL_RM2[] = { { 2915, 3204, 281, 584, 3, 0 }, { 3219, 3265, 327, 621, 4, 0 } };   /* 6013 / 6014 */
/* RM1 / RM2 gimmicks (spec_rm_gimmicks_a): dig zones 5820 and rewards 5821-5824 (spot A: newest first), RM2 jump pads
 * (5169, 4951, 5171, 5172, 5892, 5893), the stage-flag gates (10), the start clean-ups 6039 / 6040 and the basement
 * break-out zone 5925 */
static const zone_t DIGZ_RM1[] = { { -12, 1186, 966, 1173 }, { 3455, 4489, 692, 1009 }, { 4416, 5362, 523, 840 }, { 2305, 2357, 1091, 1149 },
                                   { 1207, 1620, 1109, 1226 }, { 6539, 6653, 487, 620 } };
static const signed char DIGR_RM1[] = { DIG_EARTH_MON, DIG_CRYSTALS, DIG_OPEN_HOLE, DIG_KEYSTONE_MON };
static const padcfg_t PADS_RM2[] = { {3264,432,0,90,0,1,0}, {3264,224,1,90,0,0,0}, {3264,32,2,90,1,0,0}, {800,608,3,0,0,1,0}, {664,384,4,270,0,0,0}, {992,40,5,180,1,0,0} };
/* RM3 jump pads (spread index A newest first: pad 0 = the last in the frame JSON). 0-7 on the Claw corridor floor (y 512:
 * Carol's scripted rides 6242-6245, src/game/rm_claw.c); 6096-6101: pad 8 D 1 B 270, pads with x > 10400 anim 7 (block),
 * pad 9 E 1, pad 11 B 270, pad 12 B 150, pad 13 D 1 B 270 (spec_rm_gimmicks_b 18) */
static const padcfg_t PADS_RM3[] = { {1120,512,0,0,0,0,0}, {1488,512,1,0,0,0,0}, {1856,512,2,0,0,0,0}, {2224,512,3,0,0,0,0},
    {2592,512,4,0,0,0,0}, {2960,512,5,0,0,0,0}, {3336,512,6,0,0,0,0}, {3656,512,7,0,0,0,0}, {3800,320,8,270,1,0,0},
    {11256,2984,9,0,0,1,1}, {11080,2840,10,0,0,0,1}, {10840,3000,11,270,0,0,1}, {10944,2792,12,150,0,0,1}, {10576,2872,13,270,1,0,1} };
static const gate_t GATES_RM1[] = { { 0x2, OT_WR_RAIL, 0, 0, 0, 0, 0 }, { 0x4, OT_RAIL2, 0, 0, 0, 0, 0 } };   /* 5832 / 5833 */
static const gate_t GATES_RM2[] = { { 0x2, OT_WR_RAIL, 116, 336, -32768, 1000, 1 }, { 0x4, OT_RAIL2, 116, 1424, 1000, 32767, 1 },   /* 5922 / 5923 */
                                    { 0x4, OT_RAIL2, 0, 0, 0, 0, 0 } };                                                      /* 5926 */
static const flagclear_t CLEAR_RM2[] = { { 0x2, 0, 3499, 0, 1199 }, { 0x4, 22, 1937, 1600, 3189 } };                        /* 6039 / 6040 */
static const zone_t BREAKOUT_RM2 = { 587, 824, 2328, 2566 };
static const camroom_t ROOMS_RM5[] = { { -32768, 900, 840, 1096, 768, 1480, -1 }, { -32768, 32767, 1095, 1608, 896, 1480, 1 },   /* 6745 / 6746, 6747 (a railing left) */
                                        { -32768, 32767, 1095, 1608, 1280, 1480, 0 }, { -32768, 32767, 1608, 32767, 1280, 2888, -1 } };   /* 6748, 6750 */
static const warp_t WARPS_RM5[] = { { -32768, 79, -32768, 1096, -1, 1024, 1816, -1, -3, -3, 704 },   /* 6731 + 6734 */
                                    { -32768, 79, 1095, 32767, -1, 704, 920, -1, -3, -3, 704 },      /* 6731 + 6735 */
                                    { 654, 900, 840, 1096, 1, 0, 1432, 1, 3, -2, 0 },                 /* 6740 (the fade trigger for a right exit INFERRED: B > 0) */
                                    { 974, 32767, 1608, 1864, 1, 0, 920, 1, 3, -2, 0 } };             /* 6743 */
static const short SFX_RM[] = { 54, 55, 59, 60, 62, 67, 68, 70, 73, 75, 77, 96, 99, 101, 110, 111, 135, 149, 213, 219, 260, 262, 306, 312, 426,   /* not in the global set (D-047): enemies, gimmicks, flow */
                                132, 134, 146, 148, 151, 152 };   /* RM5 Mantalith (rm_mantalith.c), 11025 Hz: 147 / 214 / 215 left out (sound RAM) */
/* Relic Maze acts 3-5 gimmicks (relic_gimb.c, spec_rm_gimmicks_b): moving-pillar zones (RE 6379-6385 / 6527-6531: box origin
 * in [x0, x1) x [y0, y1) -> travel C), jump pads (6096-6101 / 6429 / 6430 / 6570-6576; index = spread index, newest first;
 * RM3 pads 0-7 belong to the Claw chase, pads with x > 10400 show anim 7; RM4: no lit start pad, U8), door return, basement */
#define N_(a) (int)(sizeof a / sizeof a[0])
static const pillarzone_t PIL_RM3[] = { { 10811, 10905, 2245, 2736, -116 }, { 10905, 10955, 2248, 2739, -100 }, { 10953, 11003, 2246, 2737, -84 },
    { 11000, 11050, 2246, 2737, -68 }, { 11354, 11404, 2230, 2721, 136 }, { 11403, 11453, 2230, 2721, 88 }, { 11452, 11502, 2228, 2719, 40 } };
static const pillarzone_t PIL_RM4[] = { { 4411, 4458, 658, 894, 144 }, { 4459, 4506, 658, 894, 128 }, { 4508, 4555, 660, 896, 112 },
    { 4557, 4604, 659, 895, 96 }, { 4604, 4651, 661, 897, 80 } };
static const rmgim_t RMG_RM3 = { PIL_RM3, N_(PIL_RM3), 0, 1, 0 }, RMG_RM4 = { PIL_RM4, N_(PIL_RM4), 8, 1, 0 }, RMG_RM5 = { NULL, 0, 0, 1, 1608 };
static const padcfg_t PADS_RM4[] = { {368,632,0,0,0,0,0}, {496,392,1,0,0,0,0}, {560,632,2,0,1,0,0}, {1656,912,3,0,0,0,0}, {1472,688,4,270,1,0,0} };
/* Fortune Night pads (ObjectJumpPad_1529; spec_fn_gimmicks_a 16: A = spread newest first, RE 4950 / 7186-7196 (FN1), 4950 / 4951 / 7649 /
 * 4954 / 7650-7654 / 5365 / 7655-7663 (FN3)); FN2's only pad is an off-map template (inert) */
/* Sky Battalion pads (ObjectJumpPad_539; A = 3564's spread index, newest first, the off-map template (-104, 576) last; RE session 19b):
 * SB2 9309 (A 0: E 1, B 0), 5357 (A 1: B 180), 9310 (A 2: B 180, D 1); SB3 4950 (A 0: E 1), 5357, 9310, 5172 (A 3: E 1), 9816 (A 4: B 90,
 * D 1), 9817 (A 5: E 1), 9818 (A 6: B 270, D 1); SB4 5169 (A 0: E 1, B 90), 5170 (A 1: B 270), 9988 (A 2: B 90), 9989 (A 3: B 270, D 1) */
static const padcfg_t PADS_SB2[] = { {3984,656,0,0,0,1,0}, {4224,272,1,180,0,0,0}, {4496,272,2,180,1,0,0} };
static const padcfg_t PADS_SB3[] = { {7024,344,0,0,0,1,0}, {7200,176,1,180,0,0,0}, {7488,176,2,180,1,0,0}, {12000,1048,3,0,0,1,0},
    {12280,720,4,90,1,0,0}, {11664,1048,5,0,0,1,0}, {11392,720,6,270,1,0,0} };
static const padcfg_t PADS_SB4[] = { {9856,784,0,90,0,1,0}, {9744,600,1,270,0,0,0}, {10224,600,2,90,0,0,0}, {10112,784,3,270,1,0,0} };
static const padcfg_t PADS_FN1[] = { {3880,1328,0,0,0,1,0}, {3944,1008,1,90,1,0,0}, {3464,1032,2,0,0,1,0}, {3520,744,3,270,0,0,0}, {3584,440,4,270,1,0,0},
    {7528,1072,6,90,0,1,0}, {7208,952,7,180,1,0,0}, {10472,704,8,90,0,1,0}, {10472,416,9,90,0,0,0}, {10472,128,10,90,1,0,0},
    {7656,1168,11,270,0,1,0}, {7784,1016,12,270,0,0,0}, {7912,728,13,270,1,0,0} };
static const padcfg_t PADS_FN3[] = { {5568,784,0,0,0,1,0}, {5888,640,1,90,0,0,0}, {5632,640,2,270,1,0,0}, {7312,1088,4,0,0,1,0}, {7792,1088,5,0,1,0,0},
    {9504,800,6,0,0,1,0}, {9568,424,7,180,0,0,0}, {9984,464,8,90,0,0,0}, {9856,752,9,270,0,0,0}, {10176,712,10,180,0,0,0}, {10752,752,11,90,0,0,0},
    {10400,520,12,180,0,0,0}, {10496,800,13,0,0,0,0}, {10368,656,14,270,0,0,0}, {10720,616,15,180,0,0,0}, {11360,616,16,180,1,0,0},
    {10656,800,17,0,0,1,0}, {10656,616,18,180,0,0,0}, {11360,616,19,180,1,0,0} };
static const padcfg_t PADS_RM5[] = { {352,2664,0,0,0,0,0}, {624,2632,1,0,1,0,0}, {1176,520,2,270,0,1,0}, {1176,216,3,270,1,0,0} };

static const short SFX_AT[] = { 55, 62, 69, 99, 172, 173, 240, 242, 251, 252, 278, 306, 308, 316 };
/* Fortune Night flow (session 17, RE spec_fn_flow 1-3, 9; RV where the spec says so). Checkpoints (FN1 7366, FN2 / FN4 inline,
 * FN3 7818, FN5 8420 / 8421: the second one takes g4 to 3, so a restart there never re-arms the samurai 8422); FN4 / FN5 end
 * at X > W + 20 (8295, B > 0); FN3 ends by its Serpentine pass (fn_flow.c, 7710-7725); FN6 deaths go to FN5 with g4 2 */
static const cpzone_t CP_FN1[] = { { 7700, 11524, 2, 7700, 1500, 0, 0 } };
static const cpzone_t CP_FN2[] = { { 6520, 11524, 2, 6520, 490, 0, 0 } };
static const cpzone_t CP_FN3[] = { { 9800, 10000, 2, 9536, 360, 0, 0 } };
static const cpzone_t CP_FN4[] = { { 7670, 7770, 2, 7670, 840, 0, 0 } };
static const cpzone_t CP_FN5[] = { { 8430, 9430, 2, 8430, 608, 0, 0 }, { 10560, 11560, 3, 10880, 560, 0, 0 } };
static const exit_t EX_FN45[] = { { 1, 0, 20, 0, 0, NULL } };
/* FN3 start by character (8082 MV.A = 0 / 8083 MV.A != 0). The boss parts are the boss module's types (spec_fn_bosses Part X:
 * 821-825 Serpentine arm / hitbox / floor check / shield / charge glow, 840-843 samurai body / hands / wheel / floor probe);
 * Lilac also loses every MonitorBomb (FN3's 8 at y 336). Carol / Milla: the limit markers move around the samurai at
 * (11136, 496), R = 65: left 11136 - 240 - 65, right 11136 + 240 + 65, bottom 1424 (the top marker is at 0 already). */
static const classkill_t CK_FN3[] = { { 1, 840, 843 }, { 1, OT_MON_BOMB, OT_MON_BOMB }, { 2, 821, 825 } };
static const markmove_t CM_FN3[] = { { 2, 10831, -32768, 11441, 1424 } };
static const cambox_t BOX_FN6 = { 224, 0, 960, 311 };   /* 8553 / handle_frame_35 inline (RV at each frame start: C 224, D 0, E 960, F 311) */
/* the stage's own sounds (D-047), one group per act (session 17: one shared FN group was 420 KB; sound RAM needs >= 256 KB free
 * with the heaviest character, ASSET_PIPELINE). Ids from the modules' snd_play_orig calls and the act's object classes (json
 * frames 30-35): flow 134 (7713 Serpentine's crash; also the lamppost break) and 61 (7716; a Carol group id, needed by
 * Lilac / Milla); everywhere: 33 hang (Carol id), coil 164 / 371 / 167, lights 171; pigeons 243 (FN1-3), boost blocks 163
 * (FN1-2), teeter 143 (FN2-5), trolley 260 (FN2), gachapon 187 / 188 (FN3-5), fireworks / flares 430, Turretus 127 (FN2-5),
 * Milla cube reflect 219, shell 231 / gimmicks B 208-249 / 428 (FN4-5); bosses (fn_boss.c): UFO 168 / 169 (FN1, FN3 chase),
 * bulldozer 165 / 166 (FN1-2), Serpentine 62 / 70 / 75 / 191-200 / 317 (FN3). All 11025 Hz (convert_sfx.sh). */
static const short SFX_FN1[] = { 33, 134, 164, 371, 171, 167, 163, 430, 243, 165, 166, 168, 169 };
static const short SFX_FN2[] = { 33, 134, 164, 371, 171, 167, 143, 163, 260, 430, 243, 127, 219, 165, 166 };
static const short SFX_FN3[] = { 33, 134, 164, 371, 171, 167, 61, 143, 187, 188, 430, 243, 127, 219,
                                 62, 70, 75, 168, 169, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 317 };
static const short SFX_FN45[] = { 33, 134, 164, 371, 171, 167, 143, 187, 188, 430, 127, 219, 231,
                                  208, 212, 216, 217, 218, 223, 224, 225, 226, 227, 228, 229, 230, 249, 428,
                                  126, 193 };   /* + the FN5 Panther chase (fn_panther.c): 8413 white burst 126 (a Carol id), 8407 taunt 193 (242 KB) */
#define SFX_FN4 SFX_FN45
#define SFX_FN5 SFX_FN45
/* FN6: flow 134 + the Panther fight (fn_panther.c, spec_fn_bosses G.11): explosion 99, white burst 126, assist strike 137,
 * plasma 165 / 166 / 191 / 436, rapid fire 193 / 194, back cannon 195 / 254 / voices 258 (Lilac) / 259, missiles 169 / 200,
 * laser 256 / 257, defeat 261, Milla's platform 219 (228 KB at 11025 Hz) */
static const short SFX_FN6[] = { 134, 99, 126, 137, 165, 166, 169, 191, 193, 194, 195, 200, 219, 254, 256, 257, 258, 259, 261, 436, 241, 288 };   /* + 241 / 288: fn_assist.c (session 18) */
/* Fortune Night bosses (fn_boss.c, spec_fn_bosses C / E): FN3 beam chase (7794 window, 7797 / 7798 / 7810 zones, 7800 / 7801
 * floors at A >= 4, Floor2 pieces at MV.B) and samurai (7815 lock, 8040 / 8048 music, 8045 y 512, turrets 8047 / 8048); FN5
 * samurai (8422 g4 < 3, 8424 CAM.D 336, no music; 8473 turret; its 8045 copy's y is not in the spec: home y + 16, INFERRED) */
static const fnboss_t FNB_FN3 = { 2024, 2064, { { 2554, 3451, 553, 824 }, { 2559, 3458, 831, 1145 }, { 3454, 3844, 835, 1194 } }, 4, 1,
                                  1, 1, 0, 0, 512, { { 11550, 949, 24 }, { 11552, 852, 48 } } };
static const fnboss_t FNB_FN5 = { 0, 0, { { 0 } }, 5, 0, 0, 0, 336, 3, 192, { { 10931, 550, 48 }, { 0, 0, 0 } } };

#define DV(d, f, n, mus) .dir = d, .frame = f, .stage = 1, .act = n, .nacts = 4, .music = mus
#define AT(d, f, n, mus) .dir = d, .frame = f, .stage = 0, .act = n, .nacts = 4, .music = mus, .plate_rule = 1, .switch_late = 1, .monitor_hop = 1, .checkpoints = 1, .sfx = SFX_AT, .nsfx = N_(SFX_AT)
/* Fortune Night enemies (fn_enemy.c, spec_fn_enemies): FN1 7346 flips the coils below y 1500 and has no laser-wall handler
 * (no col_21_29 in test_collisions_29); FN2 has no flare-wall handler (col_24_30); FN5's balloon gives a life (8349) */
static const fnenemy_t FNE_FN1 = { 1500, 1, 0, 0 }, FNE_FN2 = { 0, 0, 1, 0 }, FNE_FN5 = { 0, 0, 0, 1 };
/* Fortune Night (session 17, spec_fn_flow 9.1): music slots 4 / 6 / 7 of the frame's loop table (tools/re/bgm_refs.py, 3) */
#define FN(d, f, n, mus) .dir = d, .frame = f, .stage = 3, .act = n, .nacts = 6, .music = mus, .checkpoints = 1, .sfx = SFX_FN##n, .nsfx = N_(SFX_FN##n)
#define FN_M123 .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3.adp", .mus7 = "bgm/m_stage3.adp"
#define FN_M45 .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_stage3b.adp", .mus7 = "bgm/m_stage3b.adp"
/* Sky Battalion (session 19, spec_sb_flow): checkpoints (SB2 9550 / 9551; SB3 9876 / 9877 and SB4 10076 / 10077 up to W - 100,
 * SB4's second with MV.Y > 830), the ships' right-edge exit back to SB1 (9455-9458, B > 0, g4 -1), the entries: SB1 from a
 * ship (8882: X -40, Y = start y + 16 - 400, B 15 while the fade runs; G -5 in sb_flow.c), a ship from SB1 (2955-2957: x -32,
 * start y + 16; no B event: the opening of sb_flow.c holds the player on the first hover platform) */
static const cpzone_t CP_SB2[] = { { 7192, 7392, 2, 7192, 616, 0, 0 }, { 10800, 10900, 3, 10872, 588, 0, 0 } };
static const cpzone_t CP_SB3[] = { { 5920, 14300, 2, 5920, 580, 0, 0 }, { 10100, 14300, 3, 10290, 710, 0, 0 } };
static const cpzone_t CP_SB4[] = { { 5980, 15524, 2, 5980, 266, 0, 0 }, { 12090, 15524, 3, 12090, 1160, 830, 0 } };
static const exit_t EX_SBSHIP[] = { { 1, 0, -50, 0, 0, "sb1" } };
static const entry_t EN_SB1[] = { { NULL, -40, 1189 + 16 - 400, 0, 1, 15 } };
static const entry_t EN_SBSHIP[] = { { NULL, -32, 469 + 16, 0, 1, 0 } };
/* stage sounds per act (11025 Hz; Media::play_id of the frames' own events, tools: spec_sb_flow 7): pads 62, planks 99, the
 * monitors' 172 / 173 / 278 / 308 / 316 (bombs, crates, shields: the AT ids, placed in SB2-SB4); enemies / gimmicks 54, 127,
 * 237, 264 (+ 265 / 371 SB3; 75 / 77 / 163 / 171 / 255 / 266 / 438 SB4); bosses: SB1 Kujacker 267-274, 371; SB2 chopper /
 * Spade 103, 156, 195, 446; SB3 satellite 165, 166, 169. The enemy / boss modules add their ids here. */
static const short SFX_SB1[] = { 62, 267, 268, 269, 270, 271, 272, 273, 274, 371, 99 };   /* + 99: the Kujacker's end (sb_kujacker.c) */
static const short SFX_SB2[] = { 62, 99, 54, 127, 237, 264, 103, 156, 195, 446, 172, 173, 278, 308, 316, 312, 263, 71, 155, 240, 445 };   /* + enemies / gimmicks: frag bomb 312, flamethrower 263; + Spade (sb_spade.c) 71 155 240 445 */
static const short SFX_SB3[] = { 62, 99, 54, 127, 237, 264, 265, 371, 165, 166, 169, 172, 173, 278, 308, 316, 312 };
static const short SFX_SB4[] = { 62, 99, 54, 127, 237, 264, 75, 77, 163, 171, 255, 266, 438, 172, 173, 278, 308, 316, 312 };
#define SB(d, f, n, mus) .dir = d, .frame = f, .stage = 4, .act = n, .nacts = 4, .music = mus, .checkpoints = 1, .sfx = SFX_SB##n, .nsfx = N_(SFX_SB##n)
#define SB_M34 .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage4.adp", .mus7 = "bgm/m_stage4.adp"
/* Jade Creek (session 21, stage index 5; spec_jc_flow, jc_flow.c): checkpoints JC2 10483 (7050 < X < 7250, MV.Y < 820), JC3 10655
 * (5300 < X < 5700, MV.Y > 800), JC4 10848 (4100 < X < 4400), JC5 11192 (9800 < X < 9999; its group 0x22af7 is the act-5 module's,
 * INFERRED on); JC1 has none (RE: no g4 writer in handle_frame_40). JC3's g4 10 comes from its boss (jc_flow_act3_cleared). */
static const cpzone_t CP_JC2[] = { { 7050, 7250, 2, 7080, 790, -820, 0 } };   /* ymin -820: MV.Y < 820 */
static const cpzone_t CP_JC3[] = { { 5300, 5700, 2, 5555, 1530, 800, 0 } };
static const cpzone_t CP_JC4[] = { { 4100, 4400, 2, 4184, 1640, 0, 0 } };
static const cpzone_t CP_JC5[] = { { 9800, 9999, 2, 9826, 780, 0, 0 } };
/* the stage's own sounds per act (11025 Hz): 62 jump pads (JC2-JC4), 99 explosions; the enemy / gimmick / boss modules add theirs */
/* Jade Creek stage groups (session 21; ids the JC modules play that are not resident, host FP_SID_MISS runs + the module
 * reports): enemies 60 99 101 192 233 275 314 (314 is Lilac's own: kept for her, loaded for Carol / Milla), gimmicks 208 426
 * 72 69, act 5 (jc_boss5.c + the FN Serpentine variant) 61 67 70 126 134 165 166 191 192 193 194 195 196 233 292 427 */
#define SFX_JC_ACTS 62, 99, 60, 101, 192, 233, 275, 314, 208, 426, 72, 69
static const short SFX_JC1[] = { SFX_JC_ACTS, 276 };                                            /* + the Hovercop fly-by (jc_boss.c) */
static const short SFX_JC2[] = { SFX_JC_ACTS, 277, 165, 166, 195, 77 };                          /* + the Hovercop chase */
static const short SFX_JC3[] = { SFX_JC_ACTS, 277, 165, 166, 195, 77, 296, 297, 298, 299, 315, 31, 33 };   /* + Neera (31 / 33: Lilac's / Carol's own, kept) */
static const short SFX_JC4[] = { SFX_JC_ACTS };
static const short SFX_JC5[] = { 62, 99, 61, 67, 70, 126, 134, 165, 166, 191, 192, 193, 194, 195, 196, 233, 292, 427,   /* (no frogs, streams or collapsing platforms in JC5: 314 208 426 left out, Milla sound RAM) */
                                 297, 77, 33, 60 };   /* session 22: Neera's ice ambush (jc_boss.c: 297 missile, 77 burst, 33 landing, Milla's), the BuoyBuoys past its gate (60; RV amb_l f558): ~183 KB */
/* Trap Hideout (session 21b, stage index 8; Carol / Milla only; spec_th_flow, th_flow.c). Checkpoints (RE): TH1 13501 (g4 < 2, MV.Y > 2208,
 * X < 2000 -> (2, 180, 2288); RV th1_bottom_carol), TH2 13690 + its inline copy in handle_frame_58 (7280 < X < 7480 -> (2, 7380, 570)),
 * TH3 13878 (5446 < X < 5646 -> (6, 5546, 1520)) and 13879 (11460 < X < 11560, MV.Y < 400 -> (7, 11500, 160)); TH3's g4 5 comes from
 * the Crystal Basher's end (13931, th_flow_basher_cleared). TH1's shaft camera (13498-13500, RV th1_start / th1_bottom: E 511 until
 * MV.Y > 2180, then D 2016, E 6100) */
static const cpzone_t CP_TH1[] = { { -32768, 2000, 2, 180, 2288, 2208, 0 } };
static const cpzone_t CP_TH2[] = { { 7280, 7480, 2, 7380, 570, 0, 0 } };
static const cpzone_t CP_TH3[] = { { 5446, 5646, 6, 5546, 1520, 0, 0 }, { 11460, 11560, 7, 11500, 160, -400, 0 } };   /* ymin -400: MV.Y < 400 */
static const camshaft_t SHAFT_TH1 = { 2180, 2016, 100 };
/* the stage's own sounds per act (11025 Hz): the flow plays none (RE: no Media::play_id in the flow events of handle_frame_57..59);
 * 62 jump pads (TH3), 99 explosions; the enemy / gimmick / boss modules add theirs */
/* (session 21b: the TH modules' ids that are not resident: enemies 314 151 167 371 / 71 232 240 335 219, traps 426 264 278;
 * bosses add theirs) */
static const short SFX_TH1[] = { 62, 99, 314, 336, 169 };   /* + the Laser Iris (th_boss_iris.c) */
static const short SFX_TH2[] = { 62, 99, 314, 151, 167, 371, 426, 264, 278 };
static const short SFX_TH3[] = { 62, 99, 314, 151, 167, 371, 426, 264, 278, 71, 232, 240, 335, 219,
                                  132, 74, 363, 61, 155, 156, 103, 446, 445 };   /* + the Crystal Basher, the arena, Spade */
/* Thermal Base (session 21c, stage index 6; spec_tb_flow, tb_flow.c). Checkpoints (RE, g4 / g5 / g6 + loop_cp_16): TB1 inline
 * handle_frame_45 (g4 < 2, 6823 < X < 7250 -> (2, 6932, 2026); g4 < 3, 9814 < X < 10014 -> (3, 9914, 1374)), TB2 inline handle_frame_46
 * (g4 < 2, 2000 < X < 2400, MV.Y < 2260 -> (2, 2120, 2222); g4 < 3, 6100 < X < 6300, MV.Y > 2900 -> (3, 6144, 3184)), TB3 12096 (g4 < 2,
 * 3048 < X < 3248, MV.Y > 3200 -> (2, 3333, 3440)), TB4 inline handle_frame_48 (g4 < 2, 2500 < X < 2600 -> (2, 2664, 2648); g4 < 3,
 * 2800 < X < 2900, MV.Y > 2500 -> (3, 2912, 2648)), TB5 12539-12541 (g4 < 2, 1700 < X < 2200, MV.Y > 3260 -> (2, 1855, 3096); g4 < 3,
 * 6540 < X < 6740 -> (3, 6880, 2774); g4 < 4, 10344 < X < 10544 -> (4, 10400, 2712)) and inline handle_frame_49 (g4 < 5, 14620 < X <
 * 14820 -> (4, 14720, 2328): nosnap bit 2); TB6 has none (no loop_cp_16 in handle_frame_50) */
static const cpzone_t CP_TB1[] = { { 6823, 7250, 2, 6932, 2026, 0, 0 }, { 9814, 10014, 3, 9914, 1374, 0, 0 } };
static const cpzone_t CP_TB2[] = { { 2000, 2400, 2, 2120, 2222, -2260, 0 }, { 6100, 6300, 3, 6144, 3184, 2900, 0 } };
static const cpzone_t CP_TB3[] = { { 3048, 3248, 2, 3333, 3440, 3200, 0 } };
static const cpzone_t CP_TB4[] = { { 2500, 2600, 2, 2664, 2648, 0, 0 }, { 2800, 2900, 3, 2912, 2648, 2500, 0 } };
static const cpzone_t CP_TB5[] = { { 1700, 2200, 2, 1855, 3096, 3260, 0 }, { 6540, 6740, 3, 6880, 2774, 0, 0 },
                                   { 10344, 10544, 4, 10400, 2712, 0, 0 }, { 14620, 14820, 4, 14720, 2328, 0, 2 } };
/* the stage's own sounds per act (11025 Hz): 62 jump pads, 99 explosions; the flow plays 49 (the lifts' stop, 11716 / 11719: TB1-TB3);
 * the enemy / gimmick / boss modules add theirs */
static const short SFX_TB1[] = { 62, 99, 49, 37, 59, 172, 173, 264, 279, 280, 282, 283, 285, 286, 437 };
static const short SFX_TB2[] = { 62, 99, 49, 35, 37, 38, 59, 73, 264, 279, 280, 282, 283, 285, 286, 302, 437 };
static const short SFX_TB3[] = { 62, 99, 49, 35, 37, 38, 52, 55, 59, 71, 73, 134, 173, 264, 279, 280, 282, 284, 285, 286, 302, 303, 304, 305, 309, 310, 437 };
static const short SFX_TB4[] = { 62, 99, 49, 37, 38, 59, 173, 264, 279, 280, 285, 286, 437 };
static const short SFX_TB5[] = { 62, 99, 49, 33, 35, 37, 51, 59, 73, 110, 264, 279, 280, 282, 285, 286, 302, 321, 437 };
static const short SFX_TB6[] = { 62, 99, 92, 251, 252, 264, 279, 280, 304, 305, 309 };
/* Pangu Lagoon (session 23, stage index 7, Lilac only; spec_pl_flow, pl_flow.c). Checkpoints (RE, g4 / g5 / g6 + loop_cp_16): PL1
 * inline handle_frame_51 (g4 < 2, 9200 < X < 9400 -> (2, 9264, 474)), PL2 inline handle_frame_52 (g4 < 2, 6936 < X < 7136, MV.Y < 444 ->
 * (2, 6936, 414)), PL3 none, PL4 inline handle_frame_54 (g4 < 2, 3322 < X < 3500, MV.Y > 1000 -> (2, 3322, 1410); g4 < 3, 6767 < X <
 * 6900 -> (2, 6767, 1100): the binary writes 2, so it re-arms while g4 < 3: nosnap bit 2), PL5 inline handle_frame_55 (g4 < 2, 5750 <
 * X < 6000 -> (2, 5864, 1588); g4 < 3, 7900 < X < 8000, MV.Y < 600 -> (3, 7980, 370)), PL6 13327 (g4 < 2, 2834 < X < 3000 -> (2, 2834,
 * 1550)); PL6's g4 3 (4480, 208) comes with the stone door into the arena (13236, pl_flow.c); 13328 (4000 < X < 300) never fires */
static const cpzone_t CP_PL1[] = { { 9200, 9400, 2, 9264, 474, 0, 0 } };
static const cpzone_t CP_PL2[] = { { 6936, 7136, 2, 6936, 414, -444, 0 } };   /* ymin -444: MV.Y < 444 */
static const cpzone_t CP_PL4[] = { { 3322, 3500, 2, 3322, 1410, 1000, 0 }, { 6767, 6900, 2, 6767, 1100, 0, 2 } };
static const cpzone_t CP_PL5[] = { { 5750, 6000, 2, 5864, 1588, 0, 0 }, { 7900, 8000, 3, 7980, 370, -600, 0 } };   /* ymin -600: MV.Y < 600 */
static const cpzone_t CP_PL6[] = { { 2834, 3000, 2, 2834, 1550, 0, 0 } };
/* PL3's stone-door room (13007 / 13008, RE): CAM.M 1 to the CameraLimitBoss markers once Player_326 is right of the left marker
 * (4976) and above the bottom one (279); CAM.M 2 once X <= left + 39 (pl_flow.c adds 13007's fadeout_music(6000)) */
static const camzone_t CAM_PL3 = { 0, 4976, -32768, 279, 4976, 8, 6247, 279, 4976 + 40, 32767 };
/* the stage's own sounds per act (11025 Hz): the flow events play none of their own (13228's 56 is the shared wipe); 99 explosions;
 * the enemy / gimmick / boss modules add theirs */
static const short SFX_PL1[] = { 99, 10, 50, 323, 38, 311, 319, 320, 44, 7, 174, 288, 322, 324 };
static const short SFX_PL2[] = { 99, 10, 50, 323, 38, 311, 319, 320, 44, 147, 51, 24, 33, 325, 326, 7, 174, 288, 13, 322, 324 };
static const short SFX_PL3[] = { 99, 10, 50, 323, 38, 311, 319, 320, 44, 33, 325, 326, 7, 174, 288, 62, 132, 13, 322, 324 };
static const short SFX_PL4[] = { 99, 10, 50, 323, 38, 311, 319, 320, 147, 51, 327, 29, 328, 329, 33, 325, 326, 7, 174, 288, 322, 324 };
static const short SFX_PL5[] = { 99, 10, 50, 323, 38, 311, 319, 320, 147, 51, 327, 29, 44, 328, 329, 150, 33, 325, 326, 322, 324 };
static const short SFX_PL6[] = { 99, 147, 51, 38, 311, 319, 320, 150, 62, 132, 7, 174, 288, 322, 324, 333, 330, 331, 332, 200, 169, 45, 334, 76, 29, 23, 24, 25 };
/* Battle Glacier (session 24, stage index 9 = the "STAGE 8" card for everyone; spec_bg_flow, bg_flow.c). Checkpoints (RE, g4 / g5 / g6 +
 * loop_cp_16, all inline in handle_frame_60..64): BG1 (g4 < 2, 4438 < X < 4538 -> (2, 4438, 900); g4 < 3, 9400 < X < 9500 -> (3, 9458,
 * 620)), BG2 (g4 < 2, 6040 < X < 6200 -> (2, 6150, 96)), BG3 (g4 < 2, 6044 < X < 6200 -> (2, 6138, 420)), BG4 none of its own (14502:
 * the opening Brevon mech's end -> (5, 444, 1800), bg_flow_opening_cleared), BG5 (g4 < 1, 2736 < X < 2936, MV.Y > 1000 -> (1, 2836,
 * 1260): only before the walk-in's g4 1; g4 < 2, 5000 < X < 5200 -> (2, 5000, 720); g4 < 3, 7450 < X < 7650 -> (3, 7550, 400); g4 < 4,
 * 8100 < X < 8400, MV.Y > 1600 -> (4, 8260, 1870)), BG6 none (no loop_cp_16 in handle_frame_65) */
static const cpzone_t CP_BG1[] = { { 4438, 4538, 2, 4438, 900, 0, 0 }, { 9400, 9500, 3, 9458, 620, 0, 0 } };
static const cpzone_t CP_BG2[] = { { 6040, 6200, 2, 6150, 96, 0, 0 } };
static const cpzone_t CP_BG3[] = { { 6044, 6200, 2, 6138, 420, 0, 0 } };
static const cpzone_t CP_BG5[] = { { 2736, 2936, 1, 2836, 1260, 1000, 0 }, { 5000, 5200, 2, 5000, 720, 0, 0 },
                                   { 7450, 7650, 3, 7550, 400, 0, 0 }, { 8100, 8400, 4, 8260, 1870, 1600, 0 } };
/* Milla's dig in BG1 / BG2 (RE handle_frame_60 inline / 14165: MV.L = 1 -> every MILLA.D = 1, i.e. anywhere on the ground; 3313 clears it
 * each frame): one zone over the whole frame. BG3-BG6 run 3313 and the spot events but no D writer (RE by absence): no dig there. Rewards
 * by spot index A (newest first; 5821 A 0 MonitorEarthShield, 5822 A 1 crystals x5, 14166 A 2 crystals x5, 5824 A 3 MonitorKeystone +
 * crystals x2; BG2's spots 4 / 5 have none) */
static const zone_t DIGZ_BG_ALL[] = { { -32768, 32767, -32768, 32767 } };
static const signed char DIGR_BG[] = { DIG_EARTH_MON, DIG_CRYSTALS, DIG_CRYSTALS, DIG_KEYSTONE_MON };
/* the stage's own sounds per act (11025 Hz): the flow events play none of their own (RE: the Media calls of the BG flow events are the shared
 * pause / weapon / debris ones); 62 jump pads, 99 explosions; the enemy / gimmick / boss modules add theirs */
static const short SFX_BG1[] = { 62, 99 };
static const short SFX_BG2[] = { 62, 99 };
static const short SFX_BG3[] = { 62, 99 };
static const short SFX_BG4[] = { 62, 99 };
static const short SFX_BG5[] = { 62, 99 };
static const short SFX_BG6[] = { 62, 99 };
#define BG(d, f, n, mus) .dir = d, .frame = f, .stage = 9, .act = n, .nacts = 6, .music = mus, .checkpoints = 1, .sfx = SFX_BG##n, .nsfx = N_(SFX_BG##n)
/* Final Dreadnought (session 24, stages 10-13 = rounds 1-4, two levels each; spec_fd_flow, fd_flow.c). Checkpoints (RE, g4 / g5 / g6 +
 * loop_cp_16, all inline in handle_frame_66 / 68 / 70 / 72; the b frames have none): fd1a (g4 < 2, 4856 < X < 4956 -> (2, 4856, 880);
 * g4 < 3, 8728 < X < 8928, MV.Y > 1380 -> (3, 8728, 1512); g4 < 4, 14112 < X < 14212 -> (4, 14112, 1616)), fd2a (g4 < 2, 2980 < X < 3080,
 * MV.Y < 1420 -> (2, 3000, 1390); g4 < 3, 3964 < X < 4090, MV.Y > 1900 -> (3, 3964, 2030)), fd3a (g4 < 2, 3874 < X < 3974, MV.Y > 920 ->
 * (2, 3874, 1090); g4 < 3, 7500 < X < 7684 -> (3, 7584, 1080); g4 < 4, 10000 < X < 10364, MV.Y < 504 -> (4, 10208, 480)), fd4a (g4 < 2,
 * X > 2300, MV.Y < 2960 -> (2, 48, 2960); g4 < 3, X > 2300, MV.Y < 1224 -> (3, 208, 1224); g4 < 4, 2300 < X < 4300, MV.Y > 4112 -> (4,
 * 2352, 6352); g4 < 5, 2300 < X < 4300, MV.Y < 4112 -> (5, 2472, 4112); g4 < 6, 2300 < X < 4300, MV.Y < 2032 -> (5, 2472, 2032): the
 * binary writes 5, so it re-arms while g4 < 6: nosnap bit 2) */
static const cpzone_t CP_FD1A[] = { { 4856, 4956, 2, 4856, 880, 0, 0 }, { 8728, 8928, 3, 8728, 1512, 1380, 0 }, { 14112, 14212, 4, 14112, 1616, 0, 0 } };
static const cpzone_t CP_FD2A[] = { { 2980, 3080, 2, 3000, 1390, -1420, 0 }, { 3964, 4090, 3, 3964, 2030, 1900, 0 } };
static const cpzone_t CP_FD3A[] = { { 3874, 3974, 2, 3874, 1090, 920, 0 }, { 7500, 7684, 3, 7584, 1080, 0, 0 }, { 10000, 10364, 4, 10208, 480, -504, 0 } };
static const cpzone_t CP_FD4A[] = { { 2300, 32767, 2, 48, 2960, -2960, 0 }, { 2300, 32767, 3, 208, 1224, -1224, 0 }, { 2300, 4300, 4, 2352, 6352, 4112, 0 },
                                    { 2300, 4300, 5, 2472, 4112, -4112, 0 }, { 2300, 4300, 5, 2472, 2032, -2032, 2 } };
static const exit_t EX_FD2A[] = { { 1, 0, -50, 1100, 32767, NULL } };   /* 15652: X > W - 50, B > 0 and MV.Y > 1100 */
/* the stage's own sounds per act (11025 Hz): the flow plays fd1a's entry (98 / 99 / 311 / 354, fd_flow.c); 62 jump pads,
 * 99 explosions; the enemy / gimmick / boss modules add theirs */
static const short SFX_FD1A[] = { 62, 99, 98, 311, 354 };   /* + fd1a's entry: 98 / 99 / 311 (15298), 354 (15300) */ static const short SFX_FD1B[] = { 62, 99 };
static const short SFX_FD2A[] = { 62, 99 }; static const short SFX_FD2B[] = { 62, 99 };
static const short SFX_FD3A[] = { 62, 99 }; static const short SFX_FD3B[] = { 62, 99 };
static const short SFX_FD4A[] = { 62, 99 }; static const short SFX_FD4B[] = { 62, 99 };
#define FD(d, f, st, n, sx, mus) .dir = d, .frame = f, .stage = st, .act = n, .nacts = 2, .music = mus, .checkpoints = 1, .sfx = sx, .nsfx = N_(sx)
#define PL(d, f, n, mus) .dir = d, .frame = f, .stage = 7, .act = n, .nacts = 6, .music = mus, .checkpoints = 1, .sfx = SFX_PL##n, .nsfx = N_(SFX_PL##n), .lilac_only = 1
#define TB(d, f, n, mus) .dir = d, .frame = f, .stage = 6, .act = n, .nacts = 6, .music = mus, .checkpoints = 1, .sfx = SFX_TB##n, .nsfx = N_(SFX_TB##n)
#define TH(d, f, n, mus) .dir = d, .frame = f, .stage = 8, .act = n, .nacts = 3, .music = mus, .checkpoints = 1, .sfx = SFX_TH##n, .nsfx = N_(SFX_TH##n)
#define JC(d, f, n, mus) .dir = d, .frame = f, .stage = 5, .act = n, .nacts = 5, .music = mus, .checkpoints = 1, .sfx = SFX_JC##n, .nsfx = N_(SFX_JC##n)
#define RM(d, f, n, mus) .dir = d, .frame = f, .stage = 2, .act = n, .nacts = 5, .music = mus, .checkpoints = 1, .sfx = SFX_RM, .nsfx = N_(SFX_RM)
static const level_def_t LEVELS[] = {
    /* Dragon Valley: frame index 20-23 (RE loop_play_music_N_20..23), water DV2 / DV3 (5109 / 5314), DV4 results at the
     * right edge (5381) as well as after the boss */
    { DV("dv1", 20, 1, "bgm/m_stage1.adp"), .card_first = 1 },
    { DV("dv2", 21, 2, "bgm/m_stage1b.adp"), .card_first = 5, .door_skip_ach = 18, .water = 2700, .pads = PADS_DV2, .npads = N_(PADS_DV2) },
    { DV("dv3", 22, 3, "bgm/m_stage1b.adp"), .card_first = 7, .water = 2700, .pads = PADS_DV3, .npads = N_(PADS_DV3) },
    { DV("dv4", 23, 4, "bgm/m_stage1b.adp"), .card_first = 8, .edge_results = 1, .pads = PADS_DV4, .npads = N_(PADS_DV4) },
    /* Aqua Tunnel: frame index 16-19 (tools/re/bgm_refs.py: 16 m_stage0, 17/18 m_stage0b; AT2 after the Chomper (g4 >=
     * 10, 4330) m_stage0b), water 1200 / 1200 / none / 1480 (4309 / 4478 / 4530 / 4685), AT2 walk-in B = 0 (4403), AT3
     * ends at the top (4521: MV.Y < 380), AT4 results through the boss only (4652) */
    { AT("at1", 16, 1, "bgm/m_stage0.adp"), .card_first = 21, .water = 1200, .axel2_top = 320, .dig_zones = DIGZ_AT1, .ndig_zones = N_(DIGZ_AT1), .dig_rewards = DIGR_AT1, .ndig_rewards = N_(DIGR_AT1),
      .start_fall = 1, .cps = CP_AT1, .ncps = N_(CP_AT1), .boss_cam = &CAM_AT1, .kills = KILL_AT1, .nkills = N_(KILL_AT1), .cam_right = 12480 + 213 },
    { AT("at2", 17, 2, "bgm/m_stage0.adp"), .card_first = 24, .walkin_still = 1, .music_cp = "bgm/m_stage0b.adp", .water = 1200,
      .cps = CP_AT2, .ncps = N_(CP_AT2), .boss_cam = &CAM_AT2, .kills = KILL_AT2, .nkills = N_(KILL_AT2), .cam_cp10_left = 6144 - 213, .cam_cp10_pad = 50 },
    { AT("at3", 18, 3, "bgm/m_stage0b.adp"), .card_first = 27, .exit_max_y = 380, .pipe_auto_y = 2900, .cam_right = 856 + 213 },
    { AT("at4", 19, 4, "bgm/m_stage0b.adp"), .card_first = 28, .water = 1480, .cps = CP_AT4, .ncps = N_(CP_AT4), .boss_cam = &CAM_AT4, .cam_right = 7424 + 213 },
    /* Relic Maze: frame index 24-28 (spec_rm_flow; music m_stage2, RM3 m_stage2b after its g4 = 10 checkpoint, RM4-RM5
     * m_stage2b), cards 11 / 14 / 17 / 18 / 20, no water. RM5 ends with the Mantalith (rm_mantalith.c, spec_rm_mantalith):
     * the boss starts the results (7083) and its post-results script ends the stage (7097). */
    { RM("rm1", 24, 1, "bgm/m_stage2.adp"), .card_first = 11, .exits = EX_RM1, .nexits = N_(EX_RM1), .entries = EN_RM1, .nentries = N_(EN_RM1),
      .cps = CP_RM1, .ncps = N_(CP_RM1), .flag1_y = 340, .crystals_once = 1, .wplate_cyclone = 1, .walkin_purge = 1, .gates = GATES_RM1, .ngates = N_(GATES_RM1),
      .dig_zones = DIGZ_RM1, .ndig_zones = N_(DIGZ_RM1), .dig_rewards = DIGR_RM1, .ndig_rewards = N_(DIGR_RM1) },
    { RM("rm2", 25, 2, "bgm/m_stage2.adp"), .card_first = 14, .exits = EX_RM2, .nexits = N_(EX_RM2), .entries = EN_RM2, .nentries = N_(EN_RM2),
      .cps = CP_RM2, .ncps = N_(CP_RM2), .death_to = "rm1", .kills = KILL_RM2, .nkills = N_(KILL_RM2), .plate_rule = 2, .pads = PADS_RM2, .npads = N_(PADS_RM2),
      .gates = GATES_RM2, .ngates = N_(GATES_RM2), .flagclears = CLEAR_RM2, .nflagclears = N_(CLEAR_RM2), .breakout = &BREAKOUT_RM2, .breakout_flag = 0x4 },
    { RM("rm3", 26, 3, "bgm/m_stage2.adp"), .card_first = 17, .music_cp = "bgm/m_stage2b.adp", .exits = EX_RM3, .nexits = N_(EX_RM3),
      .entries = EN_RM3, .nentries = N_(EN_RM3), .cps = CP_RM3, .ncps = N_(CP_RM3), .cam_right = 12288 + 50,
      .rmg = &RMG_RM3, .pads = PADS_RM3, .npads = N_(PADS_RM3), .nobike = 1 },
    { RM("rm4", 27, 4, "bgm/m_stage2b.adp"), .card_first = 18, .cps = CP_RM4, .ncps = N_(CP_RM4), .rmg = &RMG_RM4, .pads = PADS_RM4, .npads = N_(PADS_RM4) },
    { RM("rm5", 28, 5, "bgm/m_stage2b.adp"), .card_first = 20, .entries = EN_RM5, .nentries = N_(EN_RM5), .cam_right = 9600 + 50,
      .rmg = &RMG_RM5, .pads = PADS_RM5, .npads = N_(PADS_RM5), .rooms = ROOMS_RM5, .nrooms = N_(ROOMS_RM5), .warps = WARPS_RM5, .nwarps = N_(WARPS_RM5) },
    /* Fortune Night: frame index 29-34 (session 17, spec_fn_flow; RV: g19 = 2, CAM.E = sign.x + 213 = 12205 / 15133 / 15501).
     * FN1 always starts with the card (4307); FN2-FN5 walk in from x -32 at B 3 (4526 / 5047, RV FN3 / FN4); FN6 walks in
     * clamped to X >= 400 without a B event (8550: its floor / plane modes move the player, RV) inside a fixed camera box;
     * FN4 always restarts slot 5 (8326: the load-time music_play does, a different track). Cards 31-40 (RE counter starts). */
    { FN("fn1", 29, 1, "bgm/m_stage3.adp"), FN_M123, .fne = &FNE_FN1, .pads = PADS_FN1, .npads = N_(PADS_FN1), .card_first = 31, .start_card = 1, .cps = CP_FN1, .ncps = N_(CP_FN1), .cam_right = 11992 + 213 },
    { FN("fn2", 30, 2, "bgm/m_stage3.adp"), FN_M123, .fne = &FNE_FN2, .card_first = 33, .cps = CP_FN2, .ncps = N_(CP_FN2), .cam_right = 11992 + 213 },
    { FN("fn3", 31, 3, "bgm/m_stage3.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3c.adp", .mus7 = "bgm/m_trouble.adp", .card_first = 36, .pads = PADS_FN3, .npads = N_(PADS_FN3),
      .cps = CP_FN3, .ncps = N_(CP_FN3), .cam_right = 14920 + 213, .script_exit = 1, .fnb = &FNB_FN3, .ckills = CK_FN3, .nckills = N_(CK_FN3), .cmarks = CM_FN3, .ncmarks = N_(CM_FN3) },
    { FN("fn4", 32, 4, "bgm/m_stage3b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3b.adp", .mus7 = "bgm/m_stage3b.adp", .card_first = 37, .wplate_cyclone = 1,   /* 5677: the escalator plate takes M 26 too (fn_gimb.c) */
      .cps = CP_FN4, .ncps = N_(CP_FN4), .exits = EX_FN45, .nexits = N_(EX_FN45), .cam_right = 11992 + 213 },
    { FN("fn5", 33, 5, "bgm/m_stage3b.adp"), FN_M45, .fne = &FNE_FN5, .fnb = &FNB_FN5, .wplate_cyclone = 1, .fnb_disco = 1, .card_first = 39, .cps = CP_FN5, .ncps = N_(CP_FN5), .exits = EX_FN45, .nexits = N_(EX_FN45), .cam_right = 15288 + 213 },
    { FN("fn6", 34, 6, "bgm/m_boss.adp"), .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_boss.adp", .mus7 = "bgm/m_boss.adp", .walkin_minx = 400, .walkin_free = 1,
      .cam_box = &BOX_FN6, .death_to = "fn5", .death_g4 = 2, .res_voice = "teamlilac_madeit", .res_wipe = 1 },
    /* Sky Battalion: frame index 35-38 (session 19, spec_sb_flow; sb_flow.c): sb1 = json 36 = Zao's airship, the hub. A fresh
     * start (g4 0 -> 1, 5839) shows the card; its pads send the player to the ships (g4 -1, 8876-8878, the free fade); the
     * ships end at their right edge back to sb1 (9455-9458), where she flies in from x -40, 400 px up, B 15, G -5 (8882 /
     * 8883, 5047). The Kujacker and the results are in sb1 (results wipe 8873-8881). Cards 41-50 (counters 9763 / inline:
     * 41, 44, 48; sb1 has none). Music per frame loop table (tools/re/bgm_refs.py + the wrappers): sb1 4 m_boss3, 5 / 7
     * m_stage4, 6 m_menu2; sb2 4 m_midboss, 5 / 6 m_stage4, 7 m_trouble; sb3 / sb4 4 m_midboss, 5-7 m_stage4. Deaths
     * restart the act (1990, g4 kept). Checkpoints 9550 / 9551, 9876 / 9877, 10076 / 10077 (X < W - 100; SB4's second
     * needs MV.Y > 830). No 2860 in any SB frame: the right camera bound is the frame width. */
    /* session 21: boss voices (RE 9012-9022 / 9163 Kujacker, 9625-9628 Spade; channel 24 / 27) */
    #define VOX_SB1 (const char *const[]){ "da_04_01", "da_04_02", "da_04_03", "da_04_04", "lilac_never", "carol_fatchance", "milla_icantiwont" }
    #define VOX_SB2 (const char *const[]){ "spade_comeon", "spade_goodenough", "spade_comeon2", "lilac_hitmeifyoucan" }
    { SB("sb1", 35, 1, "bgm/m_stage4.adp"), .mus4 = "bgm/m_boss3.adp", .mus6 = "bgm/m_menu2.adp", .mus7 = "bgm/m_stage4.adp", .start_card = 1,
      .entries = EN_SB1, .nentries = N_(EN_SB1), .clear_free = 1, .res_wipe = 1,
      .vox = VOX_SB1, .nvox = 7, .res_skin_voice = 1 },
    { SB("sb2", 36, 2, "bgm/m_stage4.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage4.adp", .mus7 = "bgm/m_trouble.adp", .card_first = 41,
      .cps = CP_SB2, .ncps = N_(CP_SB2), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB2, .npads = N_(PADS_SB2),
      .vox = VOX_SB2, .nvox = 4 },
    { SB("sb3", 37, 3, "bgm/m_stage4.adp"), SB_M34, .card_first = 44,
      .cps = CP_SB3, .ncps = N_(CP_SB3), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB3, .npads = N_(PADS_SB3) },
    { SB("sb4", 38, 4, "bgm/m_stage4.adp"), SB_M34, .card_first = 48,
      .cps = CP_SB4, .ncps = N_(CP_SB4), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB4, .npads = N_(PADS_SB4) },
    /* Jade Creek: frame index 39-43 (session 21, spec_jc_flow; jc_flow.c). Music tables (RE loop wrappers): slot 4 m_midboss
     * (loop_play_music_4_13) in JC1-JC4, JC5 m_boss (10871; 10872 m_boss6 only for g1 = 5); slot 5 m_stage5 (JC1 / JC2, JC3
     * before its boss: 10581 g4 < 10), m_stage5b (JC3 g4 >= 10: 10582 / loop_next_act_41; JC4 10580; JC5 loop_play_music_5_42);
     * slots 6 / 7 m_stage5. Water ML.B 1200 / 1600 (10378 / 10566 / 10822 / 10863 / 11643). Cards (ObjectCodec A newest first):
     * 51 (JC1 x3), 54 (JC2 x3), 57 (JC3), 58 (JC4 x3); JC5 has none. JC1 starts with the card (4307); the others walk in
     * (4490 / 4526 / 10482). JC1-JC4 end at the right edge (4135); camera E = Sign x + 213 (4241 / 4242: Signs 13480, 14784,
     * 9624, 10240), JC3 after its boss C = Sign x - 213, E = W + 50. JC5: the boss's results and the wipe (8547-8549). */
    { JC("jc1", 39, 1, "bgm/m_stage5.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 51, .start_card = 1, .water = 1200, .cam_right = 13480 + 213 },
    { JC("jc2", 40, 2, "bgm/m_stage5.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 54, .water = 1600, .cps = CP_JC2, .ncps = N_(CP_JC2), .cam_right = 14784 + 213 },
    { JC("jc3", 41, 3, "bgm/m_stage5.adp"), .music_cp = "bgm/m_stage5b.adp", .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 57, .water = 1600, .cps = CP_JC3, .ncps = N_(CP_JC3), .cam_right = 9624 + 213, .cam_cp10_left = 9624 - 213, .cam_cp10_pad = 50 },
    { JC("jc4", 42, 4, "bgm/m_stage5b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 58, .water = 1600, .cps = CP_JC4, .ncps = N_(CP_JC4), .cam_right = 10240 + 213 },
    { JC("jc5", 43, 5, "bgm/m_stage5b.adp"), .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .water = 1600, .cps = CP_JC5, .ncps = N_(CP_JC5), .res_wipe = 1,
      .vox = (const char *const[]){ "serp_gotyounow", "serp_kungfu" }, .nvox = 2 },
    /* Trap Hideout: frame index 56-58 (session 21b, spec_th_flow; th_flow.c). Carol / Milla only (Classic: JC's end routes g1 != 0 here,
     * handle_frame_9 inline; 13536 / the inline copy in on_frame_57_start turn g1 0 into 2). Music (RE loop wrappers): slots 5-7
     * m_stage7 (loop_play_music_5_56) in every act, slot 4 m_midboss (loop_play_music_4_13; TH3 13701 while g4 < 5, 13702 m_boss2 from
     * g4 5). Cards (ObjectCodec A newest first, RE start events): 71 (TH1), 72 (TH2), 76 (TH3) = cards 71-80; ML.Q 38 / 39 / 40.
     * ML.A 1, ML.B 2000 in every act but no water events (4150-4170) in handle_frame_57..59: dry (RE; RV th1_bottom OTHERS.H 0).
     * TH1 starts with the card (4307) at the start point + 16 (269, -19), held there until the card fade's B reaches 96 (13533,
     * th_flow_card), then falls down the shaft; TH1 / TH2 end at the right edge (4135 -> 3578); TH2 / TH3 are entered by the
     * walk-in (4526; RV th2_start / th3_start: B 3 for 30 frames, g4 1); TH3 ends with its boss's results and the wipe (4560-4586,
     * 5394, 8547 / 8548, 13795: LevelCardFade2 F > 50 -> frame 8). Signs 6000 / 12984 / 12984: E beyond the frame (no cam_right). */
    { TH("th1", 56, 1, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage7.adp", .mus7 = "bgm/m_stage7.adp",
      .card_first = 71, .start_card = 1, .cps = CP_TH1, .ncps = N_(CP_TH1), .cam_shaft = &SHAFT_TH1 },
    { TH("th2", 57, 2, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage7.adp", .mus7 = "bgm/m_stage7.adp",
      .card_first = 72, .cps = CP_TH2, .ncps = N_(CP_TH2) },
    { TH("th3", 58, 3, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus4b = "bgm/m_boss2.adp", .mus4b_g4 = 5, .mus6 = "bgm/m_stage7.adp",
      .mus7 = "bgm/m_stage7.adp", .card_first = 76, .cps = CP_TH3, .ncps = N_(CP_TH3), .res_wipe = 1 , .max_objs = 1536 },
    /* Thermal Base: frame index 44-49 (session 21c, spec_tb_flow; tb_flow.c). Music (RE loop wrappers 4-7 of frames 44-49): slots 5-7
     * m_stage6 in TB1-TB3, m_stage6b in TB4-TB6; slot 4 m_midboss in TB1-TB5, m_boss2 in TB6 (loop_wrapper_play_music_4_49); TB6's start
     * stops channel 31 (12735: the arena starts silent, its boss plays slot 4). Cards (ObjectCodec A newest first, RE start events):
     * 61 (TB1 inline), 62 (TB2), 63 (TB3), 65 (TB4 12461), 67 (TB5 12546) = cards 61-70; ML.Q 26-31. Water: ML.A 1 in TB4 (B 3400) and TB5
     * (B 2920; the levers move it between 2920 and 3240: level_water_set), TB6 writes B 2920 below its 2000-px frame (dry in effect);
     * TB1-TB3 A 0. TB1 starts with the card (4307; the start lift rides up from the card's frame 161: 11881, tb_flow.c); TB2-TB6 by the
     * walk-in (4490 / 4526); TB1-TB5 end at the right edge (4135 / 12089 -> 3578); TB6 by its boss's results and the wipe (8547-8549).
     * Camera E = the Sign's x + 213 (4240-4242; the Signs stand at the frames' right edges, TB3 8032): beyond the frame, no cam_right. */
    { TB("tb1", 44, 1, "bgm/m_stage6.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage6.adp", .mus7 = "bgm/m_stage6.adp",
      .card_first = 61, .start_card = 1, .cps = CP_TB1, .ncps = N_(CP_TB1) },
    { TB("tb2", 45, 2, "bgm/m_stage6.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage6.adp", .mus7 = "bgm/m_stage6.adp",
      .card_first = 62, .cps = CP_TB2, .ncps = N_(CP_TB2) },
    { TB("tb3", 46, 3, "bgm/m_stage6.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage6.adp", .mus7 = "bgm/m_stage6.adp",
      .card_first = 63, .cps = CP_TB3, .ncps = N_(CP_TB3) },
    { TB("tb4", 47, 4, "bgm/m_stage6b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage6b.adp", .mus7 = "bgm/m_stage6b.adp",
      .card_first = 65, .water = 3400, .cps = CP_TB4, .ncps = N_(CP_TB4) },
    { TB("tb5", 48, 5, "bgm/m_stage6b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage6b.adp", .mus7 = "bgm/m_stage6b.adp",
      .card_first = 67, .water = 2920, .cps = CP_TB5, .ncps = N_(CP_TB5) },
    { TB("tb6", 49, 6, "bgm/m_stage6b.adp"), .mus4 = "bgm/m_boss2.adp", .mus6 = "bgm/m_stage6b.adp", .mus7 = "bgm/m_stage6b.adp",
      .music_off = 1, .res_wipe = 1, .vox = (const char *const[]){ "sx_still" }, .nvox = 1 },
    /* Pangu Lagoon: frame index 50-55 (session 23, spec_pl_flow; pl_flow.c). Lilac only: Classic routes Thermal Base's end (I 6) to
     * frame 50 for g1 = 0 (handle_frame_9 inline) and every act's start writes g1 = 0 (12844; PL1's start inline): .lilac_only. Music
     * (RE loop wrappers 4-7 of frames 50-55): slots 5-7 m_stage8 in PL1-PL3 (PL3's through 12965 while the music group 0x228ed is on),
     * m_stage8b in PL4-PL6; slot 4 m_midboss (loop_play_music_4_13 / 4_36) in PL1-PL5, m_boss4 in PL6 (loop_play_music_4_55). Cards
     * (ObjectCodec A newest first, RE start counters): 81 (PL1), 83 (PL2), 86 (PL3: the template only), 86 (PL4), 88 (PL5) = cards
     * 81-90; ML.Q 32-37. ML.A 1 / ML.B 2000 in every act but no water events (4150-4170): dry. PL1 starts with the card (4307; card
     * frames 6 / 8: "STAGE 7" / "PANGU LAGOON", 12854 / 12962); PL2-PL6 walk in (4489 / 6051 / 4526). PL1 / PL2 / PL4 / PL5 end at
     * the right edge (4135 -> 3578); PL3 through its stone door (13009, 12997-13006: next frame, g4 -1; pl_flow.c); PL6 by the
     * Holo-Dragon's results (4560 / 5394) and the wipe (13226 / 13228-13230: frame 8). Signs at the frames' right edges: no cam_right */
    { PL("pl1", 50, 1, "bgm/m_stage8.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage8.adp", .mus7 = "bgm/m_stage8.adp",
      .card_first = 81, .start_card = 1, .cps = CP_PL1, .ncps = N_(CP_PL1) },
    { PL("pl2", 51, 2, "bgm/m_stage8.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage8.adp", .mus7 = "bgm/m_stage8.adp",
      .card_first = 83, .cps = CP_PL2, .ncps = N_(CP_PL2) },
    { PL("pl3", 52, 3, "bgm/m_stage8.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage8.adp", .mus7 = "bgm/m_stage8.adp",
      .card_first = 86, .boss_cam = &CAM_PL3 },
    { PL("pl4", 53, 4, "bgm/m_stage8b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage8b.adp", .mus7 = "bgm/m_stage8b.adp",
      .card_first = 86, .cps = CP_PL4, .ncps = N_(CP_PL4) },
    { PL("pl5", 54, 5, "bgm/m_stage8b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage8b.adp", .mus7 = "bgm/m_stage8b.adp",
      .card_first = 88, .cps = CP_PL5, .ncps = N_(CP_PL5) , .max_objs = 1280 /* session 23: A5 ~812 + 152 stingers / gas (pl_gim / pl_enemy reports) */ },
    { PL("pl6", 55, 6, "bgm/m_stage8b.adp"), .mus4 = "bgm/m_boss4.adp", .mus6 = "bgm/m_stage8b.adp", .mus7 = "bgm/m_stage8b.adp",
      .cps = CP_PL6, .ncps = N_(CP_PL6), .res_wipe = 1 },
    /* Battle Glacier: frame index 59-64 (session 24, spec_bg_flow; bg_flow.c). Every character (no g1 write in the start events). Music
     * (RE loop wrappers 4-7 of frames 59-64): slot 5 m_stage9 in BG1-BG3 (loop_play_music_5_59), m_stage9b in BG4-BG6 (5_62 / 5_64);
     * slot 4 m_midboss (4_13) in BG1-BG3, m_boss6 in BG4 / BG5 (4_62), m_boss3 in BG6 (4_64); slots 6 / 7 play nothing in BG1-BG5
     * (6_59 only resets ControllerMusic), BG6 m_hub4 / m_rescue (6_64 / 7_64). Every start plays slot 5 (2968 BG1-BG3, 12465 BG5 /
     * BG6) except BG4: 14607 fadeout_music(2000) while g4 < 5 (the opening mech fight), 14608 play_music(5) from g4 5 (.music_fade_g4).
     * Cards (ObjectCodec A newest first from the start counters 91 / 93 / 95 / 97 / 99) = cards 91-100; ML.Q 42-47. ML.A 1 / ML.B 2000
     * (BG1-BG3) / 3000 (BG4-BG6) but no water events (4150-4170) in handle_frame_60..65: dry. BG1 starts with the card (4307), BG2-BG6
     * walk in (4477 / 6051 / 4526); BG1-BG5 end at the right edge (4135 -> 3578); BG6 by its boss's results and the wipe (8547-8549 ->
     * frame 8 -> I 9 -> frame 65, Final Dreadnought round 1). Camera E (RV bg*_start): beyond the frame width in BG1-BG4 (13197 / 13973 /
     * 11453 / 13197) and BG5 (the Sign at its right edge + 213): no cam_right; BG6 2661 = its Sign (2448) + 213. */
    { BG("bg1", 59, 1, "bgm/m_stage9.adp"), .mus4 = "bgm/m_midboss.adp", .card_first = 91, .start_card = 1, .cps = CP_BG1, .ncps = N_(CP_BG1),
      .dig_zones = DIGZ_BG_ALL, .ndig_zones = N_(DIGZ_BG_ALL), .dig_rewards = DIGR_BG, .ndig_rewards = N_(DIGR_BG) },
    { BG("bg2", 60, 2, "bgm/m_stage9.adp"), .mus4 = "bgm/m_midboss.adp", .card_first = 93, .cps = CP_BG2, .ncps = N_(CP_BG2),
      .dig_zones = DIGZ_BG_ALL, .ndig_zones = N_(DIGZ_BG_ALL), .dig_rewards = DIGR_BG, .ndig_rewards = N_(DIGR_BG) },
    { BG("bg3", 61, 3, "bgm/m_stage9.adp"), .mus4 = "bgm/m_midboss.adp", .card_first = 95, .cps = CP_BG3, .ncps = N_(CP_BG3) },
    { BG("bg4", 62, 4, "bgm/m_stage9b.adp"), .mus4 = "bgm/m_boss6.adp", .card_first = 97, .music_fade_g4 = 5 },
    { BG("bg5", 63, 5, "bgm/m_stage9b.adp"), .mus4 = "bgm/m_boss6.adp", .card_first = 99, .cps = CP_BG5, .ncps = N_(CP_BG5),
      .max_objs = 1280 /* 747 on-map placements (118 + 102 switch blocks, 55 platforms) + elite / anti-grav spawns, crystals, shots */ },
    { BG("bg6", 64, 6, "bgm/m_stage9b.adp"), .mus4 = "bgm/m_boss3.adp", .mus6 = "bgm/m_hub4.adp", .mus7 = "bgm/m_rescue.adp", .res_wipe = 1,
      .cam_right = 2448 + 213 /* RV bg6_start_lilac: CAM.E 2661 = the Sign (2448) + 213 */ },
    /* Final Dreadnought: frame index 65-72 (session 24, spec_fd_flow; fd_flow.c). Stages 10-13 = rounds 1-4, two frames each; every
     * character. Music (RE loop wrappers 4-7 of frames 65-72, bgm_refs): slot 5 m_stage10 (5_65) in fd1a / fd1b, m_stage10b (5_67 / 5_68)
     * in fd2a / fd2b, m_stage10c (5_69 / 5_70) in fd3a / fd3b, m_stage10d (7_15) in fd4a, m_stage10e (5_72) in fd4b; slot 4 m_midboss
     * (4_13 / 4_36) in the a frames, m_boss (4_23) fd1b, m_boss6 / m_boss6b (15688 / 15689 by the music group) fd2b, m_boss4 (4_55) fd3b,
     * m_boss7 (16740: g4 < 3) / m_boss5 (16739: g4 >= 3) fd4b; slot 7 m_stage10d (7_69 / 7_15) fd3a-fd4a, m_stage10e fd4b; slot 6 m_boss7 fd4b. Cards (spec_stage_cards
     * 4: Intro2 frame 11-14 "ROUND n", Intro3 frame 8 "Final Dreadnought"; importer stage_card); no collectable cards (ObjectCodec none).
     * The a frames start with the card (4307: on_frame_66 / 68 / 70 / 72_start_events), the b frames walk in (4477 / 6051 / 4526; RV
     * fd1b / fd2b / fd3b / fd4b_start: MV (61, Y) at g4 1). Ends (RE handlers): fd1a / fd3a the right edge (4135 / 16102 -> 3578); fd2a
     * the right edge below y 1100 (15652); fd4a the right edge with the world stopped for 100 frames (16721: group 0x21a5d off, Sign
     * flag 8 + Sign.B; 16723 at B > 100 -> the fade); every b frame by its boss's results (4560-4586 / 5394) and the wipe (8547-8549 ->
     * frame 8 -> route I 10-12) except fd4b, whose boss script ends its results (no wipe in handle_frame_73) -> results.c's stage end ->
     * the ending (s 13 -> frame 83, ending.c). ML.A 1 / ML.B 2000 (4800 fd2a / fd2b) but only fd2b holds
     * air events, its own (15790-15796: the siren alarm's vacuum, not ML.B; spec_fd_flow 3): dry. ML.Q 48-55. */
    { FD("fd1a", 65, 10, 1, SFX_FD1A, "bgm/m_stage10.adp"), .mus4 = "bgm/m_midboss.adp", .start_card = 1, .cps = CP_FD1A, .ncps = N_(CP_FD1A),
      .max_objs = 1280 },
    { FD("fd1b", 66, 10, 2, SFX_FD1B, "bgm/m_stage10.adp"), .mus4 = "bgm/m_boss.adp", .res_wipe = 1, .max_objs = 1280 },
    { FD("fd2a", 67, 11, 1, SFX_FD2A, "bgm/m_stage10b.adp"), .mus4 = "bgm/m_midboss.adp", .start_card = 1, .cps = CP_FD2A, .ncps = N_(CP_FD2A),
      .exits = EX_FD2A, .nexits = N_(EX_FD2A), .max_objs = 1280 },
    { FD("fd2b", 68, 11, 2, SFX_FD2B, "bgm/m_stage10b.adp"), .mus4 = "bgm/m_boss6.adp", .res_wipe = 1, .max_objs = 1280 },
    { FD("fd3a", 69, 12, 1, SFX_FD3A, "bgm/m_stage10c.adp"), .mus4 = "bgm/m_midboss.adp", .mus7 = "bgm/m_stage10d.adp", .start_card = 1,
      .cps = CP_FD3A, .ncps = N_(CP_FD3A), .max_objs = 1280 },
    { FD("fd3b", 70, 12, 2, SFX_FD3B, "bgm/m_stage10c.adp"), .mus4 = "bgm/m_boss4.adp", .mus7 = "bgm/m_stage10d.adp", .res_wipe = 1,
      .max_objs = 1280 },
    { FD("fd4a", 71, 13, 1, SFX_FD4A, "bgm/m_stage10d.adp"), .mus4 = "bgm/m_midboss.adp", .mus7 = "bgm/m_stage10d.adp", .start_card = 1,
      .cps = CP_FD4A, .ncps = N_(CP_FD4A), .exit_hold = 100, .max_objs = 1280 },
    { FD("fd4b", 72, 13, 2, SFX_FD4B, "bgm/m_stage10e.adp"), .mus4 = "bgm/m_boss7.adp", .mus4b = "bgm/m_boss5.adp", .mus4b_g4 = 3,
      .mus6 = "bgm/m_boss7.adp", .mus7 = "bgm/m_stage10e.adp" /* no wipe (handle_frame_73 has no 8547-8549): the final boss's
      script ends the results (results_request_end) and results.c's stage end starts the ending (ending.c) */ },
};
static const level_def_t DEFAULT = { .dir = "", .frame = -1, .stage = -1, .act = 1, .nacts = 1, .music = "bgm/m_stage1.adp" };

const level_def_t *level_def(const char *dir) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (!strcmp(LEVELS[i].dir, dir)) return &LEVELS[i];
    return &DEFAULT;
}
const level_def_t *level_cur(void) { return level_def(G.lvdir); }
/* Classic / continue route: the first stage of each character (RE 799: Classic with G1 == 4 (Milla) -> frame 16, Aqua
 * Tunnel; everyone else -> frame 20, Dragon Valley). Indexed by the stage record index (s). */
const char *level_route_start(int character) { return level_first_of_stage(chr_is_milla(character) ? 0 : 1); }
/* first act of a stage record index (Time Attack / routes) */
const char *level_first_of_stage(int stage) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == stage && LEVELS[i].act == 1) return LEVELS[i].dir;
    return NULL;
}
/* the next act of the same stage (NULL after the last act) */
const char *level_next(const char *dir) {
    const level_def_t *d = level_def(dir);
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == d->stage && LEVELS[i].act == d->act + 1) return LEVELS[i].dir;
    return NULL;
}

/* ---- stage flow shared by every level (session 13; before: aqua.c, by act number) ---------------------------------- */
static int cam_locked;
int level_cam_locked(void) { return cam_locked; }   /* 4212 / 4451: the boss camera trigger flag */
/* the act start: remove the item sets this character does not get (4318 / 4319) and the AT2 jelly / petal row (4484 /
 * 4485: only while g1 != 1, i.e. always in the port) */
static int shaft_on;   /* TH1 13499: Sign D = 1 once the player is deeper than the shaft (reset at every act start) */
static int water_rt = -1;   /* ML.B moved by a script this act (-1: the level's .water; TB5's levers 12491-12501) */
int  level_water(void) { return water_rt >= 0 ? water_rt : level_cur()->water; }
void level_water_set(int b) { water_rt = b; }
void level_flow_spawn(void) {
    const level_def_t *L = level_cur();
    cam_locked = 0; shaft_on = 0; water_rt = -1;
    for (int k = 0; k < L->nkills; k++) {
        const killbox_t *z = &L->kills[k];
        if ((z->who == 1 && chr_is_milla(G.character)) || (z->who == 2 && !chr_is_milla(G.character))) continue;
        if ((z->who == 3 && G.character == CHR_LILAC) || (z->who == 4 && G.character == CHR_CAROL)) continue;   /* MV.A tests: bike-Carol keeps neither */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->x < z->x0 || o->x > z->x1 || o->y < z->y0 || o->y > z->y1) continue;
            if (o->type == OT_CRYSTAL || (z->jelly && (o->type == OT_JELLY || o->type == OT_PETAL))) o->alive = 0;
        }
    }
    if (L->crystals_once && G.at_g4 != 0 && G.at_g4 != 1)                     /* RM1 start: a return or a checkpoint restart */
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CRYSTAL) G.o[i].alive = 0;
    /* class sets and limit markers by character (FN3 8082: MV.A = 0, 8083: MV.A != 0; bike-Carol counts as != 0) */
    int lilac = G.character == CHR_LILAC;
    for (int k = 0; k < L->nckills; k++) {
        const classkill_t *c = &L->ckills[k];
        if ((c->who == 1) != lilac) continue;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type >= c->t0 && G.o[i].type <= c->t1) G.o[i].alive = 0;
    }
    for (int k = 0; k < L->ncmarks; k++) {
        const markmove_t *m = &L->cmarks[k];
        if ((m->who == 1) != lilac) continue;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive) continue;
            if (o->type == OT_CAMLIM_L && m->l != -32768) o->x = m->l;
            if (o->type == OT_CAMLIM_R && m->r != -32768) o->x = m->r;
            if (o->type == OT_CAMLIM_T && m->t != -32768) o->y = m->t;
            if (o->type == OT_CAMLIM_B && m->b != -32768) o->y = m->b;
        }
    }
}
/* CAM.M = 1 to the CameraLimitBoss markers of the act (loop_camera_mode1: left x, top y, right x, bottom y), for stage scripts */
void level_lock_markers(void) {
    float l = 0, t = 0, r = (float)G.lvl.w, b = (float)G.lvl.h;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_CAMLIM_L) l = o->x; else if (o->type == OT_CAMLIM_R) r = o->x; else if (o->type == OT_CAMLIM_T) t = o->y; else if (o->type == OT_CAMLIM_B) b = o->y;
    }
    cam_locked = 1; camera_boss_bounds(l, t, r, b);
}
/* the act's track for play_music(slot) (the frame's loop table: 4 boss / midboss, 5 the stage, 6 / 7 per frame) */
const char *level_music_slot(int slot) {
    const level_def_t *L = level_cur();
    if (slot == 4 && L->mus4b && G.at_g4 >= L->mus4b_g4) return L->mus4b;   /* TH3 13702 (g4 >= 5: m_boss2) */
    return slot == 4 ? L->mus4 : slot == 5 ? L->music : slot == 6 ? L->mus6 : slot == 7 ? L->mus7 : NULL;
}
/* the exit the player is taking this frame (NULL: none); acts without an exit list use the right edge (4135 / 5045) */
const exit_t *level_exit_check(const player_t *p) {
    static const exit_t RIGHT = { 1, 0, -50, 0, 0, NULL };
    const level_def_t *L = level_cur();
    double B = p->ground ? p->gsp : p->vx, Y = p->y - ORIG_REF_DY;
    if (L->script_exit) return NULL;                                 /* FN3: no 5045 / 3578 (the stage script ends the act) */
    int n = L->nexits ? L->nexits : 1;
    for (int k = 0; k < n; k++) {
        const exit_t *e = L->nexits ? &L->exits[k] : &RIGHT;
        if (e->edge > 0 ? !(p->x > G.lvl.w + e->x && B > 0) : !(p->x < e->x && B < 0)) continue;
        if ((e->ymin || e->ymax) && !(Y > e->ymin && Y < e->ymax)) continue;
        if (!e->to && !level_next(G.lvdir)) continue;
        return e;
    }
    return NULL;
}
/* the entry for an act change from act `from` (NULL: the default, x -32 at the start point) */
const entry_t *level_entry(const char *from) {
    const level_def_t *L = level_cur();
    for (int k = 0; k < L->nentries; k++) if (!L->entries[k].from || (from && !strcmp(L->entries[k].from, from))) return &L->entries[k];
    return NULL;
}
/* Classic route after a stage (RE frame 8 = handle_frame_9 inline, Classic block g0 = 1; spec_records_saves 1 / spec_th_flow 1.2):
 * MenuSequence.I = the finished stage s -> the next frame, by g1 (0 Lilac / != 0 Carol, bike-Carol, Milla): I 0 / 1 -> 24 (RM),
 * 2 -> 29 (FN), 3 -> 35 (SB), 4 -> 39 (JC), 5 -> 44 (Thermal Base, g1 = 0) / 56 (Trap Hideout, g1 != 0), 6 -> 50 (Pangu Lagoon,
 * g1 = 0) / 84 (before Battle Glacier), 7 -> 84 (g1 = 0) / 56 (g1 != 0), 8 -> 84 (g1 = 0) / 44 (g1 != 0), 9 -> 65 (Final Dreadnought
 * round 1, every g1), 10 -> 67 (round 2), 11 -> 69 (round 3), 12 -> 71 (round 4), 13 -> 83 (the ending: results.c / ending.c, not
 * a stage: NULL here). Frame 84 is the shoot-'em-up lead-in to Battle Glacier (on_frame_85: m_schmup, the dragon ride; not ported): its
 * end (22695) sets g4 0 and frame 59 = BG1, so the port routes I 6 (g1 != 0) / I 7 / I 8 (g1 = 0) straight to Battle Glacier (stage 9;
 * session 24). As stage indices; a stage the port lacks gives NULL: back to the front end. */
static const signed char ROUTE[14][2] = { { 2, 2 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 8 }, { 7, 9 }, { 9, 8 }, { 9, 6 }, { 10, 10 },
                                          { 11, 11 }, { 12, 12 }, { 13, 13 }, { -1, -1 } };
const char *level_route_next(int stage) {
    if (stage < 0 || stage >= (int)(sizeof ROUTE / sizeof ROUTE[0])) return NULL;
    int s = ROUTE[stage][G.character != CHR_LILAC];               /* g1 = 0: Lilac only */
    return s >= 0 ? level_first_of_stage(s) : NULL;
}
/* RM5 labyrinth warps (spec_rm_flow 2.5): the player is in a warp zone moving its way */
const warp_t *level_warp_check(const player_t *p) {
    const level_def_t *L = level_cur();
    double B = p->ground ? p->gsp : p->vx, X = p->x, Y = p->y - ORIG_REF_DY;
    for (int k = 0; k < L->nwarps; k++) {
        const warp_t *w = &L->warps[k];
        if (X > w->x0 && X <= w->x1 && Y > w->y0 && Y < w->y1 && (w->dir < 0 ? B < 0 : B > 0)) return w;
    }
    return NULL;
}
void level_warp_do(player_t *p, const warp_t *w) {   /* 6734-6743: a new 32-frame walk-in at the target, then 6599 (g4 = 1) */
    p->x = w->tx; p->y = w->ty + ORIG_REF_DY; p->ground = 0; p->vx = p->vy = 0; p->gsp = 0; p->face = w->face;
    G.at_g4 = w->g4; G.walk_b = w->walk_b; G.clear_t = 0; G.exit_b = 0; G.input_lock = 0;
    G.act_in = 1; G.act_fade = 248;
    camera_reset(); if (w->walkin_E > 0) G.cam_nE = w->walkin_E;   /* 6597 */
    camera_update();
}
/* normal camera bounds (spec_at_flow 3.1): C = 0, E = cam_right or the frame width; after g4 >= 10 the AT2 values */
void level_camera_bounds(void) {
    const level_def_t *L = level_cur();
    if (L->cam_cp10_pad && G.at_g4 >= 10) { G.cam_nC = (float)L->cam_cp10_left; G.cam_nE = (float)G.lvl.w + L->cam_cp10_pad; }
    else if (L->cam_right) G.cam_nE = (float)L->cam_right;
    if (L->cam_box) { G.cam_nC = L->cam_box->C; G.cam_nD = L->cam_box->D; G.cam_nE = L->cam_box->E; G.cam_nF = L->cam_box->F; }   /* FN6 8553 */
    if (L->cam_shaft) {                                             /* TH1 13498 (Sign D 0: E = the right marker's x) / 13500 (D 1) */
        if (!shaft_on) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CAMLIM_R) { G.cam_nE = G.o[i].x; break; } }
        else { G.cam_nD = L->cam_shaft->D; G.cam_nE = (float)G.lvl.w + L->cam_shaft->E_pad; }
    }
    if (L->nrooms) {                                                /* RM5 6745-6750: room bounds (the last matching row wins) */
        double X = G.p.x, Y = G.p.y - ORIG_REF_DY;
        int rails = 0; for (int i = 0; i < G.nobjs && !rails; i++) if (G.o[i].alive && G.o[i].type == OT_WR_RAIL) rails = 1;
        for (int k = 0; k < L->nrooms; k++) {
            const camroom_t *r = &L->rooms[k];
            if (X > r->x0 && X <= r->x1 && Y > r->y0 && Y < r->y1 && (r->rails < 0 || r->rails == rails)) { G.cam_nE = r->E; G.cam_nF = r->F; }
        }
    }
}
void level_release_camera(int keep_trigger) { if (!keep_trigger) cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
void level_checkpoint(int g4, float x, float y) {                   /* g4 / g5 / g6 + loop_cp_16 (restart values; public for stage scripts, session 24) */
    G.at_g4 = g4; G.at_g5 = x; G.at_g6 = y;
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
}
#define checkpoint level_checkpoint
/* per frame, after the objects: checkpoints, then the boss camera lock (lock: X > lock_x and lock_ymin < Y < lock_ymax;
 * release: X < unlock_x and Y < unlock_y; a zone with g4_max does nothing once g4 >= g4_max) */
void level_flow_frame(const player_t *p) {
    const level_def_t *L = level_cur();
    if (L->nrooms && !G.act_in) level_camera_bounds();                       /* RM5 group 0x2250b: room bounds every frame (not during a walk-in: 6597) */
    if (L->cam_box) level_camera_bounds();                                   /* FN6 8553 + the inline tail of handle_frame_35: every frame */
    if (L->cam_shaft && !shaft_on && p->y - ORIG_REF_DY > L->cam_shaft->y_on) { shaft_on = 1; level_camera_bounds(); }   /* TH1 13499 -> 13500 */
    double X = p->x;
    for (int k = 0; k < L->ncps; k++) {
        const cpzone_t *c = &L->cps[k];
        if (!(X > c->x0 && X < c->x1 && G.at_g4 < c->g4 + ((c->nosnap & 2) ? 1 : 0)) || (c->ymin > 0 && !(p->y - ORIG_REF_DY > c->ymin)) || (c->ymin < 0 && !(p->y - ORIG_REF_DY < -c->ymin))) continue;
        if (c->nosnap & 1) { G.at_g4 = c->g4; G.at_g5 = c->x; G.at_g6 = c->y; } else checkpoint(c->g4, c->x, c->y);
        if (L->music_cp && c->g4 >= 10) music_play(L->music_cp);                 /* loop_next_act_17 / _26: play_music(5) */
    }
    if (L->flag1_y) G.stage_flags = (G.stage_flags & ~1) | (p->y - ORIG_REF_DY > L->flag1_y);   /* RM1 inline: ML flag 0x1 */
    const camzone_t *z = L->boss_cam;
    if (!z || (z->g4_max && G.at_g4 >= z->g4_max)) return;
    int px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    if (!cam_locked && px > z->lock_x && py > z->lock_ymin && py < z->lock_ymax) { cam_locked = 1; camera_boss_bounds(z->l, z->t, z->r, z->b); }
    else if (cam_locked && px < z->unlock_x && py < z->unlock_y) level_release_camera(0);
}
