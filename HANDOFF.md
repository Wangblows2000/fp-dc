# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Current focus: a faithful Dragon Valley slice with Lilac.

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0, kos-chain stable, `makejobs=2`).
  **environ.sh patch required** (KOS env ordering bug): `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before sourcing environ_base.sh.
- mkdcdisc at third_party/mkdcdisc/builddir/mkdcdisc; Flycast at third_party/flycast/build/flycast (Xvfb + `LIBGL_ALWAYS_SOFTWARE=1`; for input: `xdotool windowfocus --sync` + click, keys x=A c=B s=X arrows).
- Original game runnable at /home/claude/fprun (`run_fp.sh`, gdb scripts `jump.gdb` → `set_frame(20)`), silent placeholder audio.
- User PC via device bridge: `C:\Users\wobbles\Desktop\Freedom Planet` (Windows install) and `...\game` (Linux build). Staged read-only under `/mnt/user-data/uploads/Freedom Planet/`.

## Pipeline (all reproducible)
```sh
B="/mnt/user-data/uploads/Freedom Planet/game/bin64/Chowdren"
python3 third_party/fp-assets/fp-assets.py -x -f ".../game/Assets.dat" -m tools/fmt_linux_9d00124a.json extracted/linux
python3 tools/re/dump_frames.py "$B" build/frames          # 87 frames
python3 tools/re/dump_anims.py  "$B" build/frames/anims.json
python3 tools/re/dump_backdrops.py "$B" build/frames/backdrops.json
python3 tools/convert.py extracted/assets ".../Data/BGM" converted   # legacy: provides sfx_*.wav + stage1.pcm
sh tools/build_level.sh 21 dv1 converted2                # level + PAL8 palettes
make -C host && python3 tests/test_gameplay.py converted2
source /opt/toolchains/dc/kos/environ.sh && make && make cdi    # build/fpdc.cdi
```

## Latest verified
14/14 host tests; DC build 0 warnings; Flycast renders DV1, pad input works, perf update 70 µs/frame.

## Next three actions
1. Make the original game playable in the VM (menus via xdotool or globals) → gdb per-frame trace of Lilac → fit physics constants → trace-comparison tests.
2. Sprite rotation by ground angle (PVR rotated quads; host soft renderer too).
3. Springs/platform/corkscrew/layer-switch behaviours; then frames 22–23.

## Known issues
Animation labels by eye; physics placeholders; parallax factors guessed; checkpoint and boss not implemented; music loops the whole track; 4:3 crop of 16:9.

## Safety
Never write to the user's folders. extracted/, converted*/, build/ are regenerable and gitignored. Never commit game data.
