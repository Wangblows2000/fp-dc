# Project Status (2026-10-22)

**Objective:** native Sega Dreamcast port of Freedom Planet. Current scope: a playable Dragon Valley slice with Lilac, built from data recovered from the user's Linux build.

| Milestone | State |
|---|---|
| M0 Reconnaissance | Done (Windows v1.21.5 + Linux build; see RESEARCH_LOG, PROJECT_AUDIT) |
| M1 Toolchain proof | **Done**: KOS 2.3.0 / GCC 15.2 builds, CDI boots in Flycast |
| M2 Resource extraction | **Done**: both Assets.dat variants extracted (fp-assets + custom format for Linux) |
| M3 Level-format proof | **Done**: all 87 frames' placements recovered; Dragon Valley frame 21 imported |
| M4 Rendering proof | **Done**: recovered pieces/parallax/sprites on host and DC (PAL8, 4.75 MB VRAM) |
| M5 Movement prototype | **Partly**: 360° ground modes on original collision masks; constants still UNVERIFIED |
| M6 Playable slice | **Done (emulator-validated)**: DV1 playable in Flycast with pad input; Turretus/Dragonfly/crystals/petals/spikes/springs/sign |
| M7 Original data integration | **In progress**: level + anims + art are original; behaviour and physics reconstructed |
| M8–M10 | Not started |

**Latest verified:** host 14/14 tests pass; `make` (0 warnings) and `make cdi` work; Flycast boot shows the recovered level; pad input moves Lilac; update 70 µs/frame.

**Biggest remaining gaps:** physics constants (needs measurement on the original), sprite rotation, object behaviours (springs, platforms, corkscrews, layer switches), parallax/camera rules, real-hardware test.
