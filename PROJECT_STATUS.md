# Project Status (2026-10-22, session 3)

**Objective:** native Sega Dreamcast port of Freedom Planet, built from data and behaviour recovered from the user's Linux build.

| Area | State | Evidence label |
|---|---|---|
| Original playable + traced | Done: headless, menu route, frame jumps, tracer with sound + HUD-counter logging | — |
| Movement / move set | Done: incl. grounded cyclone, cyclone ↔ down-kick cancels, crouch kick, 6-direction boost + orientation, slope/air rotation, skid/turn rule | original-trace vs host-test |
| Energy / petals / lives / crystals | Done (meter starts empty, 0.4/f refill, 3/f drain, +5 crystal; no boost cooldown) | original-trace |
| Damage / invulnerability | Hurt done; invulnerability 60 f (upper bound 61, estimate) | original-trace (bound) |
| Springs | Done: vertical, diagonal, wall springs | original-trace vs host-test |
| HUD | Done from original graphics at RV positions (426×240, D-012) | host-test + emulator |
| Front end | Done: press start, title, file select, options, character select, Time Attack stage select, transitions, original strings/sounds/music | host-test + emulator |
| Stage card / pause / results | Done; results layout INFERRED | host-test + emulator |
| Music + sound library | Done: 67 BGM as ADPCM streams, 60 sfx/voices; catalogue of 68/1,373/475 | host-test + emulator |
| Comparisons | 16/16 baseline; 46/71 overall | original-trace vs host-test |
| Enemies beyond Turretus/Dragonfly, hang gimmicks, mini-boss, boss | Not done | — |
| Real hardware | **Not tested** | — |
