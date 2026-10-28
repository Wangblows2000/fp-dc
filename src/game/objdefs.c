/* Object registry (session 13): one row per engine type with the properties of its Fusion object that every level
 * shares. Qualifier groups are class properties in Fusion (the same in every frame), so they live here and not in the
 * level code:
 *   OD_SOLID     QualifierList +0x218c0 (solid for the player's sensors)
 *   OD_OBSTACLE  +0x218d8 (one-way / platform obstacle; minus flag-0 platforms)
 *   OD_NOEDGE    +0x218f0 (the edge sensors take no angle from it)
 *   OD_BOXCOL    initialize_active(true): collision with the image rectangle instead of the mask
 * ens: the sprite (object-bank animation) set at spawn for types that need nothing else (others set theirs in their
 * own spawn code). The comment names the Fusion class(es) the importer maps to the type (tools/import_frame.py
 * ENGINE_TYPES / ENGINE_TYPES_ID); runtime-only types have none.
 * Adding an existing object to a new level needs no change here: placing the class is enough (RE_NOTES "Reusable
 * objects"). A newly ported class gets a row here, an ENGINE_TYPES entry and its behaviour code. */
#include "../fp.h"
#include "jc.h"                                 /* Jade Creek types (session 21) */
#include "th.h"                                 /* Trap Hideout types (session 21b) */
#include "fd.h"                                 /* Final Dreadnought types (session 24) */
#include "bg.h"                                 /* Battle Glacier types (session 24) */
#include "pl.h"                                 /* Pangu Lagoon types (session 23) */
#include "tb.h"                                 /* Thermal Base types (session 21c) */

static const objdef_t DEFS[] = {
    { OT_START,         0,                                     NULL         }, /* playerstartingpoint */
    { OT_CRYSTAL,       0,                                     NULL         }, /* objectcrystal */
    { OT_DRAGONFLY,     0,                                     NULL         }, /* enemydragonfly */
    { OT_MON_INVINC,    OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitorinvincibility */
    { OT_PETAL,         0,                                     NULL         }, /* objectpetal */
    { OT_SPIKES,        OD_SOLID|OD_NOEDGE,                    NULL         }, /* hazardspikesup */
    { OT_SPRING,        OD_NOEDGE,                             NULL         }, /* yellowspringup */
    { OT_SIGN,          0,                                     NULL         }, /* sign */
    { OT_TURRETUS,      0,                                     NULL         }, /* enemyturretus */
    { OT_ROCK,          OD_SOLID|OD_BOXCOL,                    NULL         }, /* breakablerock */
    { OT_MONITOR,       OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitorcrystals */
    { OT_SPRING_UL,     OD_NOEDGE,                             NULL         }, /* yellowspringupleft */
    { OT_SPRING_UR,     OD_NOEDGE,                             NULL         }, /* yellowspringupright */
    { OT_SPRING_R,      OD_SOLID|OD_NOEDGE,                    NULL         }, /* yellowspringright */
    { OT_SPRING_L,      0,                                     NULL         }, /* yellowspringleft */
    { OT_SPRING_D,      OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* yellowspringdown (as redspringdown) */
    { OT_HANG,          OD_HANG,                                     NULL         }, /* gimmickhangingidlesensor */
    { OT_MON_RINGS,     OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitorrings */
    { OT_MON_1UP,       OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitor1up */
    { OT_PARALLAX,      0,                                     NULL         }, /* levelparallaxpiece0 */
    { OT_LADDERTOP,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* laddertop */
    { OT_GANSEKI,       0,                                     NULL         }, /* enemygansekijr */
    { OT_PLAT_VALLEY,   OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelvalleyplatform1 */
    { OT_PLAT_MOVE2,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelmovingplatform2 */
    { OT_PLAT_MOVE3,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelmovingplatform3 */
    { OT_BRIDGE,        0,                                     NULL         }, /* gimmickbridge12logs */
    { OT_LOG,           OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* gimmickbridgelog */
    { OT_FLOUNDER,      0,                                     NULL         }, /* enemystarflounder */
    { OT_CACTEON,       0,                                     NULL         }, /* enemycacteon */
    { OT_SPOREPOD,      0,                                     NULL         }, /* enemysporepod */
    { OT_LIMITER,       0,                                     NULL         }, /* enemylimiter */
    { OT_CARD,          0,                                     "card"       }, /* objectcodec */
    { OT_SPIKES_L,      OD_SOLID|OD_NOEDGE,                    "spk_left"   }, /* hazardspikesleft */
    { OT_SPIKES_R,      OD_SOLID|OD_NOEDGE,                    "spk_right"  }, /* hazardspikesright */
    { OT_SPIKES_D,      OD_SOLID|OD_NOEDGE,                    "spk_down"   }, /* hazardspikesdown */
    { OT_RSPRING_UL,    OD_OBSTACLE|OD_NOEDGE,                 "rspring_ul" }, /* redspringupleft */
    { OT_CORK_L,        0,                                     NULL         }, /* halfcorkscrewleft */
    { OT_CORK_R,        0,                                     NULL         }, /* halfcorkscrewright */
    { OT_LAYER_SW,      0,                                     NULL         }, /* layerchangeswitch */
    { OT_LAYER_0,       0,                                     NULL         }, /* layerchangeto0 */
    { OT_LAYER_1,       0,                                     NULL         }, /* layerchangeto1 */
    { OT_SPIRAL_TOP,    0,                                     NULL         }, /* spiraltunneltop */
    { OT_SPIRAL_BOTTOM, 0,                                     NULL         }, /* spiraltunnelbottom */
    { OT_SPIRAL_MASK,   OD_SOLID,                              "spiralmask" }, /* spiraltunnelmask */
    { OT_BALLTUNNEL,    OD_SOLID,                              "balltunnel" }, /* levelballtunnel1 */
    { OT_GEYSER,        OD_OBSTACLE|OD_NOEDGE,                 "geyser"     }, /* levelgeyserplate */
    { OT_WR_BASE,       0,                                     NULL         }, /* minibosswretchnidbase */
    { OT_WR_TURRET,     0,                                     NULL         }, /* minibosswretchnidturret */
    { OT_WR_WEAK,       0,                                     NULL         }, /* minibosswretchnidweakspot */
    { OT_WR_RAIL,       0,                                     NULL         }, /* minibossrailing */
    { OT_WR_BARRIER,    OD_SOLID|OD_HIDDEN,                    NULL         }, /* minibossbarrier (ctor set_visible 0: the railing is the visual) */
    { OT_WR_LEG,        0,                                     NULL         }, /* minibosswretchnidleftleg1 */
    { OT_TD_BODY,       0,                                     NULL         }, /* minibosstredd */
    { OT_TD_PIECE,      0,                                     NULL         }, /* minibosstreddleft1 */
    { OT_TD_PUSH,       0,                                     NULL         }, /* enemytreddpushleft */
    { OT_TD_PUSH2,      0,                                     NULL         }, /* enemytreddpushleft2 */
    { OT_WLOG,          OD_OBSTACLE|OD_BOXCOL,                 NULL         }, /* waterfalllog */
    { OT_WLOG_GEN,      0,                                     NULL         }, /* waterfallloggenerator */
    { OT_FLOATPLAT,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelfloatingplatform */
    { OT_WF_KILL,       0,                                     NULL         }, /* - */
    { OT_BOULDER,       0,                                     NULL         }, /* gimmickboulder */
    { OT_WALLPLATE,     0,                                     NULL         }, /* switchwallplate */
    { OT_TRAPDOOR,      0,                                     NULL         }, /* switchtrapdoor */
    { OT_PPLATE,        0,                                     NULL         }, /* switchpressureplate */
    { OT_DOOR,          OD_SOLID,                              NULL         }, /* switchdoor */
    { OT_TRUNK,         OD_SOLID,                              NULL         }, /* treetrunk */
    { OT_PBLOCK1,       OD_SOLID|OD_NOEDGE|OD_BOXCOL,          "pblock1"    }, /* pushableblock1 */
    { OT_PBLOCK2,       OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* pushableblock2 */
    { OT_EXPLODE,       0,                                     NULL         }, /* - */
    { OT_PELLET,        0,                                     NULL         }, /* - */
    { OT_ITEM_FLY,      0,                                     NULL         }, /* - */
    { OT_SHIELD,        0,                                     NULL         }, /* - */
    { OT_SPORE,         0,                                     NULL         }, /* - */
    { OT_FIREBALL,      0,                                     NULL         }, /* - */
    { OT_TD_BOMB,       0,                                     NULL         }, /* - */
    { OT_DEBRIS,        0,                                     NULL         }, /* - */
    { OT_BLAST,         0,                                     NULL         }, /* - */
    { OT_THROWN,        0,                                     NULL         }, /* - */
    { OT_BAM1,          0,                                     NULL         }, /* gimmickbamboospike */
    { OT_BAM2,          0,                                     NULL         }, /* gimmickbamboospike2 */
    { OT_BAM3,          0,                                     NULL         }, /* gimmickbamboospike3 */
    { OT_BAM4,          0,                                     NULL         }, /* gimmickbamboospike4 */
    { OT_TAOSTONE,      0,                                     NULL         }, /* objecttaostone */
    { OT_SPINLOG,       0,                                     "slog0"      }, /* spinninglog */
    { OT_SPINLOG_MASK,  OD_OBSTACLE|OD_HIDDEN,                 "slogmask"   }, /* spinninglogmask */
    { OT_BROCK2,        OD_SOLID,                              NULL         }, /* breakablerock2 */
    { OT_SNAKE_HEAD,    0,                                     NULL         }, /* bossserpentinehead */
    { OT_SNAKE_BODY,    0,                                     NULL         }, /* bosssnakebody */
    { OT_SNAKE_HIT,     0,                                     NULL         }, /* bosssnakehitbox */
    { OT_GEYSER_HOLE1,  0,                                     NULL         }, /* bossgeyseroverlap1 */
    { OT_GEYSER_HOLE2,  0,                                     NULL         }, /* bossgeyseroverlap2 */
    { OT_GBOULDER,      0,                                     NULL         }, /* bossgiantboulder */
    { OT_BTUNNEL,       0,                                     NULL         }, /* bosstunnel */
    { OT_BTUNNEL2,      OD_SOLID,                              NULL         }, /* bosstunnel2 */
    { OT_BTUNNEL3,      0,                                     NULL         }, /* bosstunnel3 */
    { OT_BRK1,          0,                                     NULL         }, /* breakable1 */
    { OT_BRK2,          0,                                     NULL         }, /* breakable2 */
    { OT_BRK3,          0,                                     NULL         }, /* breakable3 */
    { OT_BOUNCEMODE,    0,                                     NULL         }, /* bossbouncemode */
    { OT_CAMTRIGGER,    0,                                     NULL         }, /* cameratriggerboss */
    { OT_JUMPPAD,       0,                                     NULL         }, /* objectjumppad */
    { OT_MON_KEYSTONE,  OD_SOLID|OD_NOEDGE,                    "mon_key"    }, /* monitorkeystone */
    { OT_LOOP016,       OD_SOLID|OD_LAYER0,                              "loop016"    }, /* levellayer016 */
    { OT_LOOP0Q,        OD_SOLID|OD_LAYER0,                              "loop0q"     }, /* levellayer0loop04 */
    { OT_LOOP115,       OD_SOLID|OD_LAYER1,                              "loop115"    }, /* levellayer115 */
    { OT_LOOP1Q,        OD_SOLID|OD_LAYER1,                              "loop1q"     }, /* levellayer1loop03 */
    { OT_CHUNK,         0,                                     NULL         }, /* - */
    { OT_WF_BACK,       0,                                     NULL         }, /* waterfallback */
    { OT_WF_FRONT,      0,                                     NULL         }, /* waterfallfront */
    { OT_WF_FRONT2,     0,                                     NULL         }, /* waterfallfront2 */
    { OT_WF_FRONT3,     0,                                     NULL         }, /* waterfallfront3 */
    { OT_WF_FRONT4,     0,                                     NULL         }, /* waterfallfront4 */
    { OT_BBOULDER,      0,                                     NULL         }, /* - */
    { OT_KEYSTONE,      0,                                     NULL         }, /* - */
    { OT_SPIRAL_FRONT,  OD_HIDDEN,                             "spiralfront" }, /* spiraltunnelfront */
    { OT_FRONTROCK,     0,                                     "frontrock1" }, /* frontrock1 */
    { OT_FRONTTREE,     0,                                     "fronttree"  }, /* fronttree */
    { OT_TREEFRONT,     0,                                     "treefront"  }, /* treefront */
    { OT_MON_WOOD,      OD_SOLID|OD_NOEDGE,                    "mon_wood"   }, /* monitorwoodshield */
    { OT_MON_EARTH,     OD_SOLID|OD_NOEDGE,                    "mon_earth"  }, /* monitorearthshield */
    { OT_MON_FIRE,      OD_SOLID|OD_NOEDGE,                    "mon_fire"   }, /* monitorfireshield */
    { OT_MON_METAL,     OD_SOLID|OD_NOEDGE,                    "mon_metal"  }, /* monitormetalshield */
    { OT_MON_WATER,     OD_SOLID|OD_NOEDGE,                    "mon_water"  }, /* monitorwatershield */
    { OT_MONICON,       0,                                     NULL         }, /* - */
    { OT_BUBBLEPATCH,   0,                                     "bpatch"     }, /* bubblepatch */
    { OT_FUEL,          0,                                     NULL         }, /* objectfuel */
    { OT_PBIKE,         0,                                     NULL         }, /* - */
    { OT_DIGSPOT,       0,                                     NULL         }, /* objectdigspot */
    { OT_FIREFLY,       0,                                     NULL         }, /* enemyfirefly */
    { OT_SLIME,         0,                                     NULL         }, /* enemyslime */
    { OT_WHISPMAKER,    0,                                     NULL         }, /* objectwhispmaker */
    { OT_ABRICK,        OD_SOLID|OD_BOXCOL,                    NULL         }, /* breakableaquabrick */
    { OT_ABRICK2,       OD_SOLID|OD_BOXCOL,                    NULL         }, /* breakableaquabrick2 */
    { OT_SLIMEBURST,    0,                                     NULL         }, /* - */
    { OT_WHISP,         0,                                     NULL         }, /* - */
    { OT_ABPIECE,       0,                                     NULL         }, /* - */
    { OT_WJET,          0,                                     NULL         }, /* waterjet */
    { OT_WJET_R,        0,                                     NULL         }, /* waterjetright */
    { OT_WJET_B,        0,                                     NULL         }, /* waterjetback */
    { OT_WJET_MAKER,    0,                                     NULL         }, /* waterjetmaker */
    { OT_JELLY,         OD_SOLID,                              NULL         }, /* jellyspring */
    { OT_RSPRING_U,     OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* redspringup */
    { OT_RSPRING_D,     OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* redspringdown */
    { OT_RSPRING_L,     OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* redspringleft */
    { OT_RSPRING_R,     OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* redspringright */
    { OT_RSPRING_UR,    OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* redspringupright */
    { OT_MON_BOMB,      OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitorbomb */
    { OT_MON_CRATE,     OD_SOLID|OD_NOEDGE,                    NULL         }, /* monitorcrate */
    { OT_WATERHIT,      0,                                     NULL         }, /* - */
    { OT_BOMBX,         0,                                     NULL         }, /* - */
    { OT_ABPLAT,        OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* aquabrickplatform */
    { OT_COLLAPSE,      OD_SOLID,                              NULL         }, /* collapsingplatform */
    { OT_SDOOR2,        OD_SOLID,                              NULL         }, /* switchdoor2 */
    { OT_MBLOCK,        0,                                     NULL         }, /* switchmillablock */
    { OT_CRUSH_V,       OD_SOLID,                              NULL         }, /* levelcrusher1 */
    { OT_CRUSH_H,       OD_SOLID|OD_BOXCOL,                    NULL         }, /* levelcrusher1_847 */
    { OT_CRUSH_H2,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* levelcrusher2 */
    { OT_PIPE,          OD_SOLID,                              NULL         }, /* gimmickrotatingpipe */
    { OT_PIPEHUB,       OD_SOLID,                              NULL         }, /* gimmickrotatingpipebase */
    { OT_PIPECOG,       0,                                     NULL         }, /* gimmickrotatingpipebase2 */
    { OT_BEAM,          OD_SOLID,                              NULL         }, /* objectspinningbeam */
    { OT_AXEL1,         OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelaxelplatform1 */
    { OT_AXEL2,         OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelaxelplatform2 */
    { OT_AXMAKER1,      0,                                     NULL         }, /* axelplatformmaker1 */
    { OT_AXMAKER2,      0,                                     NULL         }, /* axelplatformmaker2 */
    { OT_SWSPAWN1,      0,                                     NULL         }, /* gimmickswingspawner1 */
    { OT_SWSPAWN2,      0,                                     NULL         }, /* gimmickswingspawner2 */
    { OT_SWBASE,        0,                                     NULL         }, /* - */
    { OT_SWCHAIN,       0,                                     NULL         }, /* - */
    { OT_SWPLAT,        OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* - */
    { OT_SCROLLBRICK,   0,                                     NULL         }, /* scrollingbrick */
    { OT_SLIMEFLOOR,    OD_SOLID,                              NULL         }, /* slimefloor */
    { OT_RAFT,          OD_SOLID|OD_BOXCOL,                    NULL         }, /* slimeraft */
    { OT_TREEOVL,       0,                                     NULL         }, /* aquatreeoverlay */
    { OT_FISHSTATUE,    0,                                     NULL         }, /* fishstatuefront */
    { OT_CRUSH_VIS,     0,                                     NULL         }, /* - */
    { OT_SLIMEF_VIS,    0,                                     NULL         }, /* - */
    { OT_SDOOR,         OD_SOLID,                              NULL         }, /* switchdoor_770 */
    { OT_WPLATE_AT,     0,                                     NULL         }, /* switchwallplate_831 */
    { OT_EXPL01,        0,                                     NULL         }, /* - */
    { OT_IW_HEAD,       0,                                     NULL         }, /* minibossironworm */
    { OT_IW_SEG,        0,                                     NULL         }, /* minibossironwormsegment */
    { OT_IW_BASE,       0,                                     NULL         }, /* minibossironwormbase */
    { OT_CH_HEAD,       0,                                     NULL         }, /* minibosschomper */
    { OT_CH_TAIL,       0,                                     NULL         }, /* minibosschompertail */
    { OT_CH_BALL,       0,                                     NULL         }, /* minibosschomperball */
    { OT_CH_MOUTH,      0,                                     NULL         }, /* minibosschompermouth */
    { OT_SN_HEAD,       0,                                     NULL         }, /* bosschompersnake */
    { OT_SN_SEG,        0,                                     NULL         }, /* bosschompersnakesegment */
    { OT_SN_MOUTH,      0,                                     NULL         }, /* bosschompersnakemouth */
    { OT_ACID,          0,                                     NULL         }, /* - */
    { OT_ROCK_AT,       0,                                     NULL         }, /* - */
    { OT_BUBBLE,        0,                                     NULL         }, /* - */
    { OT_IW_HEADF,      0,                                     NULL         }, /* - */
    { OT_ABPLAT2,       OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* aquabrickplatform_803 */
    { OT_AXEL1P,        OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelaxelplatform1_845 */
    { OT_AXEL2P,        OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelaxelplatform2_846 */
    { OT_SWSPAWN1B,     0,                                     NULL         }, /* gimmickswingspawner1_848 */
    /* members of numbered ranges (the code uses base + k) */
    { OT_PARALLAX + 1,  0,                                     NULL         }, /* levelparallaxpiece1 */
    { OT_PARALLAX + 2,  0,                                     NULL         }, /* levelparallaxpiece2 */
    { OT_PARALLAX + 3,  0,                                     NULL         }, /* levelparallaxpiece3 */
    { OT_PARALLAX + 4,  0,                                     NULL         }, /* levelparallaxpiece4 */
    { OT_PARALLAX + 5,  0,                                     NULL         }, /* levelparallaxpiece5 */
    { OT_PARALLAX + 6,  0,                                     NULL         }, /* levelparallaxpiece6 */
    { OT_PARALLAX + 7,  0,                                     NULL         }, /* levelparallaxpiece7 */
    { OT_WR_LEG + 1,    0,                                     NULL         }, /* minibosswretchnidleftleg2 */
    { OT_WR_LEG + 2,    0,                                     NULL         }, /* minibosswretchnidleftleg3 */
    { OT_WR_LEG + 3,    0,                                     NULL         }, /* minibosswretchnidleftleg4 */
    { OT_WR_LEG + 4,    0,                                     NULL         }, /* minibosswretchnidleftleg5 */
    { OT_WR_LEG + 5,    0,                                     NULL         }, /* minibosswretchnidleftleg6 */
    { OT_WR_LEG + 6,    0,                                     NULL         }, /* minibosswretchnidrightleg1 */
    { OT_WR_LEG + 7,    0,                                     NULL         }, /* minibosswretchnidrightleg2 */
    { OT_WR_LEG + 8,    0,                                     NULL         }, /* minibosswretchnidrightleg3 */
    { OT_WR_LEG + 9,    0,                                     NULL         }, /* minibosswretchnidrightleg4 */
    { OT_WR_LEG + 10,   0,                                     NULL         }, /* minibosswretchnidrightleg5 */
    { OT_WR_LEG + 11,   0,                                     NULL         }, /* minibosswretchnidrightleg6 */
    { OT_TD_PIECE + 1,  0,                                     NULL         }, /* minibosstreddleft2 */
    { OT_TD_PIECE + 2,  0,                                     NULL         }, /* minibosstreddleft3 */
    { OT_TD_PIECE + 3,  0,                                     NULL         }, /* minibosstreddleft4 */
    { OT_TD_PIECE + 4,  0,                                     NULL         }, /* minibosstreddleft5 */
    { OT_TD_PIECE + 5,  0,                                     NULL         }, /* minibosstreddleft6 */
    { OT_TD_PIECE + 6,  0,                                     NULL         }, /* minibosstreddright1 */
    { OT_TD_PIECE + 7,  0,                                     NULL         }, /* minibosstreddright2 */
    { OT_TD_PIECE + 8,  0,                                     NULL         }, /* minibosstreddright3 */
    { OT_TD_PIECE + 9,  0,                                     NULL         }, /* minibosstreddright4 */
    { OT_TD_PIECE + 10, 0,                                     NULL         }, /* minibosstreddright5 */
    { OT_TD_PIECE + 11, 0,                                     NULL         }, /* minibosstreddright6 */
    /* Relic Maze acts 1-2 (spec_rm_gimmicks_a 0.3: Q7 solid, Q8 obstacle, Q9 no-edge, Q23 hang, box = initialize_active(true)) */
    { OT_RAIL2,         0,                                     "rail2"      }, /* minibossrailing2 (visual of a gate) */
    { OT_RPANEL,        OD_SOLID,                              NULL         }, /* gimmickrelicpanel */
    { OT_RPIECE,        0,                                     NULL         }, /* - */
    { OT_FLIPPANEL,     OD_SOLID,                              "flippanel"  }, /* gimmickrelicflippanel */
    { OT_FLIPMARK2,     0,                                     NULL         }, /* gimmickrelicflippanel2 (marker) */
    { OT_FLIPMARK3,     0,                                     NULL         }, /* gimmickrelicflippanel3 */
    { OT_FLIPMARK4,     0,                                     NULL         }, /* gimmickrelicflippanel4 */
    { OT_FBSPAWN,       0,                                     "fbspawn"    }, /* gimmickfireballspawner */
    { OT_FBTURRET1,     OD_BOXCOL,                             "fbturret1"  }, /* gimmickfireballturret1 */
    { OT_FBTURRET2,     OD_BOXCOL,                             "fbturret2"  }, /* gimmickfireballturret2 */
    { OT_FBBACK,        OD_BOXCOL,                             "fbback"     }, /* gimmickfireballturretback */
    { OT_FBALL,         0,                                     NULL         }, /* - */
    { OT_EMBER,         0,                                     NULL         }, /* - */
    { OT_HBAR,          0,                                     "hbar"       }, /* gimmickhangingmovingbar */
    { OT_HSENS,         OD_BOXCOL|OD_HANG|OD_HIDDEN,          "hsens"      }, /* gimmickhangingmovingsensor (ctor set_visible 0: the red editor box, session 20) */
    { OT_HBAR2,         0,                                     "hbar2"      }, /* gimmickhangingmovingbar2 */
    { OT_HSENS2,        OD_BOXCOL|OD_HANG|OD_HIDDEN,          "hsens2"     }, /* gimmickhangingmovingsensor2 */
    { OT_HISWITCH,      OD_BOXCOL|OD_HANG|OD_HIDDEN,          "hiswitch"   }, /* gimmickhangingidleswitch */
    { OT_HSWITCHV,      0,                                     "hswitch"    }, /* gimmickhangingswitch */
    { OT_HIMOVER,       OD_BOXCOL|OD_HANG|OD_HIDDEN,          "himover"    }, /* gimmickhangingidlemover */
    { OT_HMOVERV,       0,                                     "hmover"     }, /* gimmickhangingmover */
    { OT_MFLOOR1,       OD_SOLID|OD_BOXCOL,                    "mfloor1"    }, /* gimmickmovingfloor1 */
    { OT_MFLOOR2,       OD_SOLID|OD_BOXCOL,                    "mfloor2"    }, /* gimmickmovingfloor2 */
    { OT_MFLOOR3,       OD_SOLID|OD_BOXCOL,                    "mfloor3"    }, /* gimmickmovingfloor3 */
    { OT_RPLAT,         OD_OBSTACLE|OD_NOEDGE,                 "rplat"      }, /* gimmickrollingplatform */
    { OT_RWHEEL,        0,                                     "rwheel"     }, /* gimmickrollingplatform2 */
    { OT_SPINBALL,      0,                                     "spinball"   }, /* gimmickspinball */
    { OT_CLOCK1,        0,                                     "clock1"     }, /* clockdial1 */
    { OT_CLOCK2,        0,                                     "clock2"     }, /* clockdial2 */
    { OT_AXLE,          OD_SOLID,                              "axle"       }, /* gimmickaxletunnel */
    { OT_BLIGHT,        OD_BOXCOL,                             "blight"     }, /* basementlight */
    { OT_COLFRONT,      OD_SOLID|OD_BOXCOL|OD_HIDDEN,          "colfront"   }, /* switchcolumnfront1 (ctor set_visible 0: image 43 is a solid green box) */
    { OT_COLBACK1,      OD_BOXCOL,                             "colback1"   }, /* switchcolumnback1 */
    { OT_COLBACK2,      OD_BOXCOL,                             "colback2"   }, /* switchcolumnback2 */
    { OT_RUINS,         OD_SOLID|OD_BOXCOL,                    "ruins"      }, /* breakableruins */
    { OT_RUINPIECE,     0,                                     NULL         }, /* - */
    { OT_DIGHOLE,       OD_SOLID|OD_BOXCOL,                    "dighole"    }, /* digspothole */
    { OT_LAYER_SW2,     OD_BOXCOL,                             NULL         }, /* layerchangeswitch2 (invisible) */
    { OT_WPLATE_RM,     0,                                     "wallplate"  }, /* switchwallplate_1149 */
    { OT_LL001,         OD_SOLID|OD_BOXCOL|OD_LAYER0,          "ll001"      }, /* levellayer001 */
    { OT_LL002,         OD_SOLID|OD_BOXCOL|OD_LAYER0,          "ll002"      }, /* levellayer002 */
    { OT_LL015,         OD_SOLID|OD_LAYER0,                    "ll015"      }, /* levellayer015 (mask) */
    { OT_LL02,          OD_SOLID|OD_BOXCOL|OD_LAYER0,          "ll02"       }, /* levellayer02 */
    { OT_LL101,         OD_SOLID|OD_BOXCOL|OD_LAYER1,          "ll101"      }, /* levellayer101 */
    { OT_LL116,         OD_SOLID|OD_LAYER1,                    "ll116"      }, /* levellayer116 (mask) */
    { OT_LL1L01,        OD_SOLID|OD_LAYER1,                    "ll1l01"     }, /* levellayer1loop01 (mask) */
    { OT_LL1L02,        OD_SOLID|OD_LAYER1,                    "ll1l02"     }, /* levellayer1loop02 (mask) */
    /* Relic Maze golems (spec_rm_golems: q8 obstacle + q9 for the eye, leg blocks and the spine) */
    { OT_GEYE,          OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* minibossgolem */
    { OT_GLID,          0,                                     NULL         }, /* minibossgolem2 */
    { OT_GP1,           OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* minibossgolempiece1 */
    { OT_GP2,           OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* minibossgolempiece2 */
    { OT_GSHOT,         0,                                     NULL         }, /* - */
    { OT_BGCORE,        OD_BOXCOL,                             NULL         }, /* minibossbonegolem */
    { OT_BGSPINE,       OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* minibossbonegolempiece5 */
    { OT_BGP3,          OD_BOXCOL,                             NULL         }, /* minibossbonegolempiece3 */
    { OT_BGPC,          OD_BOXCOL,                             NULL         }, /* minibossbonegolempiece */
    { OT_BGP6,          OD_BOXCOL,                             NULL         }, /* minibossbonegolempiece6 */
    { OT_BGN1,          0,                                     NULL         }, /* minibossbonegolemneck1 */
    { OT_BGN2,          0,                                     NULL         }, /* minibossbonegolemneck2 */
    { OT_BGN3,          0,                                     NULL         }, /* minibossbonegolemneck3 */
    { OT_BGN4,          0,                                     NULL         }, /* minibossbonegolemneck4 */
    { OT_BGHEAD,        0,                                     NULL         }, /* minibossbonegolemhead */
    { OT_SHOCK,         0,                                     NULL         }, /* - */
    { OT_CAMLIM_L,      0,                                     NULL         }, /* cameralimitbossleft */
    { OT_CAMLIM_R,      0,                                     NULL         }, /* cameralimitbossright */
    { OT_CAMLIM_T,      0,                                     NULL         }, /* cameralimitbosstop */
    { OT_CAMLIM_B,      0,                                     NULL         }, /* cameralimitbossbottom */
    { OT_IRIS,          0,                                     NULL         }, /* enemyiris (relic.c) */
    { OT_SLUG,          0,                                     NULL         }, /* enemyslug */
    { OT_HOPPER,        0,                                     NULL         }, /* enemyhopper */
    { OT_FSPITTER,      0,                                     NULL         }, /* enemyfirespitter */
    { OT_SENTINEL,      0,                                     NULL         }, /* enemysentinel */
    { OT_EXBASE,        0,                                     NULL         }, /* enemyextractorbase */
    { OT_EXHEAD,        0,                                     NULL         }, /* enemyextractorhead */
    { OT_POT,           0,                                     NULL         }, /* objectpot */
    { OT_BLUEPOT,       0,                                     NULL         }, /* objectbluepot */
    { OT_FSPRITE,       0,                                     NULL         }, /* - */
    { OT_SPARK,         0,                                     NULL         }, /* - */
    { OT_SSPIKE,        0,                                     NULL         }, /* - */
    { OT_FLAME,         0,                                     NULL         }, /* - */
    { OT_FSSHELL,       0,                                     NULL         }, /* - */
    /* Relic Maze acts 3-5 gimmicks (relic_gimb.c, spec_rm_gimmicks_b 1 / 20.1): sprites set by relic_gimb_spawn */
    { OT_RM_PILLAR,     OD_SOLID,                              NULL         }, /* gimmickmovingpillar1/2/3_1395-1397 (Q7) */
    { OT_RM_IDLESW,     0,                                     NULL         }, /* gimmickhangingidleswitch_1268 (Q23 hang) */
    { OT_RM_HSWITCH,    0,                                     NULL         }, /* gimmickhangingswitch_1398 */
    { OT_RM_RSWING,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* gimmickrisingswing_1392 (Q8 Q9 Q13) */
    { OT_RM_RSPIECE,    0,                                     NULL         }, /* gimmickrisingswingpiece_1393 */
    { OT_RM_PICKAXE,    0,                                     NULL         }, /* gimmickpickaxe_1335 */
    { OT_RM_PXPART,     0,                                     NULL         }, /* gimmickpickaxehandle_1336 / base_1337 / base2_1383 */
    { OT_RM_MARKER,     0,                                     NULL         }, /* gimmickrelicflippanel2_1245 */
    { OT_RM_CUP,        0,                                     NULL         }, /* gimmickspinnycup_1380 */
    { OT_RM_ELEV,       OD_BOXCOL,                             NULL         }, /* gimmickelevator_1340 */
    { OT_RM_ELEV2,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickelevator2_1399 (Q7 Q13) */
    { OT_RM_CART,       OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* levelhandcar_1421 (Q8 Q9) */
    { OT_RM_CARTPART,   OD_BOXCOL,                             NULL         }, /* levelhandcarhandle_1420 / top_1422 */
    { OT_RM_CARTPROBE,  0,                                     NULL         }, /* levelhandcarfloor1/2/3_1423-1425 */
    { OT_RM_TSWITCH,    OD_SOLID,                              NULL         }, /* levelminetrackswitch_1419 (Q7) */
    { OT_RM_WPLATE,     0,                                     NULL         }, /* switchwallplate_1149 / 1526 / 1543, switchwallplate2_1429 / 1578 */
    { OT_RM_SPIKEBIT,   0,                                     NULL         }, /* - (BreakableSpike_1436) */
    { OT_RM_FLOATBOX,   OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* levelfloatingplatform2_1311 / 3_1403 (Q8 Q9) */
    { OT_RM_MOVPLAT,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelmovingplatform01_1389 (Q8 Q9 Q13) */
    { OT_RM_FLOORBOX,   OD_SOLID,                              NULL         }, /* active_1338 (Q7) */
    { OT_RM_ALTPATH,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* rmalternatepath_1426 / rmalternatepath2_1438 (Q7) */
    { OT_RM_RCAVE,      OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* gimmickrollingplatformcave_1449 (Q8 Q9 Q13) */
    { OT_RM_RCAVE2,     0,                                     NULL         }, /* gimmickrollingplatformcave2_1447 */
    { OT_RM_DRILL,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickdrillplatform_1480 (Q7) */
    { OT_RM_RUINS,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* breakableruins_1450 (Q7) */
    { OT_RM_RMPLAT,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelrmplatform_1517 (Q8 Q9) */
    { OT_RM_GATE,       0,                                     NULL         }, /* gatewayfront_1524 */
    { OT_RM_GATE2,      0,                                     NULL         }, /* gatewayfront2_1525 */
    { OT_RM_CRATER,     0,                                     NULL         }, /* cavesoilcraterfront_1379 / front2_1385 */
    { OT_RM_LOOP0,      OD_SOLID,                              NULL         }, /* levellayer001/002/015/02/03 (RM4 handles, Q7 Q10) */
    { OT_RM_LOOP1,      OD_SOLID,                              NULL         }, /* levellayer117_1488 / 1loop2_1489 (Q7 Q11) */
    { OT_RM_LSBASE,     0,                                     NULL         }, /* lightstonebase_1562 / base2_1564 */
    { OT_RM_LSTOP,      0,                                     NULL         }, /* lightstonetop_1563 / top2_1565 */
    { OT_RM_LSDOOR,     OD_SOLID,                              NULL         }, /* lightstonedoor1/2_1585-1586 (Q7) */
    { OT_RM_SCEIL,      OD_SOLID,                              NULL         }, /* spikeceiling_1557 (Q7) */
    { OT_RM_CRUSHBOX,   OD_BOXCOL,                             NULL         }, /* levelcrushdeath_1627 */
    { OT_RM_CHAIR,      0,                                     NULL         }, /* objectchair_1596 (Q22) */
    /* Fortune Night acts 4-5 gimmicks (fn_gimb.c, spec_fn_gimmicks_b 1 / 26.1): sprites set by fn_gimb_spawn */
    { OT_FN_ESCFRONT,   0,                                     NULL         }, /* escalatorfront1_1906 / front2_1914 */
    { OT_FN_ESCSTEP,    OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* escalatorright_1908 / left_1915 (Q8 Q9, invisible) */
    { OT_FN_FOUNTAIN,   0,                                     NULL         }, /* fountainfront_1918 / front2_1919 */
    { OT_FN_WATERPLAT,  OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* levelwaterplatform_1921 (Q8 Q9, invisible) */
    { OT_FN_ROPE,       OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickclothesline_1987 (invisible) */
    { OT_FN_TBAR,       OD_BOXCOL,                             NULL         }, /* gimmickhangingmovingbar_1951 */
    { OT_FN_TSENS,      OD_HANG|OD_BOXCOL|OD_HIDDEN,           NULL         }, /* gimmickhangingmovingsensor_1950 (Q23, invisible) */
    { OT_FN_TWHEEL,     0,                                     NULL         }, /* gimmickhangingmovingwheel_1952 */
    { OT_FN_TSTOP,      OD_BOXCOL,                             NULL         }, /* gimmickhangingmovingstopper_1949 */
    { OT_FN_LIFT,       OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelmovingplatform1_1957 (Q8 Q9 Q13) */
    { OT_FN_STILT,      OD_BOXCOL,                             NULL         }, /* platformstilt1-4_1958-1961 (layer 2) */
    { OT_FN_MPLAT2,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelmovingplatform2_1976 (Q8 Q9 Q12 Q13) */
    { OT_FN_TSPRING,    OD_SOLID|OD_NOEDGE|OD_BOXCOL,          NULL         }, /* turningspring_1944 (Q7 Q9) */
    { OT_FN_AIRBALLOON, OD_BOXCOL,                                NULL         }, /* gimmickairballoon_1991 */
    { OT_FN_IBALLOON,   OD_HANG|OD_BOXCOL|OD_HIDDEN,           NULL         }, /* gimmickhangingidleballoon_1992 (Q23, invisible) */
    { OT_FN_KEY,        OD_SOLID,                              NULL         }, /* keyboard_1984 (Q7) */
    { OT_FN_DISCOLIGHT, OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* discolight_1932 (screen overlay, drawn by fn_gimb.c) */
    { OT_FN_DISCOBALL,  0,                                     NULL         }, /* objectdiscoball_1930 / ball2_1933 */
    { OT_FN_PARASOL,    0,                                     NULL         }, /* objectparasol_1940 */
    { OT_FN_RESTROOM,   0,                                     NULL         }, /* restroommen_1973 / women_1974 (Q22) */
    { OT_FN_ARROW,      0,                                     NULL         }, /* arrowlight_1971 */
    { OT_FN_CAROUSEL,   OD_BOXCOL,                             NULL         }, /* carousel_2004 (drawn by fn_gimb.c) */
    { OT_FN_CARPLAT,    OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* carouselplatform_2005 (Q8 Q9) */
    { OT_FN_POD,        OD_SOLID,                              NULL         }, /* gimmickelevator_2035 (Q7) */
    { OT_FN_PODDOOR,    0,                                     NULL         }, /* gimmickelevatordoor_2034 */
    { OT_FN_PODCASE,    0,                                     NULL         }, /* gimmickpolepodcaseinner_2001 */
    { OT_FN_LOOP0B,     OD_SOLID|OD_LAYER0,                    NULL         }, /* layer0loop4/5/6_1977/1978/1983 (Q7 Q10) */
    { OT_FN_NOTE,       0,                                     NULL         }, /* - (EffectsHeart_444, 8209) */
    { OT_FN_MICON,      0,                                     NULL         }, /* - (MonitorIcon_363, 8214 / 8215) */
    /* Relic Maze act 3 Claw sequence (rm_claw.c, spec_rm_claw.md 1; sprites set by rm_claw_spawn) */
    { OT_SHRINE3,       0,                                     "shrine3"    }, /* shrine3 (Q20, armoured) */
    { OT_CLAW,          0,                                     "claw0"      }, /* bossclaw */
    { OT_CLAW_TALONS,   0,                                     "talons0"    }, /* bossclawtalons */
    { OT_CLAW_LINK,     0,                                     "clink"      }, /* bossclawpiece1 */
    { OT_CLAW_JOINT,    0,                                     "clink"      }, /* bossclawpiece4 */
    { OT_CLAW_HITBOX,   0,                                     "clawhit"    }, /* bossclawhitbox (Q20) */
    { OT_CLAW_BASE,     OD_SOLID,                              "clawbase"   }, /* bossclawbase (Q7) */
    { OT_CLAW_BEAM,     0,                                     NULL         }, /* - (BossReflectoBeam_1321) */
    { OT_CAR_ENGINE,    0,                                     "engine"     }, /* caravanengine (Q20) */
    { OT_CAR_WHEEL,     OD_BOXCOL,                             "cwheel"     }, /* caravanwheel */
    { OT_MERC,          0,                                     "merc"       }, /* npcmerc */
    { OT_PBLOCK3,       OD_SOLID|OD_NOEDGE|OD_BOXCOL,          "pblock3"    }, /* pushableblock3 (Q7 Q9 Q14) */
    { OT_CAR_GROUND,    OD_SOLID,                              "cground"    }, /* active_1338 (Q7, invisible) */
    { OT_CAR_ROOF2,     OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       "croof2"     }, /* levelfloatingplatform2 (Q8 Q9, invisible) */
    { OT_CAR_ROOF3,     OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       "croof3"     }, /* levelfloatingplatform3 (Q8 Q9, invisible) */
    { OT_BGWALL,        0,                                     "bgwall"     }, /* backgroundwall (layer 1) */
    { OT_BGWALL2,       0,                                     "bgwall2"    }, /* backgroundwall2 (layer 1) */
    { OT_COLUMN1,       0,                                     "column1"    }, /* breakablecolumn1 */
    { OT_COLUMN2,       0,                                     "column2"    }, /* breakablecolumn2 */
    { OT_RELICWALL,     0,                                     "relicwall"  }, /* breakablerelicwall */
    { OT_RELICDEBRIS,   0,                                     NULL         }, /* - (BreakableRelicWallDebris_1297) */
    { OT_SHRINE2,       0,                                     "shrine2"    }, /* shrine2_1296 */
    { OT_ALTPATH,       OD_SOLID,                              "altpath"    }, /* rmalternatepath_1426 (Q7; Lilac / Carol) */
    { OT_ALTPATH2,      OD_SOLID|OD_BOXCOL,                    "altpath2"   }, /* rmalternatepath2_1438 (Q7 box; Milla) */
    /* Relic Maze act 5 Mantalith / Kingdom Stone (rm_mantalith.c, spec_rm_mantalith 0; sprites set by its spawn code) */
    { OT_MT_STONE,      0,                                     NULL         }, /* kingdomstone_1589 */
    { OT_MT_SPADE,      0,                                     NULL         }, /* placeholdercharacter_1625 (Spade) */
    { OT_MT_HOOK,       0,                                     NULL         }, /* npchookline_1590 */
    { OT_MT_KS1,        0,                                     NULL         }, /* ksprojectile1_1593 */
    { OT_MT_KS2,        0,                                     NULL         }, /* ksprojectile2_1626 */
    { OT_MT_RAY1,       0,                                     NULL         }, /* lightray_1591 */
    { OT_MT_RAY2,       0,                                     NULL         }, /* lightray2_1592 */
    { OT_MT_SHRINE,     0,                                     NULL         }, /* shrine2_1588 */
    { OT_MT_CUT1,       0,                                     NULL         }, /* bossgroundcut1_1600 */
    { OT_MT_CUT2,       0,                                     NULL         }, /* bossgroundcut2_1601 */
    { OT_MT_BODY,       0,                                     NULL         }, /* bossmantalithbody_1603 */
    { OT_MT_NECK4,      0,                                     NULL         }, /* bossmantalithneck4_1604 */
    { OT_MT_NECK3,      0,                                     NULL         }, /* bossmantalithneck3_1605 */
    { OT_MT_NECK2,      0,                                     NULL         }, /* bossmantalithneck2_1610 */
    { OT_MT_NECK1,      0,                                     NULL         }, /* bossmantalithneck1_1611 */
    { OT_MT_LKNEE,      0,                                     NULL         }, /* bossmantalithleftknee_1606 */
    { OT_MT_RKNEE,      0,                                     NULL         }, /* bossmantalithrightknee_1607 */
    { OT_MT_LEG2,       0,                                     NULL         }, /* bossmantalithleg2_1609 (left foot) */
    { OT_MT_LEG3,       0,                                     NULL         }, /* bossmantalithleg3_1608 (right foot) */
    { OT_MT_BEAD,       0,                                     NULL         }, /* bossmantalithleg1_1602 (16 beads) */
    { OT_MT_HEAD,       0,                                     NULL         }, /* bossmantalithhead_1614 (q20) */
    { OT_MT_HEAD2,      0,                                     NULL         }, /* bossmantalithhead2_1613 */
    { OT_MT_HEAD3,      0,                                     NULL         }, /* bossmantalithhead3_1612 */
    { OT_MT_ARM6,       0,                                     NULL         }, /* bossmantalitharm6_1615 */
    { OT_MT_ARM3,       0,                                     NULL         }, /* bossmantalitharm3_1616 */
    { OT_MT_HIT1,       0,                                     NULL         }, /* bossmantalithhitbox1_1618 (q20) */
    { OT_MT_HIT2,       0,                                     NULL         }, /* bossmantalithhitbox2_1619 (q20) */
    { OT_MT_WARN,       0,                                     NULL         }, /* effectwarning_1528 */
    { OT_MT_DEBRIS,     0,                                     NULL         }, /* - (BossMantalithDebris_1617, EffectsDebris_427) */
    { OT_MT_SLASH,      0,                                     NULL         }, /* - (BossSlash_1598) */
    { OT_MT_CHOMP,      0,                                     NULL         }, /* - (BossChomp_1599) */
    { OT_MT_RUIN,       0,                                     NULL         }, /* - (BreakableRuins_1450, post-results) */
    { OT_MT_RPIECE,     0,                                     NULL         }, /* - (BreakableRuinsPiece_1451) */
    /* Fortune Night enemies (fn_enemy.c, spec_fn_enemies 0; sprites set by fn_enemy_spawn; none is in Q7 / Q8 / Q19) */
    { OT_FN_COIL,       OD_HIDDEN,                             "fc_base"    }, /* enemycoil_1742 (invisible pivot) */
    { OT_FN_COILHEAD,   0,                                     "fc_head"    }, /* enemycoilhead_1743 (Q20) */
    { OT_FN_COILP1,     0,                                     "fc_p1"      }, /* enemycoilpiece1_1738 */
    { OT_FN_COILP2,     0,                                     "fc_p2"      }, /* enemycoilpiece2_1740 */
    { OT_FN_COILP3,     0,                                     "fc_p3"      }, /* enemycoilpiece3_1739 */
    { OT_FN_COILP4,     0,                                     "fc_p4"      }, /* enemycoilpiece4_1741 */
    { OT_FN_COILFLIP,   OD_HIDDEN,                             "fc_flip"    }, /* enemycoilflip_1759 (marker) */
    { OT_FN_TPOLE,      0,                                     "ft_pole"    }, /* enemytrafficle_1760 */
    { OT_FN_TLIGHT1,    0,                                     "ft_l1"      }, /* enemytrafficle1_1761 (Q20) */
    { OT_FN_TLIGHT2,    0,                                     "ft_l2"      }, /* enemytrafficle2_1762 (Q20) */
    { OT_FN_TLIGHT3,    0,                                     "ft_l3"      }, /* enemytrafficle3_1763 (Q20) */
    { OT_FN_STURRET,    OD_HIDDEN,                             "st_fly"     }, /* enemysuperturretus_1833 (Q20) */
    { OT_FN_FCANNON,    0,                                     "fk_idle"    }, /* enemyflarecannon_1808 (Q20) */
    { OT_FN_ROLLER,     0,                                     "ro_idle_l"  }, /* enemyroller_1934 (Q20) */
    { OT_FN_SHELL,      0,                                     "gs_idle_l"  }, /* enemyghettoshell_1988 (Q20) */
    { OT_FN_DUSTER,     0,                                     "du_body"    }, /* enemyduster_1938 (Q20) */
    { OT_FN_DARM2,      0,                                     "du_arm2"    }, /* enemydusterarm2_1935 */
    { OT_FN_DARM1,      0,                                     "du_arm1"    }, /* enemydusterarm1_1936 */
    { OT_FN_DMOUTH,     0,                                     "du_mouth"   }, /* enemydustermouth_1937 */
    { OT_FN_BALLOON,       0,                                     "bl_float"   }, /* enemyballoon_1945 (Q20) */
    { OT_FN_PIGEON,     0,                                     "pg_sit"     }, /* pigeon_1641 (decor) */
    { OT_FN_FBURST,     0,                                     "fk_burst"   }, /* effectflareburst_1810 (off-map template) */
    { OT_FN_LASER,      0,                                     NULL         }, /* - (EnemyLaser_1781) */
    { OT_FN_FLARE,      0,                                     NULL         }, /* - (EnemyFlare_1809) */
    { OT_FN_WAVE,       0,                                     NULL         }, /* - (EnemySoundwave_1989) */
    /* Fortune Night acts 1-3 gimmicks (fn_gima.c, spec_fn_gimmicks_a 0.1 / 0.3; sprites set by fn_gima_spawn). Flag 0 (fl bit 0) drops a
     * solid / obstacle instance from the sensor tests: dragon masks (7199), arch base (7225), half-pipe walls (7535), loop layers */
    { OT_FN_DSEG,       0,                                     NULL         }, /* gimmickdragontunnel1_1734 / tunnel2_1733 (Q13) */
    { OT_FN_DSEGP,      OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* gimmickdragontunnel1_1860 (A3: Q8 Q9 Q13) */
    { OT_FN_DHEAD,      0,                                     NULL         }, /* gimmickdragontunnelhead_1737 / head2_1783 */
    { OT_FN_DMASK,      OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* gimmickdragontunnelmask1_1798 / mask2_1799 (Q8 Q9, ctor invisible) */
    { OT_FN_DHIDE,      0,                                     NULL         }, /* dragontunnelhide1/2/3 */
    { OT_FN_FCAR,       OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelferriswheel1-4 (Q8 Q9 Q13) */
    { OT_FN_FSPOKE,     0,                                     NULL         }, /* levelferriswheelpiece_1778 */
    { OT_FN_GOLD,       OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* goldplatform1/2/3 (Q8 Q9) */
    { OT_FN_ARCHBASE,   OD_SOLID|OD_HIDDEN,                    NULL         }, /* gimmickrollingarchbase_1659 (Q7, ctor invisible) */
    { OT_FN_ARCH,       OD_BOXCOL,                             NULL         }, /* gimmickrollingarch_1661 */
    { OT_FN_ARCHWHEEL,  0,                                     NULL         }, /* gimmickrollingarch2_1663 */
    { OT_FN_BOOST,      OD_BOXCOL,                             NULL         }, /* gimmickboostblock_1728 (Q20) */
    { OT_FN_BFLAME,     OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickboostblockflame_1731 */
    { OT_FN_BTURN,      OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickboostblockturn_1730 */
    { OT_FN_LBRIDGE,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmicklanternbridge1-8 (Q7; links 1-2 also Q10) */
    { OT_FN_HPIPE,      OD_SOLID|OD_HIDDEN,                    NULL         }, /* gimmickverticalhalfpipe1-6 (Q7) */
    { OT_FN_TEETER,     OD_SOLID,                              NULL         }, /* gimmickteetertotter_1831 (Q7) */
    { OT_FN_BWALL,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* breakablewall_1637 (Q7) */
    { OT_FN_BWPIECE,    0,                                     NULL         }, /* - (BreakableWallPiece_1638) */
    { OT_FN_BFLOOR,     OD_SOLID,                              NULL         }, /* breakablefloor1_1766 / floor2_1767 (Q7) */
    { OT_FN_BFPIECE,    0,                                     NULL         }, /* - (BreakableFloorPiece_1764) */
    { OT_FN_FWLAUNCH,   OD_HIDDEN,                             NULL         }, /* fireworklauncher_1735 */
    { OT_FN_FWPART,     0,                                     NULL         }, /* - (FireworkParticle_1736) */
    { OT_FN_CLOTHES,    OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickclothesline2_1702 */
    { OT_FN_CLSTOP,     OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickhanginglongsensor2_1703 */
    { OT_FN_FLAG,       0,                                     NULL         }, /* flag_1701 */
    { OT_FN_PLANTERN,   0,                                     NULL         }, /* paperlantern_527 */
    { OT_FN_FLANTERN,   0,                                     NULL         }, /* floatinglantern_1846 */
    { OT_FN_GWHEEL,     OD_BOXCOL,                             NULL         }, /* gachaponwheel_1851 */
    { OT_FN_GDOOR,      OD_BOXCOL,                             NULL         }, /* gachapondoor_1852 */
    { OT_FN_GFRONT,     0,                                     NULL         }, /* gachaponfront_1854 */
    { OT_FN_SIGN,       0,                                     NULL         }, /* signstop_1698 / signyield_1697 */
    { OT_FN_LAMP,       0,                                     NULL         }, /* lamppostfront_1791 (Q20) */
    { OT_FN_STROBE,     0,                                     NULL         }, /* strobelight_1857 */
    { OT_FN_LSIGN,      0,                                     NULL         }, /* largesign1/2/3 */
    { OT_FN_COLUMN,     OD_SOLID|OD_BOXCOL,                    NULL         }, /* largecolumn_1864 / largecolumn2_1874 (A3 Q7; boss-driven) */
    { OT_FN_RAMP,       OD_SOLID,                              NULL         }, /* ramp1_1870 / ramp2_1871 (A3 Q7) */
    { OT_FN_TROLLEY,    0,                                     NULL         }, /* leveltrollycar_1803 (SystemBox) */
    { OT_FN_TPLAT,      OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* leveltrolleyplatform_1806 / 2_1807 (Q8 Q9) */
    { OT_FN_TLIGHT,     0,                                     NULL         }, /* leveltrollycarlight_1812 / light2_1813 */
    { OT_FN_LOOP0,      OD_SOLID|OD_LAYER0,                    NULL         }, /* layer0loop / 0loop2 / 0loop3 / 0block (Q7 Q10) */
    { OT_FN_LOOP1,      OD_SOLID|OD_LAYER1,                    NULL         }, /* layer1loop / 1loop2 / 1loop3 (Q7 Q11) */
    { OT_FN_STAR,       0,                                     NULL         }, /* bgstar_16 */
    { OT_FN_PARAM,      0,                                     NULL         }, /* - (importer: per-act constants) */
    { OT_FN_DUST,       0,                                     NULL         }, /* - (EffectsDust01_369) */
    /* Fortune Night bosses (fn_boss.c, spec_fn_bosses A-E; sprites set by fn_boss_spawn). OD_HIDDEN: the constructor's
     * set_visible(0) (shown by the encounter's events) */
    { OT_FN_UFO,          OD_HIDDEN,                           NULL         }, /* minibossufo_1772 (Q20) */
    { OT_FN_UFO_PIECE,    OD_HIDDEN,                           NULL         }, /* minibossufo2_1773 */
    { OT_FN_UFO_HATCH,    OD_HIDDEN,                           NULL         }, /* minibossufohatch1_1770 / hatch2_1771 */
    { OT_FN_UFO_BEAM,     OD_HIDDEN,                           NULL         }, /* minibossufobeam_1769 */
    { OT_FN_UFO_BORDER,   OD_HIDDEN,                           NULL         }, /* minibossufoborder_1768 (marker) */
    { OT_FN_UFO_DEBRIS,   0,                                   NULL         }, /* minibossufodebris_1875 */
    { OT_FN_DOZER,        0,                                   NULL         }, /* minibossbulldozer_1746 (Q20) */
    { OT_FN_DOZER_ARM,    0,                                   NULL         }, /* minibossbulldozerarm_1747 */
    { OT_FN_DOZER_CLAW,   OD_BOXCOL,                           NULL         }, /* minibossbulldozerclaw_1750 (box collision) */
    { OT_FN_DOZER_DOOR,   0,                                   NULL         }, /* minibossbulldozerdoor_1745 (Q20) */
    { OT_FN_DOZER_WHEEL,  0,                                   NULL         }, /* minibossbulldozerwheel1_1748 / wheel2_1749 */
    { OT_FN_DOZER_HITBOX, OD_SOLID|OD_HIDDEN,                  NULL         }, /* minibossbulldozerhitbox_1752 (Q7) */
    { OT_FN_LIMITER,      OD_HIDDEN,                           NULL         }, /* minibosslimiter_1751 (marker) */
    { OT_FN_BOOMERANG,    0,                                   NULL         }, /* - (MinibossBoomerang_1753) */
    { OT_FN_DOZER_DEBRIS, 0,                                   NULL         }, /* - (MinibossBulldozerDebris_1754) */
    { OT_FN_SERP,         OD_HIDDEN,                           NULL         }, /* npcserpentine_945 (FN3 boss) */
    { OT_FN_SERP_ARM,     OD_HIDDEN,                           NULL         }, /* bossserpentinearm_1865 */
    { OT_FN_SERP_HIT,     OD_HIDDEN,                           NULL         }, /* bossserphitbox_1873 (Q20) */
    { OT_FN_SERP_FLOOR,   OD_HIDDEN|OD_BOXCOL,                 NULL         }, /* bossserpfloorcheck_1869 (box) */
    { OT_FN_SERP_SHIELD,  OD_HIDDEN,                           NULL         }, /* shieldserpentine_1872 */
    { OT_FN_CHARGE_GLOW,  OD_HIDDEN,                           NULL         }, /* bosschargeglow_1868 */
    { OT_FN_WARNING,      0,                                   NULL         }, /* - (EffectWarning_1528) */
    { OT_FN_WARNING2,     0,                                   NULL         }, /* - (EffectWarning2_1895) */
    { OT_FN_RAPIDBULLET,  0,                                   NULL         }, /* - (BossRapidBullet_1866) */
    { OT_FN_MISSILE,      0,                                   NULL         }, /* - (BossMissile_1867) */
    { OT_FN_SAMU,         0,                                   NULL         }, /* minibosssamu_1890 (Q20) */
    { OT_FN_SAMU_HAND,    0,                                   NULL         }, /* minibosssamulefthand_1888 / righthand_1889 */
    { OT_FN_SAMU_WHEEL,   0,                                   NULL         }, /* minibosssamuwheel_1886 */
    { OT_FN_SAMU_PROBE,   OD_HIDDEN,                           NULL         }, /* minibosssamufloorcheck_1891 */
    { OT_FN_SAMU_DEBRIS,  0,                                   NULL         }, /* - (MinibossSamuDebris_1892) */
    { OT_FN_BOSSGATE,     OD_SOLID,                            NULL         }, /* bossgate_1894 / _2030 (Q7) */
    /* Sky Battalion bosses (sb_boss*.c, spec_sb_bosses; sprites set by sb_boss_spawn). OD_HIDDEN: the constructor's set_visible(0) */
    { OT_SB_ASSIST,      OD_HIDDEN,                             NULL         }, /* playerassist_2205 (shown by 9738) */
    { OT_SB_COPILOT,     OD_HIDDEN,                             NULL         }, /* playerassistcopilot_2220 (shown by 9744) */
    { OT_SB_COMPASS,     OD_HIDDEN,                             NULL         }, /* guicompass_2246 (shown at the boss end) */
    { OT_SB_SAT,         0,                                     NULL         }, /* bosssatellite_2286 (Q20) */
    { OT_SB_SAT_TOP,     0,                                     NULL         }, /* bosssatellitetop_2284 */
    { OT_SB_SAT_THR,     0,                                     NULL         }, /* bosssatellitethrusters_2285 */
    { OT_SB_SAT_ARM,     0,                                     NULL         }, /* bosssatellitearm_2287 */
    { OT_SB_SAT_BEAM,    OD_HIDDEN,                             NULL         }, /* minibossufobeam_1769 (SB3) */
    { OT_SB_SAT_BEAM2,   OD_HIDDEN,                             NULL         }, /* minibossufobeam2_2288 */
    { OT_SB_KJ_BASE,     0,                                     NULL         }, /* bosskujackerbase_2118 */
    { OT_SB_KJ_HEAD,     0,                                     NULL         }, /* bosskujackerhead_2125 (Q20) */
    { OT_SB_KJ_HEAD2,    0,                                     NULL         }, /* bosskujackerhead2_2119 */
    { OT_SB_KJ_LEG1,     0,                                     NULL         }, /* bosskujackerleg1_2117 */
    { OT_SB_KJ_LEG2,     0,                                     NULL         }, /* bosskujackerleg2_2116 */
    { OT_SB_KJ_PILOT,    0,                                     NULL         }, /* bosskujackerpilot_2124 */
    { OT_SB_KJ_TAIL,     0,                                     NULL         }, /* bosskujackertail_2115 */
    { OT_SB_KJ_TARGET,   OD_HIDDEN,                             NULL         }, /* bosskujackertailtarget_2120 (Q20) */
    { OT_SB_KJ_WING1,    0,                                     NULL         }, /* bosskujackerwing1_2122 */
    { OT_SB_KJ_WING2,    0,                                     NULL         }, /* bosskujackerwing2_2123 */
    { OT_SB_TORNADO,     OD_HIDDEN,                             NULL         }, /* bosstornado_2131 */
    { OT_SB_KJ_CHARGE,   0,                                     NULL         }, /* effectschargebeam_2130 */
    { OT_SB_KJ_BOLT,     0,                                     NULL         }, /* - (BossBolt) */
    { OT_SB_KJ_LIGHTNING, 0,                                     NULL         }, /* - (BossLightning2) */
    { OT_SB_KJ_TRACKER,  0,                                     NULL         }, /* - (BossKujackerTailTracker) */
    { OT_SB_KJ_GUST,     0,                                     NULL         }, /* - (BossGust) */
    { OT_SB_KJ_GUST2,    0,                                     NULL         }, /* - (BossGust2) */
    { OT_SB_WARNING,     0,                                     NULL         }, /* - (EffectWarning_1528) */
    { OT_SB_CH,          0,                                     NULL         }, /* bosschopper_2216 (Q20) */
    { OT_SB_CH_BLADE,    0,                                     NULL         }, /* bosschopperblade_2217 */
    { OT_SB_CH_TAIL,     0,                                     NULL         }, /* bosschoppertail_2215 */
    { OT_SB_CH_TBLADE,   0,                                     NULL         }, /* bosschoppertailblade_2218 */
    { OT_SB_CH_TBLADE2,  0,                                     NULL         }, /* bosschoppertailblade2_2214 */
    { OT_SB_CH_BARS,     0,                                     NULL         }, /* bosschopperbars_2212 */
    { OT_SB_CH_BARS2,    0,                                     NULL         }, /* bosschopperbars2_2213 */
    { OT_SB_MISSILE,     0,                                     NULL         }, /* - (BossMissile_2219) */
    { OT_SB_TREDD,       0,                                     NULL         }, /* - (MinibossTreddBomb_1060) */
    { OT_SB_SHIPFALL,    0,                                     NULL         }, /* zaoshipfalling */
    { OT_SB_SP_MASK,     OD_HIDDEN,                             NULL         }, /* bossspademask_2248 (state holder) */
    { OT_SB_SPADE,       0,                                     NULL         }, /* bossspade_2247 */
    { OT_SB_SP_HIT,      OD_HIDDEN,                             NULL         }, /* bossspadehitbox_2250 (Q20) */
    { OT_SB_SP_CARD,     0,                                     NULL         }, /* - (BossSpadeCard) */
    { OT_SB_SP_IMG,      0,                                     NULL         }, /* - (EffectsSpadeImage_456) */
    { OT_SB_GR,          0,                                     NULL         }, /* bossgrappler_2331 (Q20) */
    { OT_SB_GR_HANDL,    0,                                     NULL         }, /* bossgrapplerhandleft_2330 */
    { OT_SB_GR_HANDR,    0,                                     NULL         }, /* bossgrapplerhandright_2329 */
    { OT_SB_GR_ARML,     0,                                     NULL         }, /* bossgrapplerarmleft_2327 */
    { OT_SB_GR_ARMR,     0,                                     NULL         }, /* bossgrapplerarmright_2328 */
    { OT_SB_SPIRE,       OD_SOLID,                              NULL         }, /* bosscrystalspire_2333 (Q7) */
    { OT_SB_SPAWN1,      OD_HIDDEN,                             NULL         }, /* bosscrystalspikespawner1_2325 */
    { OT_SB_SPAWN2,      OD_HIDDEN,                             NULL         }, /* bosscrystalspikespawner2_2326 */
    { OT_SB_SPIKE,       0,                                     NULL         }, /* - (BossCrystalSpike) */
    { OT_SB_SPARK,       0,                                     NULL         }, /* - (BossHomingSpark) */
    { OT_SB_CPLAT,       OD_OBSTACLE|OD_HIDDEN,                 NULL         }, /* crystalplatform_2334 (Q8) */
    { OT_SB_LSPIKE,      0,                                     NULL         }, /* - (LevelCrystalSpike_2268) */
    /* Fortune Night Panther (fn_panther.c, spec_fn_bosses G.1; sprites set by fn_panther_spawn). The scrolling floor is the
     * arena's floor (Q7 box: the frame's floor backdrops start at x 672, outside the 640-wide arena; RV T-G1: the player
     * stands at feet y 280 = its top); the plane platform carries Milla (Q8 / Q9, INFERRED from 8524-8527) */
    { OT_FNP_VALUES,    OD_HIDDEN,                             NULL         }, /* bosspanthervalues_2023 (controller) */
    { OT_FNP_HEAD,      0,                                     NULL         }, /* bosspantherhead_2013 */
    { OT_FNP_JAW,       0,                                     NULL         }, /* bosspantherjaw_2014 */
    { OT_FNP_FCAN,      0,                                     NULL         }, /* bosspantherfrontcannon_2015 */
    { OT_FNP_BCAN,      0,                                     NULL         }, /* bosspantherbackcannon_2038 */
    { OT_FNP_RIDER,     0,                                     NULL         }, /* bosspantherrider_2039 / _2009 */
    { OT_FNP_SHF,       0,                                     NULL         }, /* bosspanthershoulderf_2019 */
    { OT_FNP_SHF2,      0,                                     NULL         }, /* bosspanthershoulderf2_2020 */
    { OT_FNP_HIP,       0,                                     NULL         }, /* bosspantherhipf_2016 */
    { OT_FNP_KNEE,      0,                                     NULL         }, /* bosspantherkneef_2017 */
    { OT_FNP_LEGFL,     0,                                     NULL         }, /* bosspantherlegfl_2018 */
    { OT_FNP_LEGFR,     0,                                     NULL         }, /* bosspantherlegfr_2022 */
    { OT_FNP_FOOTFL,    0,                                     NULL         }, /* bosspantherfootfl_2012 */
    { OT_FNP_FOOTFR,    0,                                     NULL         }, /* bosspantherfootfr_2021 */
    { OT_FNP_RIBS,      0,                                     NULL         }, /* bosspantherribs_2011 */
    { OT_FNP_TAIL,      0,                                     NULL         }, /* bosspanthertail_2010 */
    { OT_FNP_HIT,       OD_HIDDEN,                             NULL         }, /* bosspantherhitbox_2043 (Q20, invisible) */
    { OT_FNP_LASER,     OD_HIDDEN,                             NULL         }, /* bosspantherlaser_2047 (drawn by fn_panther.c) */
    { OT_FNP_TARGET,    OD_HIDDEN,                             NULL         }, /* bosspanthertarget_2044 */
    { OT_FNP_GLOW,      OD_HIDDEN,                             NULL         }, /* bosschargeglow_2042 (drawn by fn_panther.c) */
    { OT_FNP_WHEEL,     OD_HIDDEN,                             NULL         }, /* bosspantherwheel_2052 (drawn by fn_panther.c) */
    { OT_FNP_FLOOR,     OD_SOLID|OD_BOXCOL,                    NULL         }, /* scrollingfloor_2037 */
    { OT_FNP_SPEEDBACK, OD_HIDDEN,                             NULL         }, /* effectspeedback_2049 (drawn by fn_panther.c) */
    { OT_FNP_PLANE,     OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL|OD_HIDDEN, NULL     }, /* planeplatform_2054 */
    { OT_FNP_ASSIST,    OD_HIDDEN,                             NULL         }, /* playerassist_2045 (the plane) */
    { OT_FNP_AOFF,      OD_HIDDEN,                             NULL         }, /* playerassistoffense_2048 */
    { OT_FNP_ADEF,      OD_HIDDEN,                             NULL         }, /* playerassistdefense_2046 (not ported: U-G3) */
    { OT_FNP_APROJ,     OD_HIDDEN,                             NULL         }, /* playerassistprojectile_2051 (not ported: U-G3) */
    { OT_FNP_SHIP1,     0,                                     NULL         }, /* ship1_2024 (FN5 decor) */
    { OT_FNP_SHIP2,     0,                                     NULL         }, /* ship2_2025 (FN5 decor, timeline B) */
    { OT_FNP_CABLE,     0,                                     NULL         }, /* shipcable_2026 */
    { OT_FNP_CABLE2,    0,                                     NULL         }, /* shipcable2_2027 */
    { OT_FNP_PLASMA,    0,                                     NULL         }, /* - (BossPlasmaShot_2040) */
    { OT_FNP_MISSILE,   0,                                     NULL         }, /* - (BossMissile_2041) */
    { OT_FNP_RBULLET,   0,                                     NULL         }, /* - (BossRapidBullet_1866) */
    { OT_FNP_WARN,      0,                                     NULL         }, /* - (EffectWarning_1528) */
    { OT_FNP_SLINE,     0,                                     NULL         }, /* - (EffectSpeedLine_2050) */
    { OT_FNP_SHIELD,    0,                                     NULL         }, /* - (ShieldSerpentine_1872, the rider's) */
    { OT_FNP_INSTA,     0,                                     NULL         }, /* - (the partner's ShieldInstaShield) */
    /* Sky Battalion stage objects (session 19, sb_flow.c; RE on_frame_36..39_init qualifiers: SwitchPlatform is in q7) */
    { OT_SB_SWPLAT,     OD_SOLID|OD_BOXCOL|OD_HIDDEN,          "sbswplat"   }, /* switchplatform_2100 (invisible box; its mask is drawn) */
    { OT_SB_SWMASK,     0,                                     "sbswmask"   }, /* switchplatformmask_2099 */
    { OT_SB_NAME1,      0,                                     "sbname1"    }, /* levelshipname1_2111 */
    { OT_SB_NAME2,      0,                                     "sbname2"    }, /* levelshipname2_2112 */
    { OT_SB_NAME3,      0,                                     "sbname3"    }, /* levelshipname3_2113 */
    { OT_SB_AIRSHIP,    OD_HIDDEN,                             "sbairship0" }, /* levelairship1_2105 (drawn on the 0.25 layer by sb_flow.c) */
    { OT_SB_ZAOFLAG,    0,                                     "sbflag"     }, /* levelzaoflag_2107 */
    { OT_SB_DARKSKY,    OD_HIDDEN,                             NULL         }, /* leveldarksky_2114 (a full-view rectangle, sb_flow.c) */
    { OT_SB_BGPLAT,     OD_HIDDEN,                             "sbbgplat"   }, /* levelsbbgplatform_2244 (the opening planks, layer 1 at 0.8) */
    /* SB_ENEMIES (sb_enemy.c, spec_sb_enemies 0; sprites set by sb_enemy_spawn; RE on_frame_37..39_init: only the crate is in q7) */
    { OT_SB_HOPPER,     0,                                     NULL         }, /* enemyhoppermkii_2189 (q20) */
    { OT_SB_MAGMEON,    0,                                     NULL         }, /* enemymagmeon_2203 (q20) */
    { OT_SB_ROTOR,      0,                                     NULL         }, /* enemyrotorblade_2184 (q20) */
    { OT_SB_ROTORSPK,   0,                                     NULL         }, /* enemyrotorbladespikes_2183 */
    { OT_SB_TUBE,       0,                                     NULL         }, /* enemyspawntube_2185 */
    { OT_SB_DRONE,      0,                                     NULL         }, /* enemydrone_2138 (q20) */
    { OT_SB_AXLE,       0,                                     NULL         }, /* enemyaxleturret_2139 (q20) */
    { OT_SB_AXLE2,      0,                                     NULL         }, /* enemyaxleturret2_2140 */
    { OT_SB_DNA,        0,                                     NULL         }, /* enemydnacannon_2260 (q20) */
    { OT_SB_DNABASE,    0,                                     NULL         }, /* enemydnacannonbase_2261 */
    { OT_SB_PELLETIZER, 0,                                     NULL         }, /* enemypelletizer_2303 (q20) */
    { OT_SB_MAGNET,     0,                                     NULL         }, /* enemymagnetizer_2294 (q20) */
    { OT_SB_MHAND,      0,                                     NULL         }, /* enemymagnetizerhand_2292 */
    { OT_SB_MLINE,      0,                                     NULL         }, /* enemymagnetizerline_2293 */
    { OT_SB_WARN,       OD_HIDDEN,                             NULL         }, /* effectwarning_1528 in SB1 / SB3 (ctor invisible; shown by the events) */
    { OT_SB_SHIPSHOT,   OD_HIDDEN,                             NULL         }, /* shipbullet_2106 (runtime; drawn on the 0.25 layer by sb_enemy_draw) */
    { OT_SB_HUBBALL,    0,                                     NULL         }, /* gimmickcannonbullet_2098 falling on SB1's deck (runtime) */
    { OT_SB_DLASER,     0,                                     NULL         }, /* enemydronelaser_2232 (runtime) */
    { OT_SB_DNALASER,   0,                                     NULL         }, /* enemydnalaser_2290 (runtime) */
    { OT_SB_CANNON,     0,                                     NULL         }, /* gimmickcannon_2142 */
    { OT_SB_CANBASE2,   0,                                     NULL         }, /* gimmickcannonbase2_2176 */
    { OT_SB_FLAMER,     0,                                     NULL         }, /* gimmickflamethrower_2182 */
    { OT_SB_CRUSHCHAIN, 0,                                     NULL         }, /* gimmickcrusherchain_2309 */
    { OT_SB_PLATFLAME,  0,                                     NULL         }, /* levelsbplatformflame1_2096 */
    { OT_SB_FANBLADE,   0,                                     NULL         }, /* levelfanblade_2193 / 2264 / 2297 */
    { OT_SB_FAN,        0,                                     NULL         }, /* gimmickfan_2223 */
    { OT_SB_FANSIDE,    0,                                     NULL         }, /* gimmickfanside_2245 */
    { OT_SB_SCHAIN,     0,                                     NULL         }, /* levelscrollingchainup / down_2302 / 2305 */
    { OT_SB_BACKEYE,    0,                                     NULL         }, /* sbbackeye / 2_2282 / 2283 */
    { OT_SB_CANBULLET,  0,                                     NULL         }, /* gimmickcannonbullet_2098 (runtime) */
    { OT_SB_FANDEBRIS,  0,                                     NULL         }, /* gimmickfandebris_2230 (runtime) */
    { OT_SB_FLOORPIECE, 0,                                     NULL         }, /* breakablefloorpiece2_2229 (runtime) */
    { OT_SB_MCRATE,     OD_SOLID|OD_BOXCOL,                    NULL         }, /* enemymagnetizercrate_2295 (q7 q9) */
    /* SB_GIM (sb_gim.c, spec_sb_gimmicks 0; sprites set by sb_gim_spawn; RE on_frame_37..39_init qualifiers, constructors) */
    { OT_SB_CANBASE,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickcannonbase_2141 (q7 q20) */
    { OT_SB_BFLOOR,     OD_SOLID,                              NULL         }, /* breakablefloor1 / 2_2196 / 2197 / 2298 / 2299 (q7) */
    { OT_SB_CHAINWALL,  OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickchainwallup / down_2307 / 2308 (q7) */
    { OT_SB_CHAINMASK,  OD_SOLID|OD_HIDDEN,                    NULL         }, /* gimmickchainwallmaskup / down_2304 / 2306 (q7, invisible mask) */
    { OT_SB_CRUSHER,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickcrusherplatform_2310 (q7) */
    { OT_SB_MBLOCK,     OD_SOLID|OD_BOXCOL,                    NULL         }, /* levelsbmovingblock1_2318 (q7) */
    { OT_SB_PLAT1,      OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelsbplatform1_2095 (q8 q9 q12 q13) */
    { OT_SB_PLAT3,      OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* levelsbplatform3_2188 (q8 q9 q13) */
    { OT_SB_BARMASK,    OD_BOXCOL|OD_HIDDEN|OD_HANG,           NULL         }, /* gimmicksbbarmask_2313 (q13 q23: the hang handle) */
    { OT_SB_BAR,        OD_BOXCOL,                             NULL         }, /* gimmicksbbar_2314 (q13) */
    { OT_SB_LIGHTNING,  OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* hazardlightningradial_2254 (invisible) */
    { OT_SB_LIGHTNING2, OD_BOXCOL,                             NULL         }, /* hazardlightningradial2_2256 */
    { OT_SB_FIRE,       OD_BOXCOL,                             NULL         }, /* hazardfire up / down / left / right */
    { OT_SB_CUPSTART,   OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickspincupstartpoint_2275 */
    { OT_SB_CUPLAND,    OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickspincuplandpoint_2270 */
    { OT_SB_SPINCUP,    OD_BOXCOL,                             NULL         }, /* gimmickspincup_2269 */
    { OT_SB_LOOP0,      OD_SOLID|OD_LAYER0,                    NULL         }, /* sbloop1 / 4 / 7, active3 / 4 (q7 q10) */
    { OT_SB_LOOP1,      OD_SOLID|OD_LAYER1,                    NULL         }, /* sbloop2 / 6, active / active2 (q7 q11) */
    { OT_SB_LOOP1B,     OD_SOLID|OD_BOXCOL|OD_LAYER1,          NULL         }, /* sbloop3 / 5 (q7 q11, box collision) */
    { OT_SB_PBLOCK4,    OD_SOLID|OD_BOXCOL|OD_NOEDGE,          NULL         }, /* pushableblock4_2259 (q7 q9 q14) */
    { OT_SB_BTUNNEL,    OD_SOLID|OD_HIDDEN,                    NULL         }, /* levelballtunnel2 / 3 (q7 q27, invisible: the roll tube mask) */
    { OT_SB_DOOR,       OD_SOLID,                              NULL         }, /* switchdoor_2102 (q7; rises with its pad, sb_gim.c) */
    /* ==== Jade Creek (session 21): one block per module */
    /* ---- JC_FLOW (jc_flow.c) */
    { OT_JCF_LOOP0,     OD_SOLID|OD_LAYER0,                    NULL         }, /* jcloop3 / 4_2463 / 2464 (q7 q10) */
    { OT_JCF_LOOP1,     OD_SOLID|OD_LAYER1,                    NULL         }, /* jcloop2_2462 (q7 q11) */
    { OT_JCF_LOOPX,     OD_SOLID,                              NULL         }, /* jcloop1_2461 (q7; flag 0 by jc_flow.c) */
    /* ---- JC_ENEMIES (jc_enemy.c, spec_jc_enemies; sprites set by jc_enemy_spawn; RE on_frame_40..44_init q20) */
    { OT_JC_METEOR,     OD_HIDDEN,                             NULL         }, /* enemymeteorroller_2416 (q20; ctor invisible) */
    { OT_JC_MFLAME,     OD_HIDDEN,                             NULL         }, /* enemymeteorrollerflame_2417 (shown while its roller rolls) */
    { OT_JC_BUOY,       0,                                     NULL         }, /* enemybuoybuoy_2407 (q20) */
    { OT_JC_PLANT,      0,                                     NULL         }, /* enemyplantturretus_2393 (q20) */
    { OT_JC_PHAT,       0,                                     NULL         }, /* enemyplantturretushat_2394 */
    { OT_JC_PSTALK,     0,                                     NULL         }, /* enemyplantturretusstalk_2395 */
    { OT_JC_FROG,       0,                                     NULL         }, /* enemybullfrag_2389 (q20) */
    { OT_JC_FROGMASK,   OD_HIDDEN,                             NULL         }, /* enemybullfragmask_2410 (ctor invisible) */
    { OT_JC_SHARK,      0,                                     NULL         }, /* enemysharktorpedo_2421 (q20) */
    { OT_JC_ORBITER,    0,                                     NULL         }, /* enemybubblorbiter_2455 (q20) */
    { OT_JC_LAUNCHER,   0,                                     NULL         }, /* hazardtorpedolauncher_2515 */
    { OT_JC_ELITE,      0,                                     NULL         }, /* enemyelite_2529 */
    { OT_JC_ELITEMASK,  OD_HIDDEN,                             NULL         }, /* enemyelitemask_2528 (q20; ctor invisible) */
    { OT_JC_MEXPL,      0,                                     NULL         }, /* effectsexplosion06_435 thrown by a meteor (runtime) */
    { OT_JC_BJET,       0,                                     NULL         }, /* enemybuoybullet_2409 / 2_2415 (runtime) */
    { OT_JC_LEAF,       0,                                     NULL         }, /* effectfallingleaf_2396 (runtime) */
    { OT_JC_BUBBLE,     0,                                     NULL         }, /* enemybullfragbubble_2408 (runtime) */
    { OT_JC_TORP,       0,                                     NULL         }, /* enemysharktorpedo2_2422 (runtime) */
    { OT_JC_OSHOT,      0,                                     NULL         }, /* badnikgolemshot_1260 (runtime) */
    { OT_JC_WARN,       0,                                     NULL         }, /* effectwarning_1528 (runtime) */
    { OT_JC_LASER,      0,                                     NULL         }, /* enemytrooperlaser_2530 (runtime) */
    { OT_JC_GLOW,       0,                                     NULL         }, /* enemyelitechargeglow_2527 (runtime) */
    { OT_JC_TBOMB,      0,                                     NULL         }, /* minibosstreddbomb_1060 from a launcher (runtime) */
    /* ---- JC_GIM (jc_gim.c; RE on_frame_40..44_init qualifiers, create_* initialize_active / set_visible). The sprites are set
     * by jc_gim_spawn. LevelCreekPlatform_2425 is OT_PLAT_VALLEY (importer JCG_TYPES_ID). */
    { OT_JCG_STREAM,    OD_BOXCOL,                             NULL         }, /* jcwaterstream1-8 (1-6 q26) */
    { OT_JCG_LEAFBACK,  OD_BOXCOL,                             NULL         }, /* gimmickleafplatformback_2357 */
    { OT_JCG_LEAF,      OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* gimmickleafplatform1 / 2 (q8 q9; ignored off the player's layer) */
    { OT_JCG_FLEAF,     0,                                     NULL         }, /* gimmickflyingleafplatform_2476 */
    { OT_JCG_FLEAFM,    OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* gimmickflyingleafplatformmask_2477 (q8 q9 q13, invisible) */
    { OT_JCG_SLIDE,     OD_SOLID|OD_HIDDEN,                    NULL         }, /* jcslidingplatform_2377 (q7 q13, invisible) */
    { OT_JCG_SLIDEM,    OD_BOXCOL,                             NULL         }, /* jcslidingplatformmask_2381 (the drawn slab) */
    { OT_JCG_SLIDEL,    OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* jcslidingplatformlandpoint_2382 (invisible) */
    { OT_JCG_COLL,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* leveljccollapsingplatform_2456 / 2475 (q7) */
    { OT_JCG_CPIECE,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* leveljccollapsingplatformpiece_2457 (q7; runtime) */
    { OT_JCG_SEEDSP,    0,                                     NULL         }, /* gimmickseedspawner_2418 */
    { OT_JCG_SEED,      0,                                     NULL         }, /* gimmickflyingseed_2419 (runtime) */
    { OT_JCG_BUBSP,     OD_BOXCOL,                             NULL         }, /* gimmickbubblespringspawner_2449 */
    { OT_JCG_BUBBACK,   0,                                     NULL         }, /* gimmickbubblespringback2_2459 (runtime) */
    { OT_JCG_BUBFRONT,  0,                                     NULL         }, /* gimmickbubblespringfront2_2460 (runtime) */
    { OT_JCG_SLIP,      OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* levelslipperysurface_2473 (invisible) */
    { OT_JCG_FALLLEAF,  OD_BOXCOL,                             NULL         }, /* effectfallingleaf_2396 */
    /* ---- JC_BOSS (jc_boss.c; spec_jc_bosses; sprites set by jc_boss_spawn). OD_HIDDEN: the constructor's set_visible(0) */
    { OT_JCB_HC,        0,                                     NULL         }, /* bosshovercop_2429 */
    { OT_JCB_TB1,       0,                                     NULL         }, /* bosshovercopturbine1_2430 */
    { OT_JCB_TB2,       0,                                     NULL         }, /* bosshovercopturbine2_2428 */
    { OT_JCB_PROP1,     0,                                     NULL         }, /* bosshovercoppropellor_2431 */
    { OT_JCB_PROP2,     0,                                     NULL         }, /* bosshovercoppropellor2_2432 */
    { OT_JCB_SIR1,      0,                                     NULL         }, /* bosshovercopsiren1_2433 */
    { OT_JCB_SIR2,      0,                                     NULL         }, /* bosshovercopsiren2_2434 */
    { OT_JCB_SIR3,      0,                                     NULL         }, /* bosshovercopsiren3_2427 */
    { OT_JCB_SIR4,      0,                                     NULL         }, /* bosshovercopsiren4_2426 */
    { OT_JCB_HOOK,      OD_HIDDEN,                             NULL         }, /* bosshovercophook_2468 (shown by the ice death) */
    { OT_JCB_HOOK2,     0,                                     NULL         }, /* bosshovercophook2_2472 (the missile chain) */
    { OT_JCB_LIGHT,     0,                                     NULL         }, /* levelhovercraftlight_2465 (additive, x scale 8) */
    { OT_JCB_ICED,      OD_HIDDEN,                             NULL         }, /* playericedeath_2467 */
    { OT_JCB_NEERA,     OD_HIDDEN,                             NULL         }, /* npcneera_2483 (shown by 10666) */
    { OT_JCB_NHIT,      OD_HIDDEN,                             NULL         }, /* bossneerahitbox_2482 (Q20, A 120) */
    { OT_JCB_ICEBAR,    OD_SOLID,                              NULL         }, /* levelicebarrier_2480 (q 0x218c0: solid) */
    { OT_JCB_ICEPIECE,  0,                                     NULL         }, /* levelicebarrierpiece_2495 (template) */
    { OT_JCB_GLOW,      0,                                     NULL         }, /* - (BossHoverCopChargeglow_2471) */
    { OT_JCB_CRYSTAL,   0,                                     NULL         }, /* - (BossIceCrystal_2466) */
    { OT_JCB_MISSILE,   0,                                     NULL         }, /* - (BossIceMissile_2470) */
    { OT_JCB_STICKY,    0,                                     NULL         }, /* - (EffectStickyIce_2469) */
    { OT_JCB_METEOR,    0,                                     NULL         }, /* - (BossMeteor_2435) */
    { OT_JCB_RING,      0,                                     NULL         }, /* - (BossIceRing_2481) */
    { OT_JCB_NSHIELD,   0,                                     NULL         }, /* - (Neera's elemental shield) */
    { OT_JCB_BUBBLE,    0,                                     NULL         }, /* - (MediumBubble_805 behind a missile) */
    /* ---- JC_BOSS5 (jc_boss5.c; numbers = src/game/jc.h OT_J5_*; sprites set by jc_boss5_spawn; OD_HIDDEN: the constructor's set_visible(0), RE create_*) */
    { 1480,               0,                                   NULL         }, /* J5_GUN: bossgunship_2559 */
    { 1481,               0,                                   NULL         }, /* J5_GUN_BLADEL: bossgunshipbladeleft_2557 */
    { 1482,               0,                                   NULL         }, /* J5_GUN_BLADER: bossgunshipbladeright_2558 */
    { 1483,               OD_HIDDEN,                           NULL         }, /* J5_GUN_CANL: bossgunshipcannonleft_2566 */
    { 1484,               OD_HIDDEN,                           NULL         }, /* J5_GUN_CANR: bossgunshipcannonright_2567 */
    { 1485,               0,                                   NULL         }, /* J5_GUN_HIT: bossgunshiphitbox_2577 (Q20) */
    { 1486,               0,                                   NULL         }, /* J5_GUN_LIGHT: bossgunshiplight_2562 */
    { 1487,               0,                                   NULL         }, /* J5_GUN_LIGHTL: bossgunshiplightleft_2563 */
    { 1488,               0,                                   NULL         }, /* J5_GUN_LIGHTR: bossgunshiplightright_2564 */
    { 1489,               0,                                   NULL         }, /* J5_GUN_RACKL: bossgunshipmissilerackleft_2555 */
    { 1490,               0,                                   NULL         }, /* J5_GUN_RACKR: bossgunshipmissilerackright_2556 */
    { 1491,               0,                                   NULL         }, /* J5_GUN_PILOT: bossgunshippilot_2565 */
    { 1492,               0,                                   NULL         }, /* J5_GUN_PLAT: bossgunshipplatform_2553 */
    { 1493,               OD_HIDDEN,                           NULL         }, /* J5_GUN_TROOPER: bossgunshiptrooper_2554 */
    { 1494,               0,                                   NULL         }, /* J5_GUN_TURBL: bossgunshipturbineleft_2560 */
    { 1495,               0,                                   NULL         }, /* J5_GUN_TURBR: bossgunshipturbineright_2561 */
    { 1496,               0,                                   NULL         }, /* J5_GUN_FLARE: - (BossGunshipFlare_2568) */
    { 1497,               0,                                   NULL         }, /* J5_GUN_MISSILE: - (BossMissile_1867, the racks') */
    { 1498,               0,                                   NULL         }, /* J5_ELITE: enemyelite_2529 (the placed one is a template: destroyed, 11651) */
    { 1499,               OD_HIDDEN,                           NULL         }, /* J5_ELITEMASK: enemyelitemask_2528 (Q20) */
    { 1500,               0,                                   NULL         }, /* J5_WARN2: - (EffectWarning2_1895) */
    { 1501,               0,                                   NULL         }, /* J5_ELITELASER: - (EnemyTrooperLaser) */
    { 1502,               0,                                   NULL         }, /* J5_BOLT: - (LevelLightningBolt, runtime) */
    { 1503,               0,                                   NULL         }, /* J5_GLOW: - (EnemyEliteChargeGlow) */
    { 1504,               0,                                   NULL         }, /* J5_HERO: - (the closing's heroes) */
    { 1505,               0,                                   NULL         }, /* J5_FADE: - (LevelCardFade, the white card) */
    /* ==== end Jade Creek */
    /* ==== Trap Hideout (session 21b): one block per module */
    /* ---- TH_FLOW (th_flow.c) */
    { OT_THF_BACKLIGHT, 0,                                     NULL         }, /* thbacklight1_3113 (frame by th_flow.c, 13532) */
    { OT_THF_FLAME,     0,                                     NULL         }, /* thflame_3176 (torch flame, looping) */
    { OT_THF_SPOT,      OD_HIDDEN,                             NULL         }, /* thspotlight_3209 (ctor invisible; TH3 14083 shows it) */
    /* ---- TH_ENEMIES (th_enemy.c, spec_th_enemies; sprites set by th_enemy_spawn; RE on_frame_57..59_init q7 / q20, create_* set_visible) */
    { OT_TH_TOTEM,      OD_SOLID,                              NULL         }, /* enemytotematic_3144 (q7 q20; flag 0 while the player is a ball) */
    { OT_TH_PUP,        OD_LANDS,                              NULL         }, /* enemymetalpup_3148 (q20; 13616) */
    { OT_TH_PUPPET,     OD_LANDS,                              NULL         }, /* enemyshockpuppet_3149 (q20; 13627) */
    { OT_TH_ORB,        OD_LANDS,                              NULL         }, /* enemybladeorbiter_3165 (q20; 13645) */
    { OT_TH_BLADEL,     0,                                     NULL         }, /* enemybladeorbiterleft_3163 */
    { OT_TH_BLADER,     0,                                     NULL         }, /* enemybladeorbiterright_3164 */
    { OT_TH_PUNCH,      0,                                     NULL         }, /* enemyrocketpuncher_3182 (q20; drawn with its rig) */
    { OT_TH_PHAND,      0,                                     NULL         }, /* enemyrocketpuncherhand_3183 */
    { OT_TH_PSHIELD,    OD_SOLID,                              NULL         }, /* enemyrocketpunchershield_3184 (q7 q20) */
    { OT_TH_PWARN,      OD_HIDDEN,                             NULL         }, /* effectrocketpuncherwarning_3185 (ctor invisible) */
    { OT_TH_BIKE,       0,                                     NULL         }, /* enemybikeninja_3203 */
    { OT_TH_BMASK,      OD_HIDDEN,                             NULL         }, /* enemybikeninjamask_3204 (q20; ctor invisible) */
    { OT_TH_BWARN,      OD_HIDDEN,                             NULL         }, /* effectbikeninjawarning_3206 (ctor invisible) */
    { OT_TH_STAR,       0,                                     NULL         }, /* enemyninjastar_3221 (runtime) */
    { OT_TH_BDEAD,      0,                                     NULL         }, /* enemybikeninjadefeated_3205 (runtime) */
    /* ---- TH_GIM (th_gim.c; qualifiers RE on_frame_57..59_init) */
    { OT_THG_BEAM,        0,                                   NULL         }, /* hazardscanlasersweep / right / left / down / up (Q33: th_gim.c) */
    { OT_THG_HEAD,        0,                                   NULL         }, /* hazardscanlaserhead / head2-5 */
    { OT_THG_PILLAR,      OD_SOLID,                            NULL         }, /* levelhideoutplatform1 (Q7 Q12 Q13: a path platform, objects.c) */
    { OT_THG_PFLAME,      0,                                   NULL         }, /* levelhideoutplatformflame */
    { OT_THG_FLAME,       0,                                   NULL         }, /* thflame (scenery) */
    { OT_THG_SHUR,        0,                                   NULL         }, /* hazardshuriken (Q20) */
    { OT_THG_SHBASE,      0,                                   NULL         }, /* hazardshurikenbase */
    { OT_THG_SHTURN,      OD_HIDDEN,                           NULL         }, /* hazardshurikenturn (ctor set_visible(0)) */
    { OT_THG_BARREL,      0,                                   NULL         }, /* gimmickbarrel */
    { OT_THG_BDEBRIS,     0,                                   NULL         }, /* - (GimmickBarrelDebris_3127, runtime) */
    { OT_THG_BLIGHT,      0,                                   NULL         }, /* thbacklight1 */
    /* ---- TH_BOSS (th_boss.c) */
    { OT_THB_IRIS,      0,                                     NULL         }, /* bosslaseriris_3141 (Q20, A 60) */
    { OT_THB_CANNON,    0,                                     NULL         }, /* bosslaseririscannon_3140 */
    { OT_THB_WINGL,     0,                                     NULL         }, /* bosslaseririswingleft_3138 */
    { OT_THB_WINGR,     0,                                     NULL         }, /* bosslaseririswingright_3139 */
    { OT_THB_LASER,     0,                                     NULL         }, /* hazardscanlaserboss_3142 */
    { OT_THB_CB_BASE,   0,                                     NULL         }, /* bosscrystalbasherbase_3199 */
    { OT_THB_CB_FACE,   OD_HIDDEN,                             NULL         }, /* bosscrystalbasherface_3198 (Q20, A 120; shown by 13916) */
    { OT_THB_CB_HEAD,   0,                                     NULL         }, /* bosscrystalbasherhead_3197 */
    { OT_THB_CB_SEG,    0,                                     NULL         }, /* bosscrystalbashersegment_3194 */
    { OT_THB_CB_SPIKE,  0,                                     NULL         }, /* bosscrystalspike_3195 */
    { OT_THB_CB_SPIKE2, 0,                                     NULL         }, /* bosscrystalspike2_3196 */
    { OT_THB_DOOR,      OD_SOLID,                              NULL         }, /* bossdoor_3200 (the player stands on it: RV c1_c) */
    { OT_THB_CROWD,     0,                                     NULL         }, /* thcrowd_3213 (scenery; speed 8 + R(30): 14047) */
    { OT_THB_GOAL,      OD_HIDDEN,                             NULL         }, /* guigoalmessage_3217 (GUI: th_boss_arena.c) */
    { OT_THB_ENLEFT,    OD_HIDDEN,                             NULL         }, /* guienemiesleft_3218 (GUI counter: th_boss_arena.c) */
    /* ==== end Trap Hideout */
    /* ==== Thermal Base (session 21c): one block per module */
    /* ---- TB_FLOW (tb_flow.c) */
    { OT_TBF_LIFT,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* levellift_2649 (Q7 Q13; box: INFERRED, the mask's top edge is the box's) */
    { OT_TBF_LIFTTOP,   OD_HIDDEN,                             NULL         }, /* levellifttop_2650 (ctor set_visible(0)) */
    { OT_TBF_LIFTBOT,   OD_HIDDEN,                             NULL         }, /* levelliftbottom_2651 (ctor set_visible(0)) */
    { OT_TBF_WALLLIGHT, 0,                                     NULL         }, /* tbwalllightanim_2644 */
    /* ---- TB_ENEMIES (tb_enemy.c) */
    { OT_TBE_SEEKER,    OD_LANDS_KS,                           NULL         }, /* enemyseeker_2603 (q20; crash 11779: keystone, sound 29) */
    { OT_TBE_PUSHUP,    0,                                     NULL         }, /* enemypushup_2669 */
    { OT_TBE_PUHIT,     OD_HIDDEN | OD_LANDS,                  NULL         }, /* enemypushuphitbox_2663 (q20, ctor invisible; lands 11798: sound 50) */
    { OT_TBE_SQUID,     0,                                     NULL         }, /* enemylavasquid_2740 (q20; beaten: 2 debris) */
    { OT_TBE_TANK,      OD_LANDS_KS,                           NULL         }, /* enemytankbot_2838 (q20; crash 12443) */
    { OT_TBE_SAUCER,    0,                                     NULL         }, /* enemysaucer_2828 (q20; beaten: debris) */
    { OT_TBE_LEG1,      0,                                     NULL         }, /* enemysaucerleg1_2827 */
    { OT_TBE_LEG1 + 1,  0,                                     NULL         }, /* enemysaucerleg2_2829 */
    { OT_TBE_LEG1 + 2,  0,                                     NULL         }, /* enemysaucerleg3_2830 */
    { OT_TBE_LEG4,      0,                                     NULL         }, /* enemysaucerleg4_2826 */
    { OT_TBE_SLIM,      OD_HIDDEN,                             NULL         }, /* enemysaucerlimiter_2832 (ctor invisible) */
    { OT_TBE_BULLET,    0,                                     NULL         }, /* enemyhomingbullet_2648 (runtime) */
    { OT_TBE_PULAS,     0,                                     NULL         }, /* enemypushuplaser_2670 (runtime) */
    { OT_TBE_PULAS2,    OD_HIDDEN,                             NULL         }, /* enemypushuplaser2_2695 (runtime, ctor invisible) */
    { OT_TBE_PULAS3,    0,                                     NULL         }, /* enemypushuplaser3_2696 (runtime) */
    { OT_TBE_PUDEB,     0,                                     NULL         }, /* enemypushupdebris_2664 (runtime) */
    { OT_TBE_SQLAS,     0,                                     NULL         }, /* enemylavasquidlaser_2741 (runtime) */
    { OT_TBE_DEBRIS,    OD_LANDS_KS,                           NULL         }, /* enemylavasquiddebris_2742 / enemysaucerdebris_2831 (runtime, q20; 11987 / 12537) */
    { OT_TBE_TDEB,      0,                                     NULL         }, /* enemytankbotdebris_2839 (runtime) */
    /* ---- TB_GIM (tb_gim.c; qualifiers RE on_frame_45..50_init: Q7 solid, Q8 obstacle, Q9 no-edge, Q10 / Q11 loop layers, Q23 hang;
     * box collision = constructor initialize_active(1)) */
    { OT_TBG_KEY,       0,                                     NULL         }, /* objectkeycard */
    { OT_TBG_TERM,      0,                                     NULL         }, /* lockeddoorterminal */
    { OT_TBG_LDOOR,     0,                                     NULL         }, /* lockeddoorback / front / seal */
    { OT_TBG_BARRIER,   OD_SOLID|OD_BOXCOL|OD_HIDDEN,          NULL         }, /* lockeddoorbarrier (q7; flag 0 once open: 11812) */
    { OT_TBG_MDOOR,     0,                                     NULL         }, /* metaldoorback / front (scenery) */
    { OT_TBG_FPLAT,     OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* levelfireplatform1 (q8 q9 q12 q13) */
    { OT_TBG_FPLAT80,   OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* levelfireplatform1_2665 (TB1, G 80) */
    { OT_TBG_FFLAME,    0,                                     NULL         }, /* levelfireplatformflame1 / 2 */
    { OT_TBG_SPLAT,     OD_SOLID|OD_HIDDEN,                    NULL         }, /* levelscrollingplatform1 / 2 (q7 q13; ctor invisible) */
    { OT_TBG_SMASK,     0,                                     NULL         }, /* levelscrollingplatformmask1 / 2 */
    { OT_TBG_SFLAME,    0,                                     NULL         }, /* levelscrollingplatformflame1 / 2 */
    { OT_TBG_SMARK,     OD_HIDDEN,                             NULL         }, /* levelscrollingplatformstart / end */
    { OT_TBG_PUSH,      OD_HIDDEN,                             NULL         }, /* levelscrollingplatformplayerpush */
    { OT_TBG_PISTON,    OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* levelpistonplatform1 (q8 q9 q13; ctor invisible) */
    { OT_TBG_PMASK,     0,                                     NULL         }, /* levelpistonplatformmask */
    { OT_TBG_SPIN,      OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL|OD_HIDDEN, NULL     }, /* levelspinplatform (q8 q9 q13; ctor invisible) */
    { OT_TBG_SPINMASK,  0,                                     NULL         }, /* levelspinplatformmask */
    { OT_TBG_SPINSUP,   0,                                     NULL         }, /* levelspinplaformsupport */
    { OT_TBG_VENT,      OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* airventplatform (q8 q9) */
    { OT_TBG_LAVA,      0,                                     NULL         }, /* hazardlavapit (no event in tb2 / tb3: scenery) */
    { OT_TBG_LFALL,     0,                                     NULL         }, /* hazardlavafall / hazardlavafall3 */
    { OT_TBG_LWARN,     0,                                     NULL         }, /* hazardlavafallwarning */
    { OT_TBG_MAGMA,     0,                                     NULL         }, /* magmaflow (no event: scenery) */
    { OT_TBG_XBLOCK,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* objectexplosiveblock (q7) */
    { OT_TBG_BUTTON,    0,                                     NULL         }, /* switchbuttonleft / right */
    { OT_TBG_FTRAP,     OD_SOLID|OD_BOXCOL,                    NULL         }, /* hazardfloortrap (q7) */
    { OT_TBG_BLOCKER,   OD_SOLID|OD_BOXCOL|OD_HIDDEN,          NULL         }, /* playerblocker (q7; ctor invisible) */
    { OT_TBG_CONV,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickconveyorbelt* (q7 q31) */
    { OT_TBG_LOOP0,     OD_SOLID|OD_LAYER0,                    NULL         }, /* tbpipeloop2 / 3 / 6 / 7 (q7 q10) */
    { OT_TBG_LOOP0B,    OD_SOLID|OD_BOXCOL|OD_LAYER0,          NULL         }, /* tbpipeloop5 / 9 / 11 (q7 q10, box) */
    { OT_TBG_LOOP1,     OD_SOLID|OD_LAYER1,                    NULL         }, /* tbpipeloop1 / 4 / 8 (q7 q11) */
    { OT_TBG_LOOP1B,    OD_SOLID|OD_BOXCOL|OD_LAYER1,          NULL         }, /* tbpipeloop10 (q7 q11, box) */
    { OT_TBG_CTOP,      OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* gimmickcrankplatformtop1 / 2 (q8 q9) */
    { OT_TBG_CPART,     0,                                     NULL         }, /* gimmickcrankplatform mid / base / arm */
    { OT_TBG_PRESS,     0,                                     NULL         }, /* gimmickgiantpressertube / gimmckgiantpresser */
    { OT_TBG_PHIT,      OD_HIDDEN,                             NULL         }, /* gimmckgiantpresserhitbox (ctor invisible) */
    { OT_TBG_TURB,      OD_SOLID|OD_BOXCOL,                    NULL         }, /* gimmickwaterturbine (q7) */
    { OT_TBG_TBUBSP,    OD_HIDDEN,                             NULL         }, /* tubebubblespawner (ctor invisible) */
    { OT_TBG_BGTURB,    0,                                     NULL         }, /* underwaterbgturbine */
    { OT_TBG_WTLIGHT,   0,                                     NULL         }, /* tbwaterlight */
    { OT_TBG_LEVER,     0,                                     NULL         }, /* gimmickwaterlever1 / 2 */
    { OT_TBG_WMASK,     OD_HANG|OD_HIDDEN|OD_BOXCOL,           NULL         }, /* gimmickwaterlevelmask1 / 2 (q23; ctor invisible) */
    { OT_TBG_BWAVE,     0,                                     NULL         }, /* gimmickbubblewavespawner / 2 */
    { OT_TBG_TBUB,      0,                                     NULL         }, /* - (TubeBubble_2849 / GimmickWaterTurbineBubble_2865) */
    { OT_TBG_DESTROYER, OD_HIDDEN,                             NULL         }, /* - (ObjectExplosiveBlockDestroyer_2744, ctor invisible) */
    { OT_TBG_FX,        0,                                     NULL         }, /* - (effects) */
    /* ---- TB_BOSS (tb_boss.c) */
    { OT_TBB_SHRIMP,    0,                                     NULL         }, /* bossroboshrimp_2673 (Q20, A 1) */
    { OT_TBB_SH_SEG,    0,                                     NULL         }, /* bossroboshrimpsegment_2672 (Q20, A 10) */
    { OT_TBB_SH_TAIL,   0,                                     NULL         }, /* bossroboshrimptail_2675 */
    { OT_TBB_SH_LIGHT,  0,                                     NULL         }, /* bossroboshrimplightning_2674 */
    { OT_TBB_SH_BLOCK,  OD_HIDDEN,                             NULL         }, /* bossroboshrimpblocker_2691 (ctor set_visible(0)) */
    { OT_TBB_TRAPDOOR,  OD_SOLID,                              NULL         }, /* minibosstrapdoor_2747 (the room's ceiling door; INFERRED solid) */
    { OT_TBB_PUSHER,    0,                                     NULL         }, /* enemywallpusher_2748 (Q20, armoured) */
    { OT_TBB_SQUID,     0,                                     NULL         }, /* bossrobosquid_2765 (Q20, A 50) */
    { OT_TBB_SQ_GLASS,  0,                                     NULL         }, /* bossrobosquidglass_2764 */
    { OT_TBB_SQ_JET,    0,                                     NULL         }, /* bossrobosquidjet_2766 */
    { OT_TBB_SQ_ARML,   0,                                     NULL         }, /* bossrobosquidleftarm_2762 */
    { OT_TBB_SQ_ARMR,   0,                                     NULL         }, /* bossrobosquidrightarm_2763 */
    { OT_TBB_SPIDER,    0,                                     NULL         }, /* bosssyntaxspider_2755 (the mask: Q20, A 200 / 30) */
    { OT_TBB_SP_BODY,   0,                                     NULL         }, /* bosssyntaxspiderbody_2753 */
    { OT_TBB_SP_CANNON, 0,                                     NULL         }, /* bosssyntaxspiderlavacannon_2752 */
    { OT_TBB_SP_ARM,    0,                                     NULL         }, /* bosssyntaxspiderarm_2754 */
    { OT_TBB_SP_LEGF,   0,                                     NULL         }, /* bosssyntaxspiderlegfront_2756 */
    { OT_TBB_SP_LEGB,   0,                                     NULL         }, /* bosssyntaxspiderlegback_2751 */
    { OT_TBB_SP_GRAPPLE, 0,                                    NULL         }, /* bosssyntaxgrappleline_2770 */
    { OT_TBB_SPLASH,    0,                                     NULL         }, /* bosslavasplash1_2758 / 2_2759 */
    { OT_TBB_SX_PART,   0,                                     NULL         }, /* bosssyntaxsquid back / shoulders / head / body / claws (TB6 rig) */
    { OT_TBB_SX_ARML,   0,                                     NULL         }, /* bosssyntaxsquidarmleft_2891 */
    { OT_TBB_SX_ARMR,   0,                                     NULL         }, /* bosssyntaxsquidarmright_2892 */
    { OT_TBB_SX_HIT,    OD_HIDDEN,                             NULL         }, /* bosssyntaxsquidhitbox_2902 (Q20, A 50; ctor set_visible(0)) */
    { OT_TBB_SX_LASER,  OD_HIDDEN,                             NULL         }, /* bosslavalaser_2896 (the off-map template; blobs are OT_TBB_SHOT) */
    { OT_TBB_SX_SPHERE, 0,                                     NULL         }, /* bossspikesphere_2897 / 2_2898 */
    { OT_TBB_SX_SPLASH, OD_HIDDEN,                             NULL         }, /* bosslavasplash left / right 1 / 2 (ctor set_visible(0)) */
    { OT_TBB_PLAT,      OD_SOLID,                              NULL         }, /* bosstbplatform_2886 (the arena floor / ledges; INFERRED solid) */
    { OT_TBB_PLAT2,     OD_SOLID,                              NULL         }, /* bosstbplatform2_2908 */
    { OT_TBB_WAYPOINT,  OD_HIDDEN,                             NULL         }, /* bosswaypoint_2905 (ctor set_visible(0)) */
    { OT_TBB_FX,        0,                                     NULL         }, /* - (runtime: a one-shot effect) */
    { OT_TBB_SHOT,      0,                                     NULL         }, /* - (runtime: a lava blob BossLavaLaser_2896) */
    /* ==== end Thermal Base */
    /* ==== Pangu Lagoon (session 23): one block per module */
    /* ---- PL_FLOW (pl_flow.c) */
    { OT_PLF_LOOP01,    OD_SOLID|OD_LAYER0,                    "pll01"      }, /* pllayer01_2975 (Q10: solid on loop layer 0; pixel mask) */
    { OT_PLF_LOOP02,    OD_SOLID|OD_LAYER0,                    "pll02"      }, /* pllayer02_2976 (Q10) */
    { OT_PLF_LOOP11,    OD_SOLID|OD_LAYER1,                    "pll11"      }, /* pllayer11_2973 (Q11: solid on loop layer 1) */
    { OT_PLF_LOOP12,    OD_SOLID|OD_LAYER1,                    "pll12"      }, /* pllayer12_2974 (Q11) */
    /* ---- PL_ENEMIES (pl_enemy.c, spec_pl_enemies; sprites set by pl_enemy_spawn; RE on_frame_51..56_init q20) */
    { OT_PL_STINGER,    OD_LANDS,                              NULL         }, /* enemystinger_2933 (q20; 12810: the Ganseki landing) */
    { OT_PL_NEST,       0,                                     NULL         }, /* enemystingernest_2937 (q20; flag 16, burst death) */
    { OT_PL_SUNFLORA,   0,                                     NULL         }, /* enemysunflora_2940 (q20; flag 16, regrows) */
    { OT_PL_GAS,        OD_HIDDEN,                             NULL         }, /* enemysleepgas_2941 (drawn at its blend alpha by pl_enemy_draw) */
    { OT_PL_TERREON,    OD_HIDDEN,                             NULL         }, /* enemyterreon_3006 (q20; drawn with its rig) */
    { OT_PL_TLEG,       OD_HIDDEN,                             NULL         }, /* enemyterreonleftleg_3004 (q20, armoured) */
    { OT_PL_TRLEG,      OD_HIDDEN,                             NULL         }, /* enemyterreonrightleg_3005 (q20, armoured) */
    { OT_PL_TLSH,       OD_HIDDEN,                             NULL         }, /* enemyterreonleftshoulder_3002 */
    { OT_PL_TRSH,       OD_HIDDEN,                             NULL         }, /* enemyterreonrightshoulder_3003 */
    { OT_PL_TWARN,      OD_HIDDEN,                             NULL         }, /* effectterreonwarning_3008 (shown by 12912) */
    { OT_PL_OBELISK,    0,                                     NULL         }, /* enemyobelisk_3047 (q20; flag 16, the wreck stays) */
    { OT_PL_WSTINGER,   0,                                     NULL         }, /* enemywallstinger_3056 (q20) */
    { OT_PL_WBACK,      0,                                     NULL         }, /* enemywallstingerback_3058 (json layer 1 scenery) */
    { OT_PL_WLTOP,      OD_HIDDEN,                             NULL         }, /* wallstingerlimittop_3060 */
    { OT_PL_WLBOT,      OD_HIDDEN,                             NULL         }, /* wallstingerlimitbottom_3061 */
    { OT_PL_TDEBRIS,    0,                                     NULL         }, /* enemyterreondebris_3007 (runtime) */
    { OT_PL_OLASER,     0,                                     NULL         }, /* enemyobelisklaser_3048 (runtime) */
    { OT_PL_WSPAWN,     OD_HIDDEN,                             NULL         }, /* enemywallstingerspawn_3057 (runtime marker) */
    { OT_PL_CONFETTI,   0,                                     NULL         }, /* effectsconfetti_2943 (runtime) */
    /* ---- PL_GIM (pl_gim.c; qualifiers RE on_frame_51..56_init; sprites set by pl_gim_spawn) */
    { OT_PLG_VBLOCK,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* objectvanishingblock_3035 (Q7, ctor initialize_active(1); flag 0 = vanished) */
    { OT_PLG_TGATE,     OD_HIDDEN,                             NULL         }, /* objecttimegate_2915 (ctor set_visible(0); col_9_50 box) */
    { OT_PLG_TPIECE,    0,                                     NULL         }, /* objecttimegatepiece_2916 (Q4) */
    { OT_PLG_STRING,    0,                                     NULL         }, /* gimmickbowstring_2997 */
    { OT_PLG_ARROW,     0,                                     NULL         }, /* gimmickbowarrow_2998 */
    { OT_PLG_ARROWPLAT, OD_OBSTACLE|OD_NOEDGE|OD_HIDDEN,       NULL         }, /* gimmickbowarrowplatform_2999 (Q8 Q9, ctor set_visible(0)) */
    { OT_PLG_PINWHEEL,  0,                                     NULL         }, /* objectpinwheel_2992 */
    { OT_PLG_CTOP,      0,                                     NULL         }, /* objectcyclonetop_2969 */
    { OT_PLG_BWALL,     OD_HIDDEN,                             NULL         }, /* gimmickbreakoutwallleft_3053 / right_3049 (ctor set_visible(0)) */
    { OT_PLG_BBALL,     0,                                     NULL         }, /* gimmickbreakoutball_3052 */
    { OT_PLG_BPLAT,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* gimmickbreakoutplatform_3050 (Q8 Q9) */
    { OT_PLG_BMASK,     0,                                     NULL         }, /* gimmickbreakoutplatformmask_3051 */
    { OT_PLG_LPLAT,     OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levellagoonplatform1_2994 (Q8 Q9 Q12 Q13) */
    { OT_PLG_LPLAT2,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levellagoonplatform2_3000 (Q8 Q9 Q12 Q13) */
    { OT_PLG_SWORD,     OD_HIDDEN,                             NULL         }, /* hazardgiantsword_3067 (Q20, ctor set_visible(0)) */
    { OT_PLG_SWMASK,    0,                                     NULL         }, /* hazardgiantswordmask_3068 */
    { OT_PLG_SPINBALL,  0,                                     NULL         }, /* gimmickspinball_1173 in Pangu Lagoon */
    { OT_PLG_CHAIN,     0,                                     NULL         }, /* gimmickspinballchain / 2 / 3 / 4 */
    { OT_PLG_PLSWITCH,  0,                                     NULL         }, /* gimmickhangingplswitch_3023 */
    { OT_PLG_SDOOR,     0,                                     NULL         }, /* objectstonedoor1 / 2 / 3 / 4 (drawn x-scaled by pl_gim_draw) */
    { OT_PLG_ACTIVE,    OD_OBSTACLE|OD_BOXCOL|OD_HIDDEN,       NULL         }, /* active_2927 (Q8, ctor set_visible(0) + initialize_active(1)) */
    { OT_PLG_REBOUND,   0,                                     NULL         }, /* - (runtime: EffectRebound / 2) */
    { OT_PLG_BURST,     0,                                     NULL         }, /* - (runtime: EffectsWhiteBurst_31) */
    /* ---- PL_BOSS (pl_boss.c) */
    { OT_PLB_NECK,      OD_HIDDEN,                             NULL         }, /* bossholodragonneck_3086 (the controller, invisible) */
    { OT_PLB_BREATH,    OD_HIDDEN,                             "plb_breath" }, /* bossholodragonbreath_3078 (ctor invisible) */
    { OT_PLB_TTOOTH,    0,                                     "plb_ttooth" }, /* bossholodragontoptooth_3080 */
    { OT_PLB_BTOOTH,    0,                                     "plb_btooth" }, /* bossholodragonbottomtooth_3081 */
    { OT_PLB_HSCALE,    0,                                     "plb_hscale" }, /* bossholodragonscale_3082 */
    { OT_PLB_SCALE2,    0,                                     "plb_s2"     }, /* bossholodragonscale2_3083 */
    { OT_PLB_SCALE3,    0,                                     "plb_s3"     }, /* bossholodragonscale3_3084 */
    { OT_PLB_SCALE4,    0,                                     "plb_s4"     }, /* bossholodragonscale4_3085 */
    { OT_PLB_EAR,       0,                                     "plb_ear"    }, /* bossholodragonear_3087 */
    { OT_PLB_EYE,       0,                                     "plb_eye"    }, /* bossholodragoneye_3088 */
    { OT_PLB_PUPIL,     0,                                     "plb_pupil"  }, /* bossholodragonpupil_3089 */
    { OT_PLB_BROW,      0,                                     "plb_brow"   }, /* bossholodragonbrow_3090 */
    { OT_PLB_HIT1,      0,                                     "plb_hit1"   }, /* bossholodragonhitspot1_3094 (Q20, A 3) */
    { OT_PLB_HIT2,      0,                                     "plb_hit2"   }, /* bossholodragonhitspot2_3095 (Q20, A 3) */
    { OT_PLB_HIT3,      0,                                     "plb_hit3"   }, /* bossholodragonhitspot3_3096 (Q20, A 3) */
    { OT_PLB_CLAW,      OD_HIDDEN,                             NULL         }, /* - (runtime: bossholodragonclaw_3093) */
    { OT_PLB_EYE2,      OD_HIDDEN,                             NULL         }, /* - (runtime: bossholodragoneye2_3097, Q20 A 20) */
    { OT_PLB_LASER,     OD_HIDDEN,                             NULL         }, /* - (runtime: bossholodragonlaser_3079) */
    { OT_PLB_FLAME,     0,                                     NULL         }, /* - (runtime: bossholodragonflame_3092) */
    { OT_PLB_PANGU,     0,                                     NULL         }, /* - (runtime: npcpangu_3098) */
    { OT_PLB_ARM,       0,                                     NULL         }, /* - (runtime: npcpanguleftarm / rightarm_3101 / 3102) */
    { OT_PLB_FX,        0,                                     NULL         }, /* - (runtime: the flash / backdrop) */
    /* ==== end Pangu Lagoon */
    /* ==== Battle Glacier (session 24): one block per module */
    /* ---- BG_FLOW (bg_flow.c) */
    { OT_BGF_LOOP1,     OD_SOLID|OD_LAYER1,                    "bgl1"       }, /* bgloop1_3340 (Q7 + Q11: solid on loop layer 1; visible Active) */
    { OT_BGF_LOOP2,     OD_SOLID|OD_LAYER0,                    "bgl2"       }, /* bgloop2_3343 (Q7 + Q10: solid on loop layer 0) */
    { OT_BGF_LOOP3,     OD_SOLID|OD_LAYER0,                    "bgl3"       }, /* bgloop3_3344 (Q7 + Q10) */
    /* ---- BG_ENEMIES (bg_enemy.c, spec_bg_enemies; sprites set by bg_enemy_spawn; RE on_frame_62..64_init q20: the drill) */
    { OT_BGE_DRILL,     OD_HIDDEN,                             NULL         }, /* enemydrillsat_3291 (q20; ctor invisible) */
    { OT_BGE_SPAWNER,   OD_HIDDEN,                             NULL         }, /* enemyelitespawner_3361 (ctor invisible) */
    { OT_BGE_WAVE,      0,                                     NULL         }, /* - (runtime: enemyelitespawnerwave_3363) */
    { OT_BGE_GHOST,     OD_HIDDEN,                             NULL         }, /* - (runtime: effectsspadeimage_456 dir 12, bg_enemy_draw) */
    { OT_BGE_DEB,       0,                                     NULL         }, /* - (runtime: effectsdebris_427 anim 5) */
    /* ---- BG_GIM (bg_gim.c; qualifiers RE on_frame_60..65_init; box = initialize_active(1) in the constructor) */
    { OT_BGG_TBMASK,    OD_SOLID|OD_HIDDEN,                    NULL         }, /* objectteleblockmask_3255 (Q7, ctor invisible; rotated by its A) */
    { OT_BGG_TBLOCK,    0,                                     NULL         }, /* - (runtime: objectteleblock_3254, the flying block) */
    { OT_BGG_SLIDE,     OD_HIDDEN,                             NULL         }, /* gimmickspiralslidestart_3268 / end_3269 (ctor invisible) */
    { OT_BGG_SNAKE,     0,                                     NULL         }, /* gimmicksnakedrill_3273 (no qualifier: scenery + rocks) */
    { OT_BGG_SPIRE,     0,                                     NULL         }, /* gimmicksnowspire_3249 (drawn at its mask) */
    { OT_BGG_SPMASK,    OD_SOLID|OD_HIDDEN,                    NULL         }, /* gimmicksnowspiremask_3251 (Q7, ctor invisible) */
    { OT_BGG_SOLAR,     0,                                     NULL         }, /* bgsolarpanel_3252 (scenery) */
    { OT_BGG_GPLAT1,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelglacierplatform1_3275 (Q8 Q9 Q12 Q13) */
    { OT_BGG_GPLAT2,    OD_OBSTACLE|OD_NOEDGE,                 NULL         }, /* levelglacierplatform2_3293 (Q8 Q9 Q12 Q13) */
    { OT_BGG_BAR,       0,                                     NULL         }, /* gimmickbgbar_3283 (drawn at its mask) */
    { OT_BGG_BARMASK,   OD_HANG|OD_BOXCOL|OD_HIDDEN,           NULL         }, /* gimmickbgbarmask_3284 (Q23, ctor invisible) */
    { OT_BGG_BGPLAT,    OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL,       NULL         }, /* bgplatform1_3313 (Q8 Q9) */
    { OT_BGG_FERRIS,    OD_OBSTACLE|OD_NOEDGE|OD_BOXCOL|OD_HIDDEN, NULL     }, /* levelcrankferris_3317 (Q8 Q9 Q13, ctor invisible) */
    { OT_BGG_FERRISV,   0,                                     NULL         }, /* levelcrankferrismask / 2 / 3 / 4 (sub 0-3: the drawn car and spokes) */
    { OT_BGG_FSTOP,     OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* levelcrankferriscollisionstopper_3324 (ctor invisible) */
    { OT_BGG_AGSPAWN,   OD_BOXCOL,                             NULL         }, /* gimmickantigravspawner_3323 */
    { OT_BGG_AG,        OD_BOXCOL,                             NULL         }, /* - (runtime: gimmickantigrav_3322) */
    { OT_BGG_MAGNET,    OD_BOXCOL,                             NULL         }, /* gimmickmagnetlauncher_3326 */
    { OT_BGG_MSTOP,     OD_BOXCOL|OD_HIDDEN,                   NULL         }, /* gimmickmagnetlauncherleft_3328 / right_3329 (sub 0-1, ctor invisible) */
    { OT_BGG_MLIGHT,    OD_BOXCOL,                             NULL         }, /* gimmickmagnetlauncherlight_3330 */
    { OT_BGG_SPINMINE,  0,                                     NULL         }, /* hazardspinmine_3331 (Q20, flag 5 armoured) */
    { OT_BGG_SWBLOCK,   OD_SOLID|OD_BOXCOL,                    NULL         }, /* objectswitchblock1_3354 / 2_3355 (sub 0-1, Q7; flag 0 = off) */
    { OT_BGG_TSWITCH,   0,                                     NULL         }, /* gimmickteleswitch_3356 */
    { OT_BGG_TSCRYSTAL, OD_BOXCOL,                             NULL         }, /* gimmickteleswitchcrystal1_3357 / 2_3358 (sub 0-1) */
    { OT_BGG_TRANSP,    OD_SOLID|OD_BOXCOL,                    NULL         }, /* objecttransporter_3360 (Q7) */
    { OT_BGG_SIREN,     OD_BOXCOL,                             NULL         }, /* objectsiren_3362 (scenery) */
    /* ---- BG_BOSS (bg_boss.c) */
    /* ==== end Battle Glacier */
    /* ==== Final Dreadnought (session 24): one block per module */
    /* ---- FD_FLOW (fd_flow.c) */
    /* ---- FD_ENEMIES (fd_enemy.c) */
    /* ---- FD_GIM (fd_gim.c) */
    /* ---- FD_BOSS (fd_boss.c) */
    /* ==== end Final Dreadnought */
};
static objdef_t BY_TYPE[MAX_OT];
static int built;
const objdef_t *objdef(int type) {
    if (!built) { for (unsigned i = 0; i < sizeof DEFS / sizeof DEFS[0]; i++) BY_TYPE[DEFS[i].type] = DEFS[i]; built = 1; }
    return (unsigned)type < MAX_OT ? &BY_TYPE[type] : &BY_TYPE[0];
}
