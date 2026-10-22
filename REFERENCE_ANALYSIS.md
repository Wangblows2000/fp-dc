# Reference Analysis

## What I could actually inspect
| Reference | Access result |
|---|---|
| Primary: https://youtu.be/7yqo3ffqInA?t=34 (Lilac Walkthrough, Stage 1 Dragon Valley) | **Not inspected.** The fetch failed (client_error). There's no video-frame access in this environment. |
| Secondary: https://www.youtube.com/watch?v=HVZCLis_jgw | **Metadata only:** "Freedom Planet - Full Playthrough as Lilac (Normal Mode, No Deaths)", Galaxia Shadow, 2023-08-27. Chapters: 1:20 Dragon Valley, 10:57 Relic Maze, 34:59 Fortune Night, 52:49 Sky Battalion, 1:09:52 Jade Creek, 1:21:39 Thermal Base, 1:40:44 Pangu Lagoon, 2:00:18 Battle Glacier, 2:24:49 / 2:28:51 / 2:36:39 / 2:42:06 Final Dreadnought 1–4, 2:45:24 Final Boss. **No frames viewed.** |
| SDA knowledge base, Lilac + Mechanics pages | Read (text). Used for move rules (see PROJECT_AUDIT §3). No numeric physics. |
| **The original game itself** (Linux build, run in this VM) | Boots under Xvfb/llvmpipe with silent placeholder audio; the intro renders and keyboard input works (title advanced frame 1→2). Jumping to frame 20 via `GameManager::set_frame` loads Dragon Valley geometry, but it doesn't progress (likely needs menu-set globals). |

**No timestamped observations are claimed.** None could be made.

## Planned measurement method (replaces video estimation)
Measuring the original binary directly is more precise than video:
1. Reach Dragon Valley through the real menus (xdotool), or set the character/stage globals before `set_frame(20)`.
2. Use a gdb breakpoint once per logic frame (`GameManager::update`) to read the player object's x/y (Active fields) and alterable values.
3. Scripted input sessions: standing start → hold right; jump (tap/hold); boost; cyclone.
4. Fit constants and compare traces with the port's host trace (same format), as automated tests.

## How to provide footage if wanted
Short clips or PNG frame sequences (attached to the chat) at 60 fps would let me record timestamped observations. The direct-measurement route above is still preferred for physics.
