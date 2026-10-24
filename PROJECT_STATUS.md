# Project Status (2026-10-23, session 4)

**Objective:** native Sega Dreamcast port of Freedom Planet, built from data and behaviour recovered from the user's Linux build.

| Area | State | Evidence label |
|---|---|---|
| Original playable + traced | Done: headless, menu route, frame jumps, tracer with sound + HUD-counter logging | — |
| Movement / move set | Done: incl. grounded cyclone, cyclone ↔ down-kick cancels, crouch kick, 6-direction boost + orientation, slope/air rotation, skid/turn rule | original-trace vs host-test |
| Energy / petals / lives / crystals | Done (meter starts empty, 0.4/f refill, 3/f drain, +5 crystal; no boost cooldown) | original-trace |
| Damage / invulnerability | Recovered: event 2414 formula, difficulty multiplier, 60-frame invulnerability | RE + original-trace |
| Springs | Done: vertical, diagonal, wall springs | original-trace vs host-test |
| HUD | Done from original graphics at RV positions (426×240, D-012) | host-test + emulator |
| Front end | Done: press start, title, file select, options, character select, Time Attack stage select, transitions, original strings/sounds/music | host-test + emulator |
| Stage card / pause / results | Done; results layout INFERRED (original sequence located, events 4560–4589) | host-test + emulator |
| Music + sound library | Done: 67 BGM as ADPCM streams, 60 sfx/voices; catalogue of 68/1,373/475 | host-test + emulator |
| Comparisons | 16/16 baseline; 57/70 overall; strict 24.1% exact frames | original-trace vs host-test |
| Turretus + pellet | Recovered (firing, aim, pellet physics, damage); hit-by-attack still RV-tuned | RE + original-trace |
| Camera | Recovered (MasterCamera_408) | RE + original-trace |
| Handlebar | Grab and hold recovered; release INFERRED | RE + original-trace |
| Sensor/collision system | Not ported (main remaining physics gap) | — |
| Other enemies, mini-boss, boss | Not done | — |
| Real hardware | **Not tested** | — |

## Session 12 (2026-10-24): Aqua Tunnel
| Area | State | Evidence label |
|---|---|---|
| Aqua Tunnel acts 1-4 (Milla only; Classic / Continue / Time Attack) | Done: flow, parallax, water, music, checkpoints, act transitions, results + raft exit | RE + original-trace (cmp_at 21/35 exact) + host-test |
| AT enemies and gimmicks (A and B specs) | Done | RE + original-trace |
| Iron Worm, Chomper, Chomper Snake | Done; worm and Chomper cycles match the original exactly; defeats RE-only | RE + original-trace |
| Milla monitor lifting / throwing | Done (all stages; head bump in AT) | RE + original-trace |
| Real hardware | **Not tested** (Flycast only) | — |
