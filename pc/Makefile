# PC build (SDL2): the unchanged game code + src/platform/plat_sdl.c + the software renderer (render_soft.c).
#   make -C pc                      -> pc/build/linux/fpdc        (gcc, system SDL2: sdl2-config)
#   make -C pc PLATFORM=win32 SDL2_DIR=<SDL2-x.y.z/i686-w64-mingw32> [WIN_CC=<mingw-w64 gcc>]
#                                   -> pc/build/win32/fpdc.exe    (mingw-w64 i686; pc/compile_win32.sh fetches SDL2)
#   OUT=<dir> puts the objects and the binary elsewhere. The wrappers pc/compile_linux.sh / pc/compile_win32.sh call this.
PLATFORM ?= linux
ROOT := ..
OUT ?= build/$(PLATFORM)

# every game source (as host/Makefile, without the host-only bot / search and the Dreamcast backend)
GAME_SRC := $(sort $(wildcard $(ROOT)/src/characters/*.c $(ROOT)/src/game/*.c $(ROOT)/src/physics/*.c $(ROOT)/src/resources/*.c))
SRC := $(GAME_SRC) $(ROOT)/src/renderer/render_soft.c $(ROOT)/src/platform/plat_sdl.c
OBJ := $(patsubst $(ROOT)/src/%.c,$(OUT)/obj/%.o,$(SRC))

# FP_PC: render_soft.c presents through plat_sdl.c. No FP_HOST_ENV (host test hooks stay out of the PC build).
# -ffp-contract=off as the host build (no FMA contraction: same float results as the host tests)
# CFLAGS / LDFLAGS (default -O2) may be given on the command line; the required flags are in FP_CFLAGS / FP_LDFLAGS
CFLAGS ?= -O2
FP_CFLAGS := -std=c99 -DFP_PC -Wall -Wextra -Wno-unused-parameter -ffp-contract=off -MMD -MP

ifeq ($(PLATFORM),win32)
WIN_CC ?= i686-w64-mingw32-gcc
CC := $(WIN_CC)
EXE := fpdc.exe
ifndef SDL2_DIR
$(error PLATFORM=win32 needs SDL2_DIR=<SDL2-devel-mingw>/i686-w64-mingw32 (pc/compile_win32.sh downloads it))
endif
# SSE2 maths (no x87 excess precision: the physics matches the x86_64 / host results); __USE_MINGW_ANSI_STDIO: C99 printf
FP_CFLAGS += -msse2 -mfpmath=sse -D__USE_MINGW_ANSI_STDIO=1 -I$(SDL2_DIR)/include/SDL2 -Dmain=SDL_main
FP_LDFLAGS := -static-libgcc -mwindows
LIBS := -L$(SDL2_DIR)/lib -lmingw32 -lSDL2main -lSDL2 -lm
else
CC ?= gcc
EXE := fpdc
SDL2_CONFIG ?= sdl2-config
FP_CFLAGS += -D_POSIX_C_SOURCE=200809L $(shell $(SDL2_CONFIG) --cflags)
LIBS := $(shell $(SDL2_CONFIG) --libs) -lm
endif

all: $(OUT)/$(EXE)

$(OUT)/$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(FP_LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(OUT)/obj/%.o: $(ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FP_CFLAGS) -c -o $@ $<

-include $(OBJ:.o=.d)

clean:
	rm -rf build

.PHONY: all clean
