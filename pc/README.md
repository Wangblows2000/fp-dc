# PC build (Linux x86_64, Windows i686)

The Dreamcast port's game code, unchanged, on a PC through SDL2. `src/platform/plat_sdl.c` implements the
platform API of `src/fp.h` (as `plat_dc.c` does with KallistiOS and `plat_host.c` for the headless tests);
rendering is the software reference renderer `src/renderer/render_soft.c` (the one the host tests' PPM dumps
come from), so a PC frame is pixel-identical to the host dump of the same frame.

| Part | How |
|---|---|
| Video | 426x240 software frame -> SDL streaming texture -> window. Scaling (F10): aspect fit with sharp scaling (nearest to the next integer multiple, then filtered), integer, stretch; F9 bilinear. Options > SCREEN SIZE 16:9 / 4:3 picks the aspect as on the DC. Alt+Enter / F11 fullscreen (desktop resolution). |
| Timing | fixed 60 Hz game step (accumulator; vsync or a sleep paces the loop; up to 3 catch-up steps per drawn frame). |
| Audio | software mixer (48 kHz stereo, 48 voices): sfx WAVs (AICA 4-bit ADPCM, decoded once at load; PCM too), stereo ADPCM music `.adp` streamed (32 kHz, byte-interleaved: low nibble right, high left; whole-file loop at `size & ~31`, once-tracks end in silence), `.pcm` legacy. Volume / pan as the KOS AICA driver (logs[] table, DIPAN 3 dB steps); looping channels (bike engine) with volume / pitch updates. If the default back end cannot open a device (e.g. WASAPI without an endpoint) the others are tried, down to `dummy`. |
| Input | keyboard and SDL GameController mapped onto the DC pad bits (see README.txt); `--script` adds a host input script. |
| Saves | the VMU save payload in `fpdc.sav` (user dir: `~/.local/share/fpdc/FreedomPlanetDC/`, `%APPDATA%\fpdc\FreedomPlanetDC\`; or next to the program if a `fpdc.sav` is there; or `--save`). Same bytes as the host's `FP_SAVE` file. F3 shows the VMU LCD. |
| Data | first argument, else `data/` next to the program, else `./data`: the `converted2` layout the host reads (loose files; the disc's twiddled textures and packs are not used). |

Shared-code changes (all under `FP_PC`, which only the PC build defines; the host and Dreamcast builds compile
exactly what they did): `render_soft.c` `r_end()` hands the frame to `pc_present()`, and `r_sprite()` has a fast
path for the common 1:1 / opaque / white case (no per-pixel float maths or blend; ~3x faster, same pixels: 870
frames of 29 levels compared identical with the host dumps); `loadprof.c` skips the Dreamcast heap / VRAM figures.
No `FP_HOST_ENV` in the PC build (the host test hooks stay out).

Frame time (this 2-core VM, shared with other jobs, `--fast`, dummy video / audio, 1800 frames running right with
jumps and attacks): game update 0.2-0.4 ms, software draw 2.0-4.2 ms average (dv1 2.0, at2 2.9, pl2 2.5, fn3 2.8,
fd2a 2.3 in the quieter pass), i.e. well inside the 16.7 ms of a 60 Hz frame; the present (upload + scale) is a
GPU copy on real hardware (1.8 ms with SDL's software renderer here, 4-6 ms with Mesa llvmpipe under Xvfb).

## Building

Linux (x86_64), native:

    apt-get install build-essential libsdl2-dev        # prerequisites (Debian / Ubuntu)
    pc/build_linux.sh [outdir]                          # -> outdir/fpdc, README.txt   (default pc/dist/linux)

Windows 32-bit (i686), cross-compiled on Linux:

    apt-get install mingw-w64 curl                      # prerequisites
    pc/build_win32.sh [outdir]                          # -> outdir/fpdc.exe, SDL2.dll, README.txt (default pc/dist/win32)

`build_win32.sh` downloads the official SDL2 mingw development package (SDL 2.30.11, SHA-256 checked) once into
`pc/build/deps`; `SDL2_DEVEL=<SDL2-2.x.y dir>` uses an unpacked one instead. On Windows with MSYS2
(`pacman -S --needed make tar curl mingw-w64-i686-gcc`): `pc\build_win32.bat [outdir]` runs the same script in
the MINGW32 environment (not tested on a Windows machine here).

Both scripts take `DATA_DIR=<converted2>` to copy the game data to `<outdir>/data`. `JOBS=n` sets make's `-j`.
Directly: `make -C pc` (Linux) or `make -C pc PLATFORM=win32 SDL2_DIR=<SDL2>/i686-w64-mingw32`; objects and
binaries go to `pc/build/<platform>` (never committed: `pc/build/`, `pc/dist/` are in .gitignore).

The i686 build uses `-msse2 -mfpmath=sse` (no x87 excess precision): its gameplay frames match the x86_64 /
host frames (DV1 route below). Title-screen rotations differ by a few edge pixels (msvcrt vs glibc sinf / cosf).

## Running

    fpdc [data_dir] [--fullscreen] [--window N] [--scale aspect|integer|stretch] [--smooth] [--no-vsync]
         [--no-audio] [--save file]
    # tests / benchmarks:
    fpdc data --level dv1 [--char n] --script route.txt --frames 1300 --fast --shot-dir shots --shot-every 20

`--shot-dir` writes the 426x240 frame of game frame N as `fNNNNN.bmp` (the host dumps `fNNNNN.ppm`). The host draws
only the dumped frames and a few front-end animations advance in the draw, so `--host-draw` draws only the
`--shot-every` frames too. The log goes to stderr and `fpdc.log` in the user dir (Windows builds have no
console); every 600 frames it prints the update / software draw / present times.

Smoke test against the host harness (classic route to DV1, 65 frames compared; builds host/fpdc_host if needed;
`fpdc.exe` runs through wine; uses xvfb-run without DISPLAY):

    pc/test_pc.sh pc/dist/linux/fpdc converted2
    pc/test_pc.sh pc/dist/win32/fpdc.exe converted2

## Tested (session of the PC port)

- Linux build under Xvfb (+ openbox for fullscreen): title, front end (classic route), character select, DV1;
  65 of 65 sampled frames identical to the host dumps; keyboard via xdotool (menus, run, F3 VMU, F11 / Alt+Enter
  fullscreen); saves byte-identical to the host's and read back (options music 90 %).
- Windows build under Wine 9.0 (+ Xvfb): the same route (gameplay frames identical to the host), keyboard,
  fullscreen, saves; Direct3D renderer; audio falls back WASAPI -> DirectSound -> winmm -> dummy without a sound
  device (no failure); with SDL_AUDIODRIVER=disk the recorded music matches a reference ADPCM decode (corr 1.000).
- Not tested on real hardware here: game controllers, real sound devices, a native Windows machine.
