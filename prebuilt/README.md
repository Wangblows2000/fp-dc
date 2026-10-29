# prebuilt/: the project's ready-made programs and tools

The drag-and-drop builders (`build_dreamcast.bat` / `.sh` and, on the `pc-port` branch, `build_win32.bat` / `build_linux.sh`)
use these files, so that building needs no compiler. Each file is checked against the SHA-256 recorded in
`tools/builder/downloads.json` (and `downloads_pc.json` on the PC branch) before use. No game data is here: the builders
make that from your own copy of Freedom Planet.

| file | what | made from | licence |
|---|---|---|---|
| `fpdc.elf` | the Dreamcast program | this repository's `src/` (KallistiOS), stripped | the project's |
| `wav2adpcm.exe`, `wav2adpcm-linux-x86_64` | WAV to AICA ADPCM | `third_party/wav2adpcm/wav2adpcm.c` | `LICENSE-wav2adpcm-KOS.txt` |
| `mkdcdisc.exe`, `mkdcdisc-linux-x86_64` | the CDI writer | `mkdcdisc-2b98b0d9-src.tar.gz` (+ `libisofs-1.5.6.pl01.tar.gz` for Linux), static | `LICENSE-mkdcdisc-MIT.txt`, `LICENSE-libisofs-GPL-2.0.txt`, `LICENSE-libedc-CDDL-1.0.txt`, `LICENSE-elf_parser-MIT.txt`, `mkdcdisc-THIRD-PARTY-NOTICES.md` |
| `ffmpeg-6.1.1-linux-x86_64` | audio decoding (Linux; Windows uses the gyan.dev build, downloaded) | `ffmpeg-6.1.1.tar.xz`, minimal static configuration | `LICENSE-ffmpeg-LGPL-2.1.txt` |
| PC branch: `fpdc-win32.exe`, `SDL2.dll`, `SDL2-LICENSE.txt`, `fpdc-linux-x86_64` | the PC programs | `src/` + `pc/` (mingw-w64 i686; zig cc with static SDL 2.30.11) | the project's; SDL2: zlib |

The source archives of the GPL / LGPL tools are kept next to their programs, as their licences ask.
`SHA256SUMS` lists every file.

Maintainers: after a change of `src/`, rebuild the program(s) and commit them with the `downloads*.json` entries:
`tools/builder/build_release.sh --program-only` (fpdc.elf) and, on the PC branch, `tools/builder/build_release_pc.sh`.
`tools/builder/fpbuild.py selftest` fails while a program here is older than `src/` (its recorded `src_tree`).
Without arguments `build_release.sh` also rebuilds the pinned tools. Decision: DECISIONS.md D-119.
