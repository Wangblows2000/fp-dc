# Host build: headless test runner using the software reference renderer.
SRC = ../src/platform/plat_host.c ../src/platform/host_bot.c ../src/platform/host_search.c ../src/renderer/render_soft.c ../src/resources/assets.c \
      ../src/physics/level.c ../src/physics/sensors.c ../src/characters/player.c ../src/game/objects.c ../src/game/game.c ../src/game/ui.c ../src/game/frontend.c ../src/game/loading.c ../src/game/vmu.c ../src/game/fe_hub.c ../src/game/save.c ../src/game/loadprof.c ../src/resources/pack.c ../src/game/snap.c ../src/physics/fmath.c ../src/game/combat.c ../src/game/enemies.c ../src/game/gimmicks.c ../src/game/bosses.c ../src/game/serpentine.c ../src/game/results.c ../src/game/chase.c ../src/game/parallax.c ../src/game/shields.c ../src/game/water.c ../src/game/aqua.c ../src/game/aqua_gim.c ../src/game/aqua_boss.c ../src/game/monitors.c ../src/game/levels.c ../src/game/switches.c ../src/game/objdefs.c ../src/game/relic.c ../src/game/relic_gima.c ../src/game/relic_boss.c ../src/game/rm_mantalith.c
SRC += ../src/game/relic_gimb.c   # Relic Maze acts 3-5 gimmicks
SRC += ../src/game/fn_gima.c   # Fortune Night acts 1-3 gimmicks
SRC += ../src/game/fn_gimb.c   # Fortune Night acts 4-5 gimmicks
SRC += ../src/game/rm_claw.c   # RM3 Claw sequence
SRC += ../src/game/fn_flow.c   # Fortune Night stage flow
SRC += ../src/game/sb_flow.c   # Sky Battalion stage flow (session 19)
SRC += ../src/game/fn_enemy.c   # Fortune Night enemies
SRC += ../src/game/sb_enemy.c   # Sky Battalion enemies
SRC += ../src/game/sb_gim.c     # Sky Battalion gimmicks
SRC += ../src/game/tb_flow.c ../src/game/tb_enemy.c ../src/game/tb_gim.c ../src/game/tb_boss.c   # Thermal Base (session 21c)
SRC += ../src/game/tb_boss_shrimp.c ../src/game/tb_boss_pusher.c   # Thermal Base bosses (tb_boss.h encounters)
SRC += ../src/game/th_flow.c ../src/game/th_enemy.c ../src/game/th_gim.c ../src/game/th_boss.c   # Trap Hideout (session 21b)
SRC += ../src/game/th_boss_iris.c ../src/game/th_boss_basher.c ../src/game/th_boss_arena.c   # Trap Hideout bosses (th_boss.h encounters)
SRC += ../src/game/jc_flow.c ../src/game/jc_enemy.c ../src/game/jc_gim.c ../src/game/jc_boss.c ../src/game/jc_boss5.c   # Jade Creek (session 21)
SRC += ../src/game/fn_panther.c   # Fortune Night Panther (FN5 chase, FN6 boss)
SRC += ../src/game/fn_assist.c    # FN6 partner assist and the plane (session 18)
SRC += ../src/game/fn_boss.c   # Fortune Night bosses
SRC += ../src/game/sb_boss.c ../src/game/sb_satellite.c ../src/game/sb_kujacker.c ../src/game/sb_chopper.c ../src/game/sb_spade.c ../src/game/sb_grappler.c   # Sky Battalion bosses (session 19)
CFLAGS = -O2 -g -std=c99 -DFP_HOST_ENV -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
HDRS = $(wildcard ../src/*.h ../src/*/*.h ../src/*/*.inc)
fpdc_host: $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm
clean:
	rm -f fpdc_host
