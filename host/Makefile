# Host build: headless test runner using the software reference renderer.
SRC = ../src/platform/plat_host.c ../src/platform/host_bot.c ../src/platform/host_search.c ../src/renderer/render_soft.c ../src/resources/assets.c \
      ../src/physics/level.c ../src/physics/sensors.c ../src/characters/player.c ../src/game/objects.c ../src/game/game.c ../src/game/ui.c ../src/game/frontend.c ../src/game/loading.c ../src/game/vmu.c ../src/game/fe_hub.c ../src/game/save.c ../src/game/loadprof.c ../src/resources/pack.c ../src/game/snap.c ../src/physics/fmath.c
CFLAGS = -O2 -g -std=c99 -DFP_HOST_ENV -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
HDRS = $(wildcard ../src/*.h ../src/*/*.h ../src/*/*.inc)
fpdc_host: $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm
clean:
	rm -f fpdc_host
