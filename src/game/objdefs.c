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
    { OT_WR_BARRIER,    OD_SOLID,                              NULL         }, /* minibossbarrier */
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
    { OT_SPINLOG_MASK,  OD_OBSTACLE,                           "slogmask"   }, /* spinninglogmask */
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
    { OT_SPIRAL_FRONT,  0,                                     "spiralfront" }, /* spiraltunnelfront */
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
    { OT_HSENS,         OD_BOXCOL|OD_HANG,                     "hsens"      }, /* gimmickhangingmovingsensor */
    { OT_HBAR2,         0,                                     "hbar2"      }, /* gimmickhangingmovingbar2 */
    { OT_HSENS2,        OD_BOXCOL|OD_HANG,                     "hsens2"     }, /* gimmickhangingmovingsensor2 */
    { OT_HISWITCH,      OD_BOXCOL|OD_HANG,                     "hiswitch"   }, /* gimmickhangingidleswitch */
    { OT_HSWITCHV,      0,                                     "hswitch"    }, /* gimmickhangingswitch */
    { OT_HIMOVER,       OD_BOXCOL|OD_HANG,                     "himover"    }, /* gimmickhangingidlemover */
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
    { OT_COLFRONT,      OD_SOLID|OD_BOXCOL,                    "colfront"   }, /* switchcolumnfront1 */
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
};
static objdef_t BY_TYPE[MAX_OT];
static int built;
const objdef_t *objdef(int type) {
    if (!built) { for (unsigned i = 0; i < sizeof DEFS / sizeof DEFS[0]; i++) BY_TYPE[DEFS[i].type] = DEFS[i]; built = 1; }
    return (unsigned)type < MAX_OT ? &BY_TYPE[type] : &BY_TYPE[0];
}
