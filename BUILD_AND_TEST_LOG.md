# Build and Test Log

All entries are real command output from this environment (Ubuntu x86_64 cloud VM, 2 cores, 7.8 GB RAM).

## 2026-10-22 Session 1

### Inputs
- Staged read-only copy of `C:\Users\wobbles\Desktop\Freedom Planet` from the user's PC via the device bridge.
- sha256 values in `original/SHA256SUMS`.

### fp-assets extraction: PASS
`python3 third_party/fp-assets/fp-assets.py -x -f ".../Assets.dat" extracted/assets` took 34.9 s and exited 0.
Output: 17,162 PNG + meta, 475 ogg + meta, 190 shader files, 18 text files.

### Toolchain (kos-chain, stable profile)
- First attempt **FAILED**: `makejobs=-j2` produced `make -j-j2` → binutils `Error 2`. Root cause: the cfg expects a bare number.
- Fix: `makejobs=2`. Rebuilt in the background (`make` in `kos/utils/kos-chain`, log `/opt/toolchains/dc/chain-build.log`).
- objc/objc++ disabled to save time.
- Result: see the next entry.

### Converter: PASS
`python3 tools/convert.py extracted/assets ".../Data/BGM" converted`
```
bank lilac     40 frames  9 anims atlas 512x512
bank objs       8 frames  3 anims atlas 256x256
tex  sky/mount/forest 512x256, rock 256x256, log 256x32
7 sfx, stage1 music 15,797,076 bytes, lvl test1 200x30 tiles 24 objects
VRAM textures total: 1589248 bytes (1.52 MB of 8 MB)
```

### Host build: PASS
`make -C host` with gcc, `-Wall -Wextra`. The only warnings (misleading indentation) were fixed.

### Host gameplay tests (`python3 tests/test_gameplay.py converted`): 17/17 PASS
Bugs found and fixed by the tests or trace:
1. Landing on the 45° slope stalled the player (gsp 0.24). Cause: the airborne wall sensor at feet−4 hit the slope face. Fix: sensor at step height, plus a snap-up landing check during airborne horizontal sub-steps. Landing now keeps gsp 5.9.
2. The gentle down-slope was placed one row high, leaving a 16 px wall. Generator fixed.
3. Ground Dragon Boost ended after one frame (the snap-landing check fired at vy=0). Boost on the ground now uses ground movement.

Visual check: frames rendered by the software renderer (`build/run1/montage.png`) show correct parallax, terrain, slopes, logs, sprites, HUD and STAGE CLEAR.

### mkdcdisc: PASS (built from source, meson/ninja, v0.0.4)
### Flycast: cloned (fa9d39a), configured with CMake/Ninja (GL, no Vulkan)
