# fpdc - Dreamcast build. Requires: source $(KOS_BASE)/environ.sh
#   make            -> fpdc.elf
#   make data       -> converted/ from the user's extracted game data
#   make cdi        -> build/fpdc.cdi (bootable MIL-CD image, data under /cd/data)
#   make host test  -> host build + gameplay regression tests
TARGET = fpdc.elf
OBJS = src/platform/plat_dc.o src/renderer/render_pvr.o src/resources/assets.o \
       src/physics/level.o src/characters/player.o src/game/objects.o src/game/game.o

FP_GAME ?= /mnt/user-data/uploads/Freedom Planet
MKDCDISC ?= third_party/mkdcdisc/builddir/mkdcdisc

all: $(TARGET)

ifdef KOS_BASE
include $(KOS_BASE)/Makefile.rules
KOS_CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -O2 -DFP_PROFILE
endif

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

extracted/assets:
	python3 third_party/fp-assets/fp-assets.py -x -f "$(FP_GAME)/Assets.dat" extracted/assets

data: extracted/assets
	python3 tools/make_test_level.py
	python3 tools/convert.py extracted/assets "$(FP_GAME)/Data/BGM" converted

DATA_DIR ?= converted2
build/disc/data: $(DATA_DIR)/dv1.lv2
	rm -rf build/disc && mkdir -p build/disc/data
	cp $(DATA_DIR)/*.tex $(DATA_DIR)/*.spr $(DATA_DIR)/*.lv2 $(DATA_DIR)/*.pal $(DATA_DIR)/*.wav $(DATA_DIR)/*.pcm build/disc/data/

cdi: $(TARGET)
	rm -rf build/disc
	$(MAKE) build/disc/data
	$(MKDCDISC) -e $(TARGET) -D build/disc -n "FREEDOM PLANET DC TEST" -a "fpdc" \
	    -o build/fpdc.cdi --allow-overwrite --no-banner -N

host:
	$(MAKE) -C host

test: host
	python3 tests/test_gameplay.py converted2

clean:
	rm -f $(OBJS) $(TARGET)
	$(MAKE) -C host clean

.PHONY: all data cdi host test clean
