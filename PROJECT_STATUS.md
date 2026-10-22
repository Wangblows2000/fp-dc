# Project Status (2026-10-22)

**Objective:** native Sega Dreamcast port of Freedom Planet, built from data and behaviour recovered from the user's Linux build.

| Milestone (prompt v3) | State |
|---|---|
| Original playable + launch recorded | **Done**: headless, full window, real-menu route to DV1, frame jumps (`tools/orig/`) |
| Player logic mapped | **Done** for movement, jump, attacks, uppercut, down kick, cyclone, boost, crouch/lookup, anim selection (RUNTIME_MAP.md) |
| Comparison harness | **Done**: original trace + fpdc CSV + compare.py; **16/16 sequences pass** |
| Divergence fixes 1–6 | **Done** for traced sequences; damage, spring, boost directions, energy meter still to trace |
| Debug level select (FP_DEBUG_SELECT) | **Done**: verified frame indices/names; DV1–DV4 converted; Flycast-validated |
| Checkpoint 2 (front end, HUD, music) | Not started |
| Checkpoint 3 (objects, enemies, levels) | DV1 objects partly (Turretus, Dragonfly, crystals, petals, spikes, springs, sign) |

Labels: movement/attacks = original-trace vs host-test; level select/reload = emulator. Not yet tested on hardware.
