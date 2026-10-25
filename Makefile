# fpdc - Dreamcast build. Requires: source $(KOS_BASE)/environ.sh
#   make            -> fpdc.elf
#   make data       -> converted/ from the user's extracted game data
#   make cdi        -> build/fpdc.cdi (bootable MIL-CD image, data under /cd/data)
#   make host test  -> host build + gameplay regression tests
TARGET = fpdc.elf
OBJS = src/platform/plat_dc.o src/renderer/render_pvr.o src/resources/assets.o \
       src/physics/level.o src/physics/sensors.o src/characters/player.o src/game/objects.o src/game/game.o \
       src/game/ui.o src/game/frontend.o src/game/loading.o src/game/vmu.o src/game/fe_hub.o src/game/save.o src/game/loadprof.o src/resources/pack.o src/game/snap.o src/physics/fmath.o src/game/combat.o src/game/enemies.o src/game/gimmicks.o src/game/bosses.o src/game/serpentine.o src/game/results.o src/game/chase.o src/game/parallax.o src/game/shields.o src/game/water.o src/game/aqua.o src/game/aqua_gim.o src/game/aqua_boss.o src/game/monitors.o src/game/levels.o src/game/switches.o src/game/objdefs.o src/game/relic.o src/game/relic_gima.o src/game/relic_boss.o src/game/rm_mantalith.o
OBJS += src/game/relic_gimb.o   # Relic Maze acts 3-5 gimmicks
OBJS += src/game/fn_gima.o   # Fortune Night acts 1-3 gimmicks
OBJS += src/game/fn_gimb.o   # Fortune Night acts 4-5 gimmicks
OBJS += src/game/rm_claw.o   # RM3 Claw sequence
OBJS += src/game/fn_enemy.o   # Fortune Night enemies

FP_GAME ?= /mnt/user-data/uploads/Freedom Planet
# BGM: music tracks put on the disc (default: the whole ADPCM library). Lean disc with only the
# tracks the game plays now: make cdi BGM="$(addprefix converted2/bgm/,m_title.adp m_menu1.adp m_stage1.adp m_stage1b.adp m_clear.adp m_results.adp)" CDI=build/fpdc_lean.cdi
BGM ?= $(DATA_DIR)/bgm/*.adp
CDI ?= build/fpdc.cdi
MKDCDISC ?= third_party/mkdcdisc/builddir/mkdcdisc

all: $(TARGET)

ifdef KOS_BASE
include $(KOS_BASE)/Makefile.rules
KOS_CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -O2 -DFP_PROFILE
# header dependencies (.d files): a changed fp.h / ui.h / .inc rebuilds every object that includes it.
# Without this, objects compiled against an older game_t layout were linked into the disc (session 6 regression).
KOS_CFLAGS += -MMD -MP
# make FP_DEBUG_SELECT=1 -> (kept for compatibility, no effect) the developer level select is reached only by
#                          pressing L and R within 2 s on the title screen (src/game/frontend.c)
ifdef FP_DEBUG_SELECT
KOS_CFLAGS += -DFP_DEBUG_SELECT
endif
ifdef FP_PERF_BASELINE
KOS_CFLAGS += -DFP_PERF_BASELINE   # A/B perf measurement only: old full-scan sensor queries (touch src/game/objects.c)
endif
ifdef FP_DEBUG_ACTJUMP
KOS_CFLAGS += -DFP_DEBUG_ACTJUMP
endif
endif

-include $(OBJS:.o=.d)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

extracted/assets:
	python3 third_party/fp-assets/fp-assets.py -x -f "$(FP_GAME)/Assets.dat" extracted/assets

data: extracted/assets
	python3 tools/make_test_level.py
	python3 tools/convert.py extracted/assets "$(FP_GAME)/Data/BGM" converted

DATA_DIR ?= converted2
build/disc/data: $(DATA_DIR)/levels.txt
	rm -rf build/disc && mkdir -p build/disc/data
	cp $(DATA_DIR)/levels.txt build/disc/data/      # (the old root sfx_*.wav / stage1.pcm are unused: not on the disc)
	for d in $$(cat $(DATA_DIR)/levels.txt); do mkdir -p build/disc/data/$$d; \
	  cp $(DATA_DIR)/$$d/*.tex $(DATA_DIR)/$$d/*.spr $(DATA_DIR)/$$d/*.lv2 $(DATA_DIR)/$$d/*.pal build/disc/data/$$d/; \
	  cp $(DATA_DIR)/$$d/*.bin build/disc/data/$$d/ 2>/dev/null || true; done   # *_par.bin: parallax pieces
	# front end (ui scenes + banks + font), ADPCM music library (.adp), ADPCM sfx/voices
	mkdir -p build/disc/data/ui build/disc/data/bgm build/disc/data/sfx
	cp $(DATA_DIR)/ui/*.ui $(DATA_DIR)/ui/*.spr $(DATA_DIR)/ui/*.tex $(DATA_DIR)/ui/*.bin build/disc/data/ui/
	cp $(BGM) build/disc/data/bgm/
	cp $(DATA_DIR)/sfx/*.wav build/disc/data/sfx/
	mkdir -p build/disc/data/gal; cp $(DATA_DIR)/gal/*.tex $(DATA_DIR)/gal/*.pal $(DATA_DIR)/gal/*.wav $(DATA_DIR)/gal/*.txt build/disc/data/gal/   # gallery (session 15): loose, on demand
	python3 tools/twiddle_disc.py build/disc/data
	python3 tools/pack_disc.py build/disc/data
	if [ -n "$(DEMO)" ]; then cp "$(DEMO)" build/disc/data/demo.txt; fi   # verification builds: scripted input (plat_dc demo_load)
	python3 tools/pad_disc.py build/disc/data

cdi: $(TARGET)
	rm -rf build/disc
	$(MAKE) build/disc/data
	$(MKDCDISC) -e $(TARGET) -D build/disc -n "FREEDOM PLANET DC TEST" -a "fpdc" \
	    -o $(CDI) --allow-overwrite --no-banner -N

host:
	$(MAKE) -C host

test: host
	python3 tests/test_gameplay.py converted2

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)
	$(MAKE) -C host clean

.PHONY: all data cdi host test clean
