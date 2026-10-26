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
    { OT_SB_BTUNNEL,    OD_HIDDEN,                             NULL         }, /* levelballtunnel2 / 3 (q27, invisible) */
};
static objdef_t BY_TYPE[MAX_OT];
static int built;
const objdef_t *objdef(int type) {
    if (!built) { for (unsigned i = 0; i < sizeof DEFS / sizeof DEFS[0]; i++) BY_TYPE[DEFS[i].type] = DEFS[i]; built = 1; }
    return (unsigned)type < MAX_OT ? &BY_TYPE[type] : &BY_TYPE[0];
}
